#include "CgiHandler.hpp"

void CgiHandler::free_environment(std::vector<char*>& env) {
	for (char* ptr : env) {
		if (ptr) free(ptr);
	}
	env.clear();
}


bool CgiHandler::needsCGI(RequestState &req, const std::string &path)
{
    Logger::file("[needsCGI] Entering function with path: " + path);

	if (req.is_multipart)
	{
		Logger::file("[needsCGI] Detected multipart request, checking CGI processing.");

		// Prüfe, ob die angeforderte Datei tatsächlich ein CGI-Skript ist
		size_t dot_pos = req.requested_path.find_last_of('.');
		if (dot_pos != std::string::npos)
		{
			std::string extension = req.requested_path.substr(dot_pos);
			Logger::file("[needsCGI] Found file extension: " + extension);

			const Location* loc = server.getRequestHandler()->findMatchingLocation(req.associated_conf, path);
			if (loc && !loc->cgi.empty() && extension == loc->cgi_filetype)
			{
				Logger::file("[needsCGI] Multipart request matches CGI type: " + extension);
				return true;
			}
			else
			{
				Logger::file("[needsCGI] Multipart request does not match CGI, treating as file upload.");
			}
		}
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
        Logger::file("[ERROR] Invalid tunnel, skipping CGI write.");
        return;
    }
    CgiTunnel* tunnel = tunnel_it->second;
    auto req_it = server.getGlobalFds().request_state_map.find(tunnel->client_fd);
    if (req_it == server.getGlobalFds().request_state_map.end()) {
        Logger::file("[ERROR] Request state not found for client FD: " + std::to_string(tunnel->client_fd));
        cleanup_tunnel(*tunnel);
        return;
    }
    RequestState &req = req_it->second;

    Logger::file("[INFO] Processing CGI write for client FD: " + std::to_string(tunnel->client_fd));
    Logger::file("[INFO] Request body content: " + req.request_body);

    if (req.state != RequestState::STATE_CGI_RUNNING) {
        Logger::file("[INFO] Request no longer active, cleaning up.");
        cleanup_tunnel(*tunnel);
        return;
    }
    if (events & (EPOLLERR | EPOLLHUP)) {
        Logger::file("[ERROR] EPOLL error or hang-up detected, cleaning up.");
        cleanup_tunnel(*tunnel);
        return;
    }
    if (req.request_body.empty()) {
        Logger::file("[INFO] Request body is empty, closing connection.");
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
		Logger::file("[DEBUG] Current position in request body: " + std::to_string(pos));

		size_t to_write = std::min(MAX_CHUNK_SIZE, req.request_body.size() - pos);
		Logger::file("[DEBUG] Attempting to write " + std::to_string(to_write) + " bytes to file descriptor " + std::to_string(fd));

		ssize_t written = write(fd, req.request_body.c_str() + pos, to_write);

		if (written < 0) {
			Logger::file("[ERROR] Write operation failed with errno: " + std::to_string(errno) + " (" + std::string(strerror(errno)) + ")");

			if (errno == EPIPE) {
				Logger::file("[ERROR] Broken pipe detected, cleaning up.");
				cleanup_tunnel(*tunnel);
				return;
			} else if (errno == EAGAIN || errno == EWOULDBLOCK) {
				Logger::file("[INFO] Write would block, retrying later.");
				return;
			} else {
				Logger::file("[ERROR] Write failed: " + std::string(strerror(errno)));
				cleanup_tunnel(*tunnel);
				return;
			}
		}

		Logger::file("[INFO] Successfully written " + std::to_string(written) + " bytes to CGI process.");

		pos += written;
		Logger::file("[DEBUG] Updated position in request body: " + std::to_string(pos));

		if (written < static_cast<ssize_t>(to_write)) {
			Logger::file("[INFO] Partial write occurred, breaking the loop.");
			break;
		}
	}

Logger::file("[DEBUG] Exiting write loop. Final position: " + std::to_string(pos) + ", Total size: " + std::to_string(req.request_body.size()));

    if (pos >= req.request_body.size()) {
        Logger::file("[INFO] Entire request body sent, closing write end.");
        epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
        close(fd);
        tunnel->in_fd = -1;
        server.getGlobalFds().svFD_to_clFD_map.erase(fd);
        fd_to_tunnel.erase(fd);
        req.request_body.clear();
		Logger::file("eeeeeeeeeeeeeeeerrrrrrrrrrrraaaaaaaaaassssssssseeeeeeeeeeeddddddd");
    }
}

void CgiHandler::handleCGIRead(int epfd, int fd) {
    auto tunnel_it = fd_to_tunnel.find(fd);
    if (tunnel_it == fd_to_tunnel.end() || !tunnel_it->second) {
        Logger::file("[ERROR] Invalid tunnel, skipping CGI read.");
        return;
    }
    CgiTunnel* tunnel = tunnel_it->second;
    auto req_it = server.getGlobalFds().request_state_map.find(tunnel->client_fd);
    if (req_it == server.getGlobalFds().request_state_map.end()) {
        Logger::file("[ERROR] Request state not found for client FD: " + std::to_string(tunnel->client_fd));
        cleanup_tunnel(*tunnel);
        return;
    }
    RequestState &req = req_it->second;

    Logger::file("[INFO] Processing CGI read for client FD: " + std::to_string(tunnel->client_fd));

    if (req.state != RequestState::STATE_CGI_RUNNING) {
        Logger::file("[INFO] Request no longer active, cleaning up.");
        cleanup_tunnel(*tunnel);
        return;
    }
    const size_t chunk_size = 4096;
    char buf[chunk_size];
    ssize_t n;
    while ((n = read(fd, buf, chunk_size)) > 0) {
        Logger::file("[INFO] Read " + std::to_string(n) + " bytes from CGI process.");

        if (req.cgi_output_buffer.size() + n > req.cgi_output_buffer.max_size()) {
            Logger::file("[ERROR] CGI output exceeds buffer limits.");
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
        Logger::file("[INFO] CGI process finished reading, finalizing response.");
        finalizeCgiResponse(req, epfd, tunnel->client_fd);
        cleanup_tunnel(*tunnel);
    } else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
        Logger::file("[ERROR] Read error: " + std::string(strerror(errno)));
        cleanup_tunnel(*tunnel);
    }
}
