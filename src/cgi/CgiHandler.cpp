#include "CgiHandler.hpp"

CgiHandler::CgiHandler(Server& _server) : server(_server) {}

CgiHandler::~CgiHandler() {
	for (auto& pair : tunnels) {
		cleanup_tunnel(pair.second);
	}
}

void CgiHandler::addCgiTunnel(RequestState &req, const std::string &method, const std::string &query) {

	Logger::file("addCgiTunnel");
	Logger::file("Method: " + method + ", Content Type: ");
	Logger::file("Request Body: " + std::string(req.request_buffer.begin(), req.request_buffer.end()));
	Logger::file("Query: " + query);
	int pipe_in[2] = {-1, -1};
	int pipe_out[2] = {-1, -1};

	if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) {
		cleanup_pipes(pipe_in, pipe_out);
		return;
	}

	if (fcntl(pipe_in[0], F_SETFL, O_NONBLOCK) < 0 ||
		fcntl(pipe_in[1], F_SETFL, O_NONBLOCK) < 0 ||
		fcntl(pipe_out[0], F_SETFL, O_NONBLOCK) < 0 ||
		fcntl(pipe_out[1], F_SETFL, O_NONBLOCK) < 0) {
		cleanup_pipes(pipe_in, pipe_out);
		return;
	}

	CgiTunnel tunnel;
	tunnel.in_fd = pipe_in[1];
	tunnel.out_fd = pipe_out[0];
	tunnel.server_name = req.associated_conf->name;
	tunnel.client_fd = req.client_fd;
	tunnel.server_fd = req.associated_conf->server_fd;
	tunnel.config = req.associated_conf;
	tunnel.location = server.getRequestHandler()->findMatchingLocation(req.associated_conf, req.location_path);
	tunnel.is_busy = true;
	tunnel.last_used = std::chrono::steady_clock::now();
	tunnel.pid = -1;
	tunnel.script_path = req.requested_path;
	pid_t pid = fork();
	if (pid < 0) {
		cleanup_pipes(pipe_in, pipe_out);
		return;
	}

	if (pid == 0) {
		close(pipe_in[1]);
		close(pipe_out[0]);

		if (dup2(pipe_in[0], STDIN_FILENO) < 0) {
			_exit(1);
		}
		if (dup2(pipe_out[1], STDOUT_FILENO) < 0) {
			_exit(1);
		}

		close(pipe_in[0]);
		close(pipe_out[1]);

		setup_cgi_environment(tunnel, method, query);
		execute_cgi(tunnel);
		_exit(1);
	}

	close(pipe_in[0]);
	close(pipe_out[1]);

	server.modEpoll(server.getGlobalFds().epoll_fd, tunnel.in_fd, EPOLLOUT);
	server.modEpoll(server.getGlobalFds().epoll_fd, tunnel.out_fd, EPOLLIN);

	server.getGlobalFds().svFD_to_clFD_map[tunnel.in_fd] = req.client_fd;
	server.getGlobalFds().svFD_to_clFD_map[tunnel.out_fd] = req.client_fd;

	tunnels[tunnel.in_fd] = tunnel;
	tunnels[tunnel.out_fd] = tunnel;
	fd_to_tunnel[tunnel.in_fd] = &tunnels[tunnel.in_fd];
	fd_to_tunnel[tunnel.out_fd] = &tunnels[tunnel.out_fd];

	tunnels[tunnel.in_fd].pid = pid;
	tunnels[tunnel.out_fd].pid = pid;

	req.cgi_in_fd = tunnel.in_fd;
	req.cgi_out_fd = tunnel.out_fd;
	req.cgi_pid = pid;
	req.state = RequestState::STATE_CGI_RUNNING;
	req.cgi_done = false;
}

