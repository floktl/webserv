#include "CgiHandler.hpp"

CgiHandler::CgiHandler(Server& _server) : server(_server) {}

CgiHandler::~CgiHandler() {
	for (auto& pair : tunnels) {
		cleanup_tunnel(pair.second);
	}
}

void CgiHandler::finalizeCgiResponse(RequestState &req, int epoll_fd, int client_fd)
{
	std::string output(req.cgi_output_buffer.begin(), req.cgi_output_buffer.end());
	size_t header_end = output.find("\r\n\r\n");
	std::string response;

	if (header_end != std::string::npos &&
		(output.find("Content-type:") != std::string::npos ||
		output.find("Content-Type:") != std::string::npos))
	{
		std::string headers = output.substr(0, header_end);
		std::string body = output.substr(header_end + 4);

		response = "HTTP/1.1 200 OK\r\n";
		if (headers.find("Content-Length:") == std::string::npos)
			response += "Content-Length: " + std::to_string(body.length()) + "\r\n";
		if (headers.find("Connection:") == std::string::npos)
			response += "Connection: close\r\n";
		response += headers + "\r\n" + body;
	}
	else
	{
		response = "HTTP/1.1 200 OK\r\n"
				"Content-Type: text/html\r\n"
				"Content-Length: " + std::to_string(output.size()) + "\r\n"
				"Connection: close\r\n\r\n" + output;
	}

	if (response.length() > req.response_buffer.max_size())
	{
		std::string error_response =
			"HTTP/1.1 500 Internal Server Error\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: 26\r\n"
			"Connection: close\r\n\r\n"
			"CGI response too large.";

		req.response_buffer.clear();
		req.response_buffer.insert(req.response_buffer.begin(),
								error_response.begin(),
								error_response.end());
	}
	else
	{
		req.response_buffer.clear();
		req.response_buffer.insert(req.response_buffer.begin(),
								response.begin(),
								response.end());
	}

	req.state = RequestState::STATE_SENDING_RESPONSE;
	server.modEpoll(epoll_fd, client_fd, EPOLLOUT);
}

std::vector<char*> CgiHandler::setup_cgi_environment(const CgiTunnel &tunnel, const std::string &method, const std::string &query) {
    std::vector<char*> envp;
    std::string content_length = "0";
    std::string content_type = "application/x-www-form-urlencoded";
    std::string boundary;
    std::string http_cookie;

    auto req_it = server.getGlobalFds().request_state_map.find(tunnel.client_fd);
    if (req_it != server.getGlobalFds().request_state_map.end()) {
        const RequestState &req = req_it->second;
        http_cookie = req.cookie_header;
        std::string request(req.request_buffer.begin(), req.request_buffer.end());
        size_t header_end = request.find("\r\n\r\n");

        if (method == "POST" && header_end != std::string::npos) {
            size_t cl_pos = request.find("Content-Length: ");
            if (cl_pos != std::string::npos) {
                size_t cl_end = request.find("\r\n", cl_pos);
                if (cl_end != std::string::npos) {
                    content_length = request.substr(cl_pos + 16, cl_end - (cl_pos + 16));
                }
            }

            size_t ct_pos = request.find("Content-Type: ");
            if (ct_pos != std::string::npos) {
                size_t ct_end = request.find("\r\n", ct_pos);
                if (ct_end != std::string::npos) {
                    content_type = request.substr(ct_pos + 14, ct_end - (ct_pos + 14));
                    if (content_type.find("multipart/form-data") != std::string::npos) {
                        size_t boundary_pos = content_type.find("boundary=");
                        if (boundary_pos != std::string::npos) {
                            boundary = content_type.substr(boundary_pos + 9);
                        }
                    }
                }
            }
        }

        // Log request body to check content
        Logger::file("CGI Request Body:\n" + req.request_body);
    }

    std::vector<std::string> vars = {
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
        "UPLOAD_STORE=" + tunnel.location->upload_store,
        "CONTENT_TYPE=" + content_type,
        "CONTENT_LENGTH=" + content_length,
        "HTTP_COOKIE=" + http_cookie,
        "PATH=/usr/local/sbin:/usr/local/bin:/usr/sbin:/usr/bin:/sbin:/bin"
    };

    if (!boundary.empty()) {
        vars.push_back("BOUNDARY=" + boundary);
    }

    if (method == "POST") {
        if (content_type.find("multipart/form-data") != std::string::npos) {
            vars.push_back("REQUEST_TYPE=multipart/form-data");
        }
        vars.push_back("HTTP_TRANSFER_ENCODING=chunked");
    }

    for (const auto& var : vars) {
        char* env_var = strdup(var.c_str());
        if (env_var) {
            envp.push_back(env_var);
        }
    }
    envp.push_back(nullptr);
    return envp;
}


