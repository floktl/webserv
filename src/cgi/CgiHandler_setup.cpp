#include "CgiHandler.hpp"

CgiHandler::CgiHandler(Server& _server) : server(_server) {}

CgiHandler::~CgiHandler() {
	for (auto& pair : tunnels) {
		cleanup_tunnel(pair.second);
	}
}

void CgiHandler::setup_cgi_environment(const CgiTunnel &tunnel, const std::string &method, const std::string &query)
{
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

	if (method == "POST")
	{
		env_vars.push_back("HTTP_TRANSFER_ENCODING=chunked");
		if (content_type.find("boundary=") != std::string::npos)
			env_vars.push_back("REQUEST_TYPE=multipart/form-data");
	}

	for (const auto& env_var : env_vars)
	{
		Logger::file("  " + env_var);
		putenv(strdup(env_var.c_str()));
	}
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
    tunnel.script_path = req.requested_path;

    return true;
}

void CgiHandler::handleChildProcess(int pipe_in[2], int pipe_out[2], CgiTunnel &tunnel, const std::string &method, const std::string &query) {
    // Close unused ends of the pipes in the child process
    close(pipe_in[1]);
    close(pipe_out[0]);

    // Redirect pipes to standard input and output
    if (dup2(pipe_in[0], STDIN_FILENO) < 0 || dup2(pipe_out[1], STDOUT_FILENO) < 0) {
        Logger::file("[ERROR] dup2 failed");
        _exit(1);
    }
    close(pipe_in[0]);
    close(pipe_out[1]);

    // Set up CGI environment and execute the script
    setup_cgi_environment(tunnel, method, query);
	server.setTaskStatus(RequestState::IN_PROGRESS, tunnel.client_fd);
    execute_cgi(tunnel);
    // Exit with an error if CGI execution fails
    Logger::file("[ERROR] execute_cgi failed");
    _exit(1);
}


void CgiHandler::addCgiTunnel(RequestState &req, const std::string &method, const std::string &query)
{
    Logger::file("addCgiTunnel");
    Logger::file("Method: " + method + ", Content Type: ");
    Logger::file("Request Body: " + std::string(req.request_buffer.begin(), req.request_buffer.end()));
    Logger::file("Query: " + query);

    int pipe_in[2] = {-1, -1};
    int pipe_out[2] = {-1, -1};
    CgiTunnel tunnel;

    if (!initTunnel(req, tunnel, pipe_in, pipe_out))
        return;

    pid_t pid = fork();
    if (pid < 0)
	{
        cleanup_pipes(pipe_in, pipe_out);
        return;
    }

	// Child process
    if (pid == 0)
        handleChildProcess(pipe_in, pipe_out, tunnel, method, query);

    // Parent process
    close(pipe_in[0]);
    close(pipe_out[1]);
	if (server.getTaskStatus(tunnel.client_fd) == RequestState::IN_PROGRESS)
	{
		server.setTaskStatus(RequestState::COMPLETED, tunnel.client_fd);
		Logger::file("after");
	}
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