void CgiHandler::cleanupCGI(RequestState &req) {

	if (req.cgi_pid > 0) {
		int status;
		pid_t wait_result = waitpid(req.cgi_pid, &status, WNOHANG);

		if (wait_result == 0) {
			kill(req.cgi_pid, SIGTERM);
			usleep(100000);
			wait_result = waitpid(req.cgi_pid, &status, WNOHANG);

			if (wait_result == 0) {
				kill(req.cgi_pid, SIGKILL);
				waitpid(req.cgi_pid, &status, 0);
			}
		}
		req.cgi_pid = -1;
	}

	if (req.cgi_in_fd != -1) {
		epoll_ctl(server.getGlobalFds().epoll_fd, EPOLL_CTL_DEL, req.cgi_in_fd, NULL);
		close(req.cgi_in_fd);
		server.getGlobalFds().svFD_to_clFD_map.erase(req.cgi_in_fd);
		req.cgi_in_fd = -1;
	}

	if (req.cgi_out_fd != -1) {
		epoll_ctl(server.getGlobalFds().epoll_fd, EPOLL_CTL_DEL, req.cgi_out_fd, NULL);
		close(req.cgi_out_fd);
		server.getGlobalFds().svFD_to_clFD_map.erase(req.cgi_out_fd);
		req.cgi_out_fd = -1;
	}
}

bool CgiHandler::needsCGI(RequestState &req, const std::string &path)
{
	struct stat file_stat;
	if (stat(req.requested_path.c_str(), &file_stat) != 0) {
		return false;
	}

	if (S_ISDIR(file_stat.st_mode)) {
		req.is_directory = true;
		return false;
	}

	const Location* loc = server.getRequestHandler()->findMatchingLocation(req.associated_conf, path);
	if (!loc || loc->cgi.empty()) {
		return false;
	}

	size_t dot_pos = req.requested_path.find_last_of('.');
	if (dot_pos != std::string::npos) {
		std::string extension = req.requested_path.substr(dot_pos);
		if (extension == loc->cgi_filetype) {
			return true;
		}
	}
	return false;
}

void CgiHandler::handleCGIWrite(int epfd, int fd, uint32_t events) {
	if (events & (EPOLLERR | EPOLLHUP)) {
		auto tunnel_it = fd_to_tunnel.find(fd);
		if (tunnel_it != fd_to_tunnel.end() && tunnel_it->second) {
			cleanup_tunnel(*(tunnel_it->second));
		}
		return;
	}

	auto tunnel_it = fd_to_tunnel.find(fd);
	if (tunnel_it == fd_to_tunnel.end() || !tunnel_it->second) {
		return;
	}

	CgiTunnel* tunnel = tunnel_it->second;

	if (tunnel->pid > 0) {
		int status;
		pid_t result = waitpid(tunnel->pid, &status, WNOHANG);
		if (result > 0) {
			cleanup_tunnel(*tunnel);
			return;
		} else if (result < 0 && errno != ECHILD) {
			cleanup_tunnel(*tunnel);
			return;
		}
	}

	auto req_it = server.getGlobalFds().request_state_map.find(tunnel->client_fd);
	if (req_it == server.getGlobalFds().request_state_map.end()) {
		cleanup_tunnel(*tunnel);
		return;
	}

	RequestState &req = req_it->second;

	if (req.request_body.empty()) {
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
		close(fd);
		tunnel->in_fd = -1;
		server.getGlobalFds().svFD_to_clFD_map.erase(fd);
		fd_to_tunnel.erase(fd);
		return;
	}

	ssize_t written = write(fd, req.request_body.data(), req.request_body.size());

	if (written < 0) {
		if (errno == EPIPE) {
			cleanup_tunnel(*tunnel);
		} else if (errno != EAGAIN && errno != EWOULDBLOCK) {
			cleanup_tunnel(*tunnel);
		}
		return;
	}

	if (written > 0) {
		req.request_body.erase(req.request_body.begin(), req.request_body.begin() + written);

		if (req.request_body.empty()) {
			epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
			close(fd);
			tunnel->in_fd = -1;
			server.getGlobalFds().svFD_to_clFD_map.erase(fd);
			fd_to_tunnel.erase(fd);
		}
	}
}