bool CgiHandler::initTunnel(RequestState &req, CgiTunnel &tunnel, int pipe_in[2],
	int pipe_out[2])
{
	if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0)
	{
		cleanup_pipes(pipe_in, pipe_out);
		return false;
	}

	if (fcntl(pipe_in[0], F_SETFL, O_NONBLOCK) < 0 ||
		fcntl(pipe_in[1], F_SETFL, O_NONBLOCK) < 0 ||
		fcntl(pipe_out[0], F_SETFL, O_NONBLOCK) < 0 ||
		fcntl(pipe_out[1], F_SETFL, O_NONBLOCK) < 0)
	{
		cleanup_pipes(pipe_in, pipe_out);
		return false;
	}

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
	Logger::file("req.upload_store: " + tunnel.location->upload_store);
	tunnel.script_path = req.requested_path;
	tunnel.request = req;
	return true;
}

void CgiHandler::handleChildProcess(int pipe_in[2], int pipe_out[2], CgiTunnel &tunnel, const std::string &method, const std::string &query) {
	close(pipe_in[1]);
	close(pipe_out[0]);

	int status_pipe[2];
	if (pipe(status_pipe) < 0) {
		_exit(1);
	}

	int flags = fcntl(status_pipe[1], F_GETFL, 0);
	fcntl(status_pipe[1], F_SETFL, flags | O_NONBLOCK);

	RequestState::Task status = RequestState::IN_PROGRESS;
	write(status_pipe[1], &status, sizeof(status));

	if (dup2(pipe_in[0], STDIN_FILENO) < 0 || dup2(pipe_out[1], STDOUT_FILENO) < 0) {
		status = RequestState::COMPLETED;
		write(status_pipe[1], &status, sizeof(status));
		_exit(1);
	}

	close(pipe_in[0]);
	close(pipe_out[1]);
	//tunnel.envp = setup_cgi_environment(tunnel, method, query);
	Logger::file("Executing CGI script: " + tunnel.script_path);
    Logger::file("Request body content: " + tunnel.request.request_body);

    if (access(tunnel.location->cgi.c_str(), X_OK) != 0 ||
        !tunnel.location ||
        access(tunnel.script_path.c_str(), R_OK) != 0) {
        Logger::file("[ERROR] Cannot execute CGI script at: " + tunnel.script_path);
        _exit(1);
    }

    // Prepare arguments for execve
    std::vector<char*> args;
    args.push_back(strdup(tunnel.location->cgi.c_str()));  // CGI interpreter
    args.push_back(strdup(tunnel.script_path.c_str()));    // Script path
    args.push_back(nullptr);

    // Set up the environment
    std::vector<char*> envp = setup_cgi_environment(tunnel, method, query);

	// Send request body to stdin of CGI process
    if (!tunnel.request.request_body.empty()) {
        ssize_t written = write(STDIN_FILENO, tunnel.request.request_body.c_str(), tunnel.request.request_body.size());
        if (written < 0) {
            Logger::file("[ERROR] Failed to write request body to CGI script: " + std::string(strerror(errno)));
            _exit(1);
        }
    }

    execve(args[0], args.data(), envp.data());

    Logger::file("[ERROR] execve failed: " + std::string(strerror(errno)));
    _exit(1);
	status = RequestState::COMPLETED;
	write(status_pipe[1], &status, sizeof(status));

	close(status_pipe[1]);
	_exit(1);
}


void CgiHandler::addCgiTunnel(RequestState &req, const std::string &method, const std::string &query) {
	int pipe_in[2] = {-1, -1};
	int pipe_out[2] = {-1, -1};
	int status_pipe[2] = {-1, -1};
	CgiTunnel tunnel;

	if (pipe(status_pipe) < 0) {
		return;
	}

	if (!initTunnel(req, tunnel, pipe_in, pipe_out)) {
		close(status_pipe[0]);
		close(status_pipe[1]);
		return;
	}

	pid_t pid = fork();
	if (pid < 0) {
		cleanup_pipes(pipe_in, pipe_out);
		close(status_pipe[0]);
		close(status_pipe[1]);
		cleanup_tunnel(tunnel);
		return;
	}

	if (pid == 0) {
		close(status_pipe[0]);
		handleChildProcess(pipe_in, pipe_out, tunnel, method, query);
	}

	close(pipe_in[0]);
	close(pipe_out[1]);
	close(status_pipe[1]);

	int flags = fcntl(status_pipe[0], F_GETFL, 0);
	fcntl(status_pipe[0], F_SETFL, flags | O_NONBLOCK);

	req.pipe_fd = status_pipe[0];

	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = status_pipe[0];
	if (epoll_ctl(server.getGlobalFds().epoll_fd, EPOLL_CTL_ADD, status_pipe[0], &ev) < 0) {
		Logger::file("[ERROR] Failed to add status pipe to epoll");
	}

	server.modEpoll(server.getGlobalFds().epoll_fd, tunnel.in_fd, EPOLLOUT);
	server.modEpoll(server.getGlobalFds().epoll_fd, tunnel.out_fd, EPOLLIN);

	server.getGlobalFds().svFD_to_clFD_map[tunnel.in_fd] = req.client_fd;
	server.getGlobalFds().svFD_to_clFD_map[tunnel.out_fd] = req.client_fd;
	server.getGlobalFds().svFD_to_clFD_map[status_pipe[0]] = req.client_fd;

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

	server.getTaskManager()->sendTaskStatusUpdate(req.client_fd, RequestState::IN_PROGRESS);
}

