#include "CgiHandler.hpp"

void CgiHandler::execute_cgi(const CgiTunnel &tunnel)
{

	if (access(tunnel.location->cgi.c_str(), X_OK) != 0
		|| !tunnel.location
		|| access(tunnel.location->cgi.c_str(), X_OK) != 0
		|| access(tunnel.script_path.c_str(), R_OK) != 0)
		_exit(1);

	char* const args[] = {
		(char*)tunnel.location->cgi.c_str(),
		(char*)tunnel.script_path.c_str(),
		nullptr
	};
	server.setTaskStatus(RequestState::IN_PROGRESS, tunnel.client_fd);
	execve(args[0], args, environ);
	_exit(1);
}


bool CgiHandler::needsCGI(RequestState &req, const std::string &path)
{
	struct stat file_stat;
	if (stat(req.requested_path.c_str(), &file_stat) != 0)
		return false;

	if (S_ISDIR(file_stat.st_mode))
	{
		req.is_directory = true;
		return false;
	}

	const Location* loc = server.getRequestHandler()->findMatchingLocation(req.associated_conf, path);
	if (!loc || loc->cgi.empty())
		return false;

	size_t dot_pos = req.requested_path.find_last_of('.');
	if (dot_pos != std::string::npos)
	{
		std::string extension = req.requested_path.substr(dot_pos);
		if (extension == loc->cgi_filetype)
			return true;
	}
	return false;
}

void CgiHandler::handleCGIWrite(int epfd, int fd, uint32_t events)
{

	if (events & (EPOLLERR | EPOLLHUP))
	{
		auto tunnel_it = fd_to_tunnel.find(fd);
		if (tunnel_it != fd_to_tunnel.end() && tunnel_it->second)
			cleanup_tunnel(*(tunnel_it->second));
		return;
	}

	auto tunnel_it = fd_to_tunnel.find(fd);
	if (tunnel_it == fd_to_tunnel.end() || !tunnel_it->second)
	{
		return;
	}

	CgiTunnel* tunnel = tunnel_it->second;
	auto req_it = server.getGlobalFds().request_state_map.find(tunnel->client_fd);
	if (req_it == server.getGlobalFds().request_state_map.end())
	{
		cleanup_tunnel(*tunnel);
		return;
	}

	RequestState &req = req_it->second;

	if (req.request_body.empty() && !req.request_buffer.empty())
	{
		std::string request_str(req.request_buffer.begin(), req.request_buffer.end());
		size_t body_start = request_str.find("\r\n\r\n");
		if (body_start != std::string::npos)
		{
			req.request_body = request_str.substr(body_start + 4);
		}
	}

	if (req.request_body.empty())
	{
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
		close(fd);
		tunnel->in_fd = -1;
		server.getGlobalFds().svFD_to_clFD_map.erase(fd);
		fd_to_tunnel.erase(fd);
		return;
	}

	const size_t MAX_CHUNK_SIZE = 4096;
	size_t pos = 0;
	while (pos < req.request_body.size())
	{
		size_t to_write = std::min(MAX_CHUNK_SIZE, req.request_body.size() - pos);
		ssize_t written = write(fd, req.request_body.c_str() + pos, to_write);

		if (written < 0)
		{
			if (errno == EPIPE)
			{
				cleanup_tunnel(*tunnel);
				return;
			}
			else if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				return;
			}
			else
			{
				cleanup_tunnel(*tunnel);
				return;
			}
		}

		if (written > 0)
		{
			pos += written;
		}

		if (written < static_cast<ssize_t>(to_write))
		{
			break;
		}
	}

	if (pos >= req.request_body.size())
	{
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, nullptr);
		close(fd);
		tunnel->in_fd = -1;
		server.getGlobalFds().svFD_to_clFD_map.erase(fd);
		fd_to_tunnel.erase(fd);
		req.request_body.clear();
	}
}



void CgiHandler::handleCGIRead(int epfd, int fd)
{
	auto tunnel_it = fd_to_tunnel.find(fd);
	if (tunnel_it == fd_to_tunnel.end() || !tunnel_it->second)
	{
		return;
	}

	CgiTunnel* tunnel = tunnel_it->second;
	auto req_it = server.getGlobalFds().request_state_map.find(tunnel->client_fd);
	if (req_it == server.getGlobalFds().request_state_map.end())
	{
		return;
	}
	RequestState &req = req_it->second;

	const size_t chunk_size = 4096;
	char buf[chunk_size];
	ssize_t n;

	while ((n = read(fd, buf, chunk_size)) > 0)
	{
		if (req.cgi_output_buffer.size() + n > req.cgi_output_buffer.max_size())
		{
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

	if (n == 0)
	{
		std::string output(req.cgi_output_buffer.begin(), req.cgi_output_buffer.end());

		size_t header_end = output.find("\r\n\r\n");
		if (header_end == std::string::npos)
		{
			header_end = 0;
		}

		std::string cgi_headers = header_end > 0 ? output.substr(0, header_end) : "";
		std::string cgi_body = header_end > 0 ? output.substr(header_end + 4) : output;
		std::string response;
		if (!cgi_headers.empty() && cgi_headers.find("Location:") != std::string::npos)
		{
			response = "HTTP/1.1 302 Found\r\n" + cgi_headers + "\r\n\r\n";
		}
		else
		{
			response = "HTTP/1.1 200 OK\r\n";
			response += "Content-Length: " + std::to_string(cgi_body.length()) + "\r\n";
			response += "Connection: close\r\n";

			if (!cgi_headers.empty())
				response += cgi_headers + "\r\n";
			else
				response += "Content-Type: text/html\r\n";
			response += "\r\n" + cgi_body;
		}

		if (response.length() > req.response_buffer.max_size())
		{
			std::string error_response = "HTTP/1.1 500 Internal Server Error\r\n"
										"Content-Type: text/html\r\n"
										"Content-Length: 26\r\n\r\n"
										"Response exceeds limits.";
			req.response_buffer.clear();
			req.response_buffer.insert(req.response_buffer.begin(),
									error_response.begin(),
									error_response.end());
		}
		else
		{
			req.response_buffer.clear();
			req.response_buffer.insert(req.response_buffer.begin(), response.begin(), response.end());
		}

		req.state = RequestState::STATE_SENDING_RESPONSE;
		server.modEpoll(epfd, tunnel->client_fd, EPOLLOUT);

		server.getTaskManager()->sendTaskStatusUpdate(tunnel->client_fd, RequestState::COMPLETED);
		cleanup_tunnel(*tunnel);
	}
	else if (n < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			server.getTaskManager()->sendTaskStatusUpdate(tunnel->client_fd, RequestState::COMPLETED);
			cleanup_tunnel(*tunnel);
		}
	}
}