void CgiHandler::handleCGIRead(int epfd, int fd) {
	Logger::file("=== CGI Read Debug Start ===");

	auto tunnel_it = fd_to_tunnel.find(fd);
	if (tunnel_it == fd_to_tunnel.end() || !tunnel_it->second) {
		Logger::file("Invalid tunnel");
		return;
	}

	CgiTunnel* tunnel = tunnel_it->second;
	auto req_it = server.getGlobalFds().request_state_map.find(tunnel->client_fd);
	if (req_it == server.getGlobalFds().request_state_map.end()) {
		Logger::file("Invalid request state");
		return;
	}
	RequestState &req = req_it->second;

	char buf[4096];
	Logger::file("Starting read loop");
	ssize_t n;
	while ((n = read(fd, buf, sizeof(buf))) > 0) {
		Logger::file("Read chunk: " + std::to_string(n) + " bytes");
		Logger::file("Content: " + std::string(buf, n));
		req.cgi_output_buffer.insert(req.cgi_output_buffer.end(), buf, buf + n);
	}
	Logger::file("Read loop ended with n=" + std::to_string(n));

	if (n == 0) {
		std::string output(req.cgi_output_buffer.begin(), req.cgi_output_buffer.end());
		Logger::file("Complete CGI output: " + output);

		size_t header_end = output.find("\r\n\r\n");
		if (header_end == std::string::npos) {
			Logger::file("No headers found");
			header_end = 0;
		}

		std::string cgi_headers = header_end > 0 ? output.substr(0, header_end) : "";
		std::string cgi_body = header_end > 0 ? output.substr(header_end + 4) : output;

		Logger::file("CGI Headers: " + cgi_headers);
		Logger::file("CGI Body length: " + std::to_string(cgi_body.length()));

		std::string response;
		if (!cgi_headers.empty() && cgi_headers.find("Location:") != std::string::npos) {
			Logger::file("Redirect detected, creating 302 response");
			response = "HTTP/1.1 302 Found\r\n" + cgi_headers + "\r\n\r\n";
		} else {
			Logger::file("Normal response");
			response = "HTTP/1.1 200 OK\r\n";
			response += "Content-Length: " + std::to_string(cgi_body.length()) + "\r\n";
			response += "Connection: close\r\n";

			if (!cgi_headers.empty()) {
				response += cgi_headers + "\r\n";
			} else {
				response += "Content-Type: text/html\r\n";
			}
			response += "\r\n" + cgi_body;
		}

		Logger::file("Final response headers: " + response.substr(0, response.find("\r\n\r\n")));
		req.response_buffer.assign(response.begin(), response.end());
		req.state = RequestState::STATE_SENDING_RESPONSE;
		server.modEpoll(epfd, tunnel->client_fd, EPOLLOUT);
		Logger::file("Response queued for sending");

		cleanup_tunnel(*tunnel);
	}
	else if (n < 0) {
		Logger::file("Read error: " + std::string(strerror(errno)));
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			cleanup_tunnel(*tunnel);
		}
	}

	Logger::file("=== CGI Read Debug End ===");
}

void CgiHandler::cleanup_tunnel(CgiTunnel &tunnel) {
	int in_fd = tunnel.in_fd;
	int out_fd = tunnel.out_fd;
	pid_t pid = tunnel.pid;

	if (in_fd != -1) {
		epoll_ctl(server.getGlobalFds().epoll_fd, EPOLL_CTL_DEL, in_fd, NULL);
		close(in_fd);
		server.getGlobalFds().svFD_to_clFD_map.erase(in_fd);
		fd_to_tunnel.erase(in_fd);
	}

	if (out_fd != -1) {
		epoll_ctl(server.getGlobalFds().epoll_fd, EPOLL_CTL_DEL, out_fd, NULL);
		close(out_fd);
		server.getGlobalFds().svFD_to_clFD_map.erase(out_fd);
		fd_to_tunnel.erase(out_fd);
	}

	if (pid > 0) {
		kill(pid, SIGTERM);
		usleep(100000);
		if (waitpid(pid, NULL, WNOHANG) == 0) {
			kill(pid, SIGKILL);
			waitpid(pid, NULL, 0);
		}
	}

	tunnels.erase(in_fd);
}

