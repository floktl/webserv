/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiHandler.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/11 14:42:00 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/29 14:29:35 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "CgiHandler.hpp"

CgiHandler::CgiHandler(Server& _server) : server(_server) {}

CgiHandler::~CgiHandler() {
	for (auto& pair : tunnels) {
		cleanup_tunnel(pair.second);
	}
}

void CgiHandler::addCgiTunnel(RequestState &req, const std::string &method, const std::string &query) {
	std::stringstream ss;
	ss << "Creating new CGI tunnel for client_fd " << req.client_fd;
	Logger::file(ss.str());

	int pipe_in[2] = {-1, -1};
	int pipe_out[2] = {-1, -1};

	if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) {
		Logger::file("Error creating pipes: " + std::string(strerror(errno)));
		cleanup_pipes(pipe_in, pipe_out);
		return;
	}

	if (fcntl(pipe_in[0], F_SETFL, O_NONBLOCK) < 0 ||
		fcntl(pipe_in[1], F_SETFL, O_NONBLOCK) < 0 ||
		fcntl(pipe_out[0], F_SETFL, O_NONBLOCK) < 0 ||
		fcntl(pipe_out[1], F_SETFL, O_NONBLOCK) < 0) {
		Logger::file("Failed to set non-blocking mode: " + std::string(strerror(errno)));
		cleanup_pipes(pipe_in, pipe_out);
		return;
	}

	CgiTunnel tunnel;
	tunnel.in_fd = pipe_in[1];
	tunnel.out_fd = pipe_out[0];
	tunnel.client_fd = req.client_fd;
	tunnel.server_fd = req.associated_conf->server_fd;
	tunnel.config = req.associated_conf;
	tunnel.location = findMatchingLocation(req.associated_conf, req.location_path);
	tunnel.is_busy = true;
	tunnel.last_used = std::chrono::steady_clock::now();
	tunnel.pid = -1;

	std::string requested_file = req.requested_path;
	size_t last_slash = requested_file.find_last_of('/');
	if (last_slash != std::string::npos) {
		requested_file = requested_file.substr(last_slash + 1);
	}
	if (requested_file.empty() || requested_file == "?") {
		requested_file = "index.php";
	}
	tunnel.script_path = tunnel.config->root + "/" + requested_file;
	Logger::file("FORK IT BABY!!!!");
	pid_t pid = fork();
	if (pid < 0) {
		Logger::file("Fork failed: " + std::string(strerror(errno)));
		cleanup_pipes(pipe_in, pipe_out);
		return;
	}

	if (pid == 0) {
		close(pipe_in[1]);
		close(pipe_out[0]);

		if (dup2(pipe_in[0], STDIN_FILENO) < 0) {
			Logger::file("dup2 stdin failed: " + std::string(strerror(errno)));
			_exit(1);
		}
		if (dup2(pipe_out[1], STDOUT_FILENO) < 0) {
			Logger::file("dup2 stdout failed: " + std::string(strerror(errno)));
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

	ss.str("");
	ss << "CGI tunnel created:\n"
	<< "- PID: " << pid << "\n"
	<< "- in_fd: " << req.cgi_in_fd << "\n"
	<< "- out_fd: " << req.cgi_out_fd << "\n"
	<< "- client_fd: " << req.client_fd << "\n"
	<< "- script: " << tunnel.script_path << "\n"
	<< "- Active tunnels: " << tunnels.size();
	Logger::file(ss.str());
}

void CgiHandler::cleanupCGI(RequestState &req) {
	std::stringstream ss;
	ss << "\n=== CGI Cleanup Start ===\n"
	<< "Cleaning up CGI resources for client_fd " << req.client_fd;
	Logger::file(ss.str());

	if (req.cgi_pid > 0) {
		int status;
		pid_t wait_result = waitpid(req.cgi_pid, &status, WNOHANG);

		ss.str("");
		ss << "CGI process (PID " << req.cgi_pid << ") status check:\n"
		<< "- waitpid result: " << wait_result;
		if (wait_result > 0) {
			ss << "\n- exit status: " << WEXITSTATUS(status);
			if (WIFSIGNALED(status)) {
				ss << "\n- terminated by signal: " << WTERMSIG(status);
			}
		}
		Logger::file(ss.str());

		if (wait_result == 0) {
			Logger::file("CGI process still running, sending SIGTERM");
			kill(req.cgi_pid, SIGTERM);
			usleep(100000);
			wait_result = waitpid(req.cgi_pid, &status, WNOHANG);

			if (wait_result == 0) {
				Logger::file("Process didn't terminate, sending SIGKILL");
				kill(req.cgi_pid, SIGKILL);
				waitpid(req.cgi_pid, &status, 0);
			}
		}
		req.cgi_pid = -1;
	}

	if (req.cgi_in_fd != -1) {
		ss.str("");
		ss << "Closing CGI input pipe (fd " << req.cgi_in_fd << ")";
		Logger::file(ss.str());
		epoll_ctl(server.getGlobalFds().epoll_fd, EPOLL_CTL_DEL, req.cgi_in_fd, NULL);
		close(req.cgi_in_fd);
		server.getGlobalFds().svFD_to_clFD_map.erase(req.cgi_in_fd);
		req.cgi_in_fd = -1;
	}

	if (req.cgi_out_fd != -1) {
		ss.str("");
		ss << "Closing CGI output pipe (fd " << req.cgi_out_fd << ")";
		Logger::file(ss.str());
		epoll_ctl(server.getGlobalFds().epoll_fd, EPOLL_CTL_DEL, req.cgi_out_fd, NULL);
		close(req.cgi_out_fd);
		server.getGlobalFds().svFD_to_clFD_map.erase(req.cgi_out_fd);
		req.cgi_out_fd = -1;
	}

	Logger::file("=== CGI Cleanup End ===\n");
}


const Location* CgiHandler::findMatchingLocation(const ServerBlock* conf, const std::string& path) {
	for (size_t i = 0; i < conf->locations.size(); i++) {
		const Location &loc = conf->locations[i];
		if (path.find(loc.path) == 0) {
		Logger::file(path);
		Logger::file(loc.path);
		Logger::file("DAS IST CGI: " + loc.cgi);
		Logger::file(loc.cgi_param);
			return &loc;
		}
	}
	return NULL;
}

bool CgiHandler::needsCGI(const ServerBlock* conf, const std::string &path) {
	if (path.length() >= 4 && path.substr(path.length() - 4) == ".php") {
		return true;
	}

	Logger::file("MAEAEAEAEEEHHHHHH!!!!!!!");
	const Location* loc = findMatchingLocation(conf, path);
	if (!loc)
	{
	Logger::file("LOG NICHT DA!!!!!!!");
		return false;
	}
		Logger::file("DAS IST CGI OUTER: " + loc->cgi);
	if (!loc->cgi.empty())
	{
		Logger::file("CGI NICHT LEER, CGI WIRD VERWENDET!!!!!!!");
		return true;
	}
	Logger::file("after maeaeae");
	return false;
}

void CgiHandler::handleCGIWrite(int epfd, int fd, uint32_t events) {
	std::stringstream ss;
	ss << "\n=== CGI Write Handler Start ===\n"
	<< "Handling fd=" << fd << "\n"
	<< "Active tunnels: " << tunnels.size() << "\n"
	<< "Events: EPOLLIN=" << (events & EPOLLIN) << " EPOLLOUT=" << (events & EPOLLOUT)
	<< " EPOLLHUP=" << (events & EPOLLHUP) << " EPOLLERR=" << (events & EPOLLERR);
	Logger::file(ss.str());

	if (events & (EPOLLERR | EPOLLHUP)) {
		Logger::file("Pipe error or hangup detected");
		auto tunnel_it = fd_to_tunnel.find(fd);
		if (tunnel_it != fd_to_tunnel.end() && tunnel_it->second) {
			cleanup_tunnel(*(tunnel_it->second));
		}
		return;
	}

	auto tunnel_it = fd_to_tunnel.find(fd);
	if (tunnel_it == fd_to_tunnel.end() || !tunnel_it->second) {
		Logger::file("No valid tunnel found for fd " + std::to_string(fd));
		return;
	}

	CgiTunnel* tunnel = tunnel_it->second;

	if (tunnel->pid > 0) {
		int status;
		pid_t result = waitpid(tunnel->pid, &status, WNOHANG);
		if (result > 0) {
			ss.str("");
			ss << "CGI process " << tunnel->pid << " has terminated with status " << status;
			Logger::file(ss.str());
			cleanup_tunnel(*tunnel);
			return;
		} else if (result < 0 && errno != ECHILD) {
			Logger::file("waitpid error: " + std::string(strerror(errno)));
			cleanup_tunnel(*tunnel);
			return;
		}
	}

	auto req_it = server.getGlobalFds().request_state_map.find(tunnel->client_fd);
	if (req_it == server.getGlobalFds().request_state_map.end()) {
		Logger::file("No request state found for client_fd " + std::to_string(tunnel->client_fd));
		cleanup_tunnel(*tunnel);
		return;
	}

	RequestState &req = req_it->second;

	if (req.request_buffer.empty()) {
		Logger::file("Request buffer is empty, closing input pipe");
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
		close(fd);
		tunnel->in_fd = -1;
		server.getGlobalFds().svFD_to_clFD_map.erase(fd);
		fd_to_tunnel.erase(fd);
		return;
	}

	ss.str("");
	ss << "Writing " << req.request_buffer.size() << " bytes to CGI process";
	ss << std::string(req.request_buffer.begin(), req.request_buffer.end()) << "\n";
	Logger::file(ss.str());

	size_t to_write = std::min(static_cast<size_t>(CHUNK_SIZE), req.request_buffer.size());
	ssize_t written = write(fd, req.request_buffer.data(), to_write);

	if (written < 0) {
		ss.str("");
		ss << "Write error: " << strerror(errno) << " (errno: " << errno << ")";
		Logger::file(ss.str());

		if (errno == EPIPE) {
			Logger::file("Broken pipe detected");
			cleanup_tunnel(*tunnel);
		} else if (errno != EAGAIN && errno != EWOULDBLOCK) {
			Logger::file("Fatal write error");
			cleanup_tunnel(*tunnel);
		}
		return;
	}

	if (written > 0) {
		req.request_buffer.erase(req.request_buffer.begin(),
                                req.request_buffer.begin() + written);
		ss.str("");
		ss << "Successfully wrote " << written << " bytes";
		Logger::file(ss.str());

		if (req.request_buffer.empty()) {
			Logger::file("Request buffer empty after write, closing input pipe");
			epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
			close(fd);
			tunnel->in_fd = -1;
			server.getGlobalFds().svFD_to_clFD_map.erase(fd);
			fd_to_tunnel.erase(fd);
		} else {
			server.modEpoll(epfd, fd, EPOLLOUT);
		}
	}

	Logger::file("=== CGI Write Handler End ===");
}

void CgiHandler::handleCGIRead(int epfd, int fd) {
    std::stringstream ss;
    ss << "\n=== CGI Read Handler Start ===\n"
       << "Handling fd=" << fd;
    Logger::file(ss.str());

    auto tunnel_it = fd_to_tunnel.find(fd);
    if (tunnel_it == fd_to_tunnel.end() || !tunnel_it->second) {
        Logger::file("No tunnel found for fd " + std::to_string(fd));
        return;
    }
    CgiTunnel* tunnel = tunnel_it->second;

    auto req_it = server.getGlobalFds().request_state_map.find(tunnel->client_fd);
    if (req_it == server.getGlobalFds().request_state_map.end()) {
        Logger::file("No request state found for client_fd " + std::to_string(tunnel->client_fd));
        return;
    }
    RequestState &req = req_it->second;

    // Nicht-statische Variable!
    std::string output;
    char buf[4096];
    ssize_t n;

    ss.str("");
    ss << "Attempting to read from CGI output pipe";
    Logger::file(ss.str());

    while ((n = read(fd, buf, sizeof(buf))) > 0) {
        output.append(buf, n);
        ss.str("");
        ss << "Read " << n << " bytes from CGI";
        Logger::file(ss.str());
    }

    if (n == 0) {  // EOF erreicht
        ss.str("");
        ss << "EOF reached, total bytes read: " << output.size();
        Logger::file(ss.str());

        size_t header_end = output.find("\r\n\r\n");
        if (header_end == std::string::npos) {
            header_end = 0;
        }

        std::string cgi_headers = header_end > 0 ?
            output.substr(0, header_end) : "";
        std::string cgi_body = header_end > 0 ?
            output.substr(header_end + 4) : output;

        ss.str("");
        ss << "Parsed CGI output:\n"
           << "Headers (" << cgi_headers.length() << " bytes):\n" << cgi_headers << "\n"
           << "Body (" << cgi_body.length() << " bytes)";
        Logger::file(ss.str());

        std::string response = "HTTP/1.1 200 OK\r\n";
        response += "Content-Length: " + std::to_string(cgi_body.length()) + "\r\n";
        response += "Connection: close\r\n";

        if (!cgi_headers.empty()) {
            response += cgi_headers + "\r\n";
        } else {
            response += "Content-Type: text/html\r\n";
        }

        response += "\r\n" + cgi_body;

        ss.str("");
        ss << "Final response headers:\n" << response.substr(0, response.find("\r\n\r\n"));
        Logger::file(ss.str());

        req.response_buffer.assign(response.begin(), response.end());
        req.state = RequestState::STATE_SENDING_RESPONSE;
        server.modEpoll(epfd, tunnel->client_fd, EPOLLOUT);

        cleanup_tunnel(*tunnel);
    }
    else if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            ss.str("");
            ss << "Read error: " << strerror(errno);
            Logger::file(ss.str());
            cleanup_tunnel(*tunnel);
        }
    }

    Logger::file("=== CGI Read Handler End ===\n");
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

	std::vector<std::string> env_vars = {
		"REDIRECT_STATUS=200",
		"GATEWAY_INTERFACE=CGI/1.1",
		"SERVER_PROTOCOL=HTTP/1.1",
		"REQUEST_METHOD=" + method,
		"QUERY_STRING=" + query,
		"SCRIPT_FILENAME=" + tunnel.script_path,  // Full path is important
		"SCRIPT_NAME=" + tunnel.script_path,
		"DOCUMENT_ROOT=" + tunnel.config->root,
		"SERVER_SOFTWARE=webserv/1.0",
		"SERVER_NAME=localhost",
		"SERVER_PORT=" + tunnel.config->port,
		"CONTENT_TYPE=application/x-www-form-urlencoded",
		"PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
	};

	for (const auto& env_var : env_vars) {
		putenv(strdup(env_var.c_str()));
	}
}

