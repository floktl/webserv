#include "CgiHandler.hpp"

void CgiHandler::free_environment(std::vector<char*>& env) {
	for (char* ptr : env) {
		if (ptr) free(ptr);
	}
	env.clear();
}

void CgiHandler::execute_cgi(const CgiTunnel &tunnel) {
	if (access(tunnel.location->cgi.c_str(), X_OK) != 0 ||
		!tunnel.location ||
		access(tunnel.script_path.c_str(), R_OK) != 0) {
		_exit(1);
	}

	char* const args[] = {
		(char*)tunnel.location->cgi.c_str(),
		(char*)tunnel.script_path.c_str(),
		nullptr
	};

	execve(args[0], args, tunnel.envp.data());
	_exit(1);
}

bool CgiHandler::needsCGI(RequestState &req, const std::string &path)
{
    Logger::file("[needsCGI] Entering function with path: " + path);

	if (req.is_multipart)
	{
        Logger::file("[needsCGI] Detected file upload request, skipping CGI processing.");
		struct stat file_stat;
    	if (stat(req.requested_path.c_str(), &file_stat) != 0)
    	{
    	    Logger::file("[needsCGI] Stat failed for path: " + req.requested_path);
    	    return false;
    	}

    	if (S_ISDIR(file_stat.st_mode))
    	{
    	    Logger::file("[needsCGI] Path is a directory: " + req.requested_path);
    	    req.is_directory = true;
    	    return false;
    	}
        return false;
    }
	else
	{
        Logger::file("[needsCGI] Regular form submission detected.");
    Logger::file("[needsCGI] Checking file status for path: " + req.requested_path);


    Logger::file("[needsCGI] Checking for CGI configuration...");
    const Location* loc = server.getRequestHandler()->findMatchingLocation(req.associated_conf, path);

    if (!loc)
    {
        Logger::file("[needsCGI] No matching location found for path: " + path);
        return false;
    }

    if (loc->cgi.empty())
    {
        Logger::file("[needsCGI] CGI configuration is empty for path: " + path);
        return false;
    }

    size_t dot_pos = req.requested_path.find_last_of('.');
    if (dot_pos != std::string::npos)
    {
        std::string extension = req.requested_path.substr(dot_pos);
        Logger::file("[needsCGI] Found file extension: " + extension);

        if (extension == loc->cgi_filetype)
        {
            Logger::file("[needsCGI] CGI script detected for path: " + req.requested_path);
            return true;
        }
        else
        {
            Logger::file("[needsCGI] File extension " + extension + " does not match CGI type " + loc->cgi_filetype);
        }
    }
    else
    {
        Logger::file("[needsCGI] No file extension found in path: " + req.requested_path);
    }

    Logger::file("[needsCGI] No matching CGI file type for path: " + req.requested_path);
    }
    return false;
}




void CgiHandler::handleCGIWrite(int epfd, int fd, uint32_t events) {
    auto tunnel_it = fd_to_tunnel.find(fd);
    if (tunnel_it == fd_to_tunnel.end() || !tunnel_it->second) {
        return;
    }
    CgiTunnel* tunnel = tunnel_it->second;
    auto req_it = server.getGlobalFds().request_state_map.find(tunnel->client_fd);
    if (req_it == server.getGlobalFds().request_state_map.end()) {
        cleanup_tunnel(*tunnel);
        return;
    }
    RequestState &req = req_it->second;
    // Prüfe ob der Request noch aktiv ist
    if (req.state != RequestState::STATE_CGI_RUNNING) {
        cleanup_tunnel(*tunnel);
        return;
    }
    if (events & (EPOLLERR | EPOLLHUP)) {
        cleanup_tunnel(*tunnel);
        return;
    }
    if (req.request_body.empty()) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
        tunnel->in_fd = -1;
        server.getGlobalFds().svFD_to_clFD_map.erase(fd);
        fd_to_tunnel.erase(fd);
        return;
    }
    const size_t MAX_CHUNK_SIZE = 4096;
    size_t pos = 0;
    while (pos < req.request_body.size()) {
        size_t to_write = std::min(MAX_CHUNK_SIZE, req.request_body.size() - pos);
        ssize_t written = write(fd, req.request_body.c_str() + pos, to_write);
        if (written < 0) {
            if (errno == EPIPE) {
                cleanup_tunnel(*tunnel);
                return;
            } else if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return;
            } else {
                cleanup_tunnel(*tunnel);
                return;
            }
        }
        pos += written;
        if (written < static_cast<ssize_t>(to_write)) {
            break;
        }
    }
    if (pos >= req.request_body.size()) {
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
        tunnel->in_fd = -1;
        server.getGlobalFds().svFD_to_clFD_map.erase(fd);
        fd_to_tunnel.erase(fd);
        req.request_body.clear();
    }
}










void CgiHandler::handleCGIRead(int epfd, int fd) {
    auto tunnel_it = fd_to_tunnel.find(fd);
    if (tunnel_it == fd_to_tunnel.end() || !tunnel_it->second) {
        return;
    }
    CgiTunnel* tunnel = tunnel_it->second;
    auto req_it = server.getGlobalFds().request_state_map.find(tunnel->client_fd);
    if (req_it == server.getGlobalFds().request_state_map.end()) {
        cleanup_tunnel(*tunnel);
        return;
    }
    RequestState &req = req_it->second;
    // Prüfe ob der Request bereits bearbeitet wurde
    if (req.state != RequestState::STATE_CGI_RUNNING) {
        cleanup_tunnel(*tunnel);
        return;
    }
    const size_t chunk_size = 4096;
    char buf[chunk_size];
    ssize_t n;
    while ((n = read(fd, buf, chunk_size)) > 0) {
        if (req.cgi_output_buffer.size() + n > req.cgi_output_buffer.max_size()) {
            std::string error_response = "HTTP/1.1 500 Internal Server Error\r\n"
                                        "Content-Type: text/html\r\n"
                                        "Content-Length: 26\r\n\r\n"
                                        "CGI output exceeds limits.";
            req.response_buffer.clear();
            req.response_buffer.insert(req.response_buffer.begin(),
                                    error_response.begin(),
                                    error_response.end());
            req.state = RequestState::STATE_SENDING_RESPONSE;
            server.modEpoll(epfd, tunnel->client_fd, EPOLLOUT);
            server.getTaskManager()->sendTaskStatusUpdate(tunnel->client_fd, RequestState::COMPLETED);
            cleanup_tunnel(*tunnel);
            return;
        }
        req.cgi_output_buffer.insert(req.cgi_output_buffer.end(), buf, buf + n);
    }
    if (n == 0) {
        finalizeCgiResponse(req, epfd, tunnel->client_fd);
        cleanup_tunnel(*tunnel);
    } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        cleanup_tunnel(*tunnel);
    }
}