void CgiHandler::setup_cgi_environment(const CgiTunnel &tunnel, const std::string &method, const std::string &query) {
	clearenv();

	std::string content_length = "0";
	std::string content_type = "application/x-www-form-urlencoded";
	std::string http_cookie;

	auto req_it = server.getGlobalFds().request_state_map.find(tunnel.client_fd);
	if (req_it != server.getGlobalFds().request_state_map.end()) {
		const RequestState &req = req_it->second;
		http_cookie = req.cookie_header;
		if (method == "POST") {
			std::string request(req.request_buffer.begin(), req.request_buffer.end());
			size_t header_end = request.find("\r\n\r\n");
			if (header_end != std::string::npos && header_end + 4 < request.length()) {
				content_length = std::to_string(request.length() - (header_end + 4));
			}

			size_t content_type_pos = request.find("Content-Type: ");
			if (content_type_pos != std::string::npos) {
				size_t content_type_end = request.find("\r\n", content_type_pos);
				if (content_type_end != std::string::npos) {
					content_type = request.substr(content_type_pos + 14,
						content_type_end - (content_type_pos + 14));
					Logger::file("Found Content-Type: " + content_type);
				}
			}
		}
	}

	std::vector<std::string> env_vars = {
		"REDIRECT_STATUS=200",
		"GATEWAY_INTERFACE=CGI/1.1",
		"SERVER_PROTOCOL=HTTP/1.1",
		"REQUEST_METHOD=" + method,
		"QUERY_STRING=" + query,
		"SCRIPT_FILENAME=" + tunnel.script_path,
		"SCRIPT_NAME=" + tunnel.script_path,
		"DOCUMENT_ROOT=" + tunnel.config->root,
		"SERVER_SOFTWARE=webserv/1.0",
		"SERVER_NAME=" + tunnel.server_name,
		"SERVER_PORT=" + tunnel.config->port,
		"CONTENT_TYPE=" + content_type,
		"CONTENT_LENGTH=" + content_length,
		"HTTP_COOKIE=" + http_cookie,
		"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
	};

	if (method == "POST") {
		env_vars.push_back("HTTP_TRANSFER_ENCODING=chunked");
		if (content_type.find("boundary=") != std::string::npos) {
			env_vars.push_back("REQUEST_TYPE=multipart/form-data");
		}
	}

	for (const auto& env_var : env_vars) {
		Logger::file("  " + env_var);
		putenv(strdup(env_var.c_str()));
	}
}

void CgiHandler::execute_cgi(const CgiTunnel &tunnel) {

	if (access(tunnel.location->cgi.c_str(), X_OK) != 0) {
		_exit(1);
	}

	if (!tunnel.location) {
		_exit(1);
	}

	if (access(tunnel.location->cgi.c_str(), X_OK) != 0) {
		_exit(1);
	}

	if (access(tunnel.script_path.c_str(), R_OK) != 0) {
		_exit(1);
	}

	char* const args[] = {
		(char*)tunnel.location->cgi.c_str(),
		(char*)tunnel.script_path.c_str(),
		nullptr
	};

	execve(args[0], args, environ);
	_exit(1);
}

void CgiHandler::cleanup_pipes(int pipe_in[2], int pipe_out[2]) {
	if (pipe_in[0] != -1) close(pipe_in[0]);
	if (pipe_in[1] != -1) close(pipe_in[1]);
	if (pipe_out[0] != -1) close(pipe_out[0]);
	if (pipe_out[1] != -1) close(pipe_out[1]);
}