void CgiHandler::execute_cgi(const CgiTunnel &tunnel) {
	Logger::file("Child process environment:");
	for (char** env = environ; *env != nullptr; env++) {
		Logger::file(*env);
	}

	Logger::file("File descriptors:");
	Logger::file("STDIN: " + std::to_string(fcntl(STDIN_FILENO, F_GETFD)));
	Logger::file("STDOUT: " + std::to_string(fcntl(STDOUT_FILENO, F_GETFD)));
	Logger::file("STDERR: " + std::to_string(fcntl(STDERR_FILENO, F_GETFD)));

	if (!tunnel.location) {
		Logger::file("No matching location found for CGI execution");
		_exit(1);
	}

	Logger::file("Executing CGI with:");
	Logger::file("- PHP-CGI path: " + tunnel.location->cgi);
	Logger::file("- Script path: " + tunnel.script_path);

	if (access(php_cgi, X_OK) != 0) {
		Logger::file("PHP-CGI binary not executable or not found at " + std::string(php_cgi));
		_exit(1);
	}

	if (!tunnel.location) {
		Logger::file("No matching location found for CGI execution");
		_exit(1);
	}

	Logger::file("Executing CGI with:");
	Logger::file("- PHP-CGI path: " + tunnel.location->cgi);
	Logger::file("- Script path: " + tunnel.script_path);

	if (access(tunnel.location->cgi.c_str(), X_OK) != 0) {
		Logger::file("PHP-CGI binary not executable or not found at " + tunnel.location->cgi +
					": " + std::string(strerror(errno)));
		_exit(1);
	}

	if (access(tunnel.script_path.c_str(), R_OK) != 0) {
		Logger::file("Script file not readable at " + tunnel.script_path +
					": " + std::string(strerror(errno)));
		_exit(1);
	}

	char* const args[] = {
		(char*)php_cgi,
		(char*)tunnel.location->cgi.c_str(),
		(char*)tunnel.script_path.c_str(),
		nullptr
	};

	execve(args[0], args, environ);
	Logger::file("execve failed for " + tunnel.location->cgi + ": " + std::string(strerror(errno)));
	_exit(1);
}

void CgiHandler::cleanup_pipes(int pipe_in[2], int pipe_out[2]) {
	if (pipe_in[0] != -1) close(pipe_in[0]);
	if (pipe_in[1] != -1) close(pipe_in[1]);
	if (pipe_out[0] != -1) close(pipe_out[0]);
	if (pipe_out[1] != -1) close(pipe_out[1]);
}