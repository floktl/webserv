#include "Server.hpp"

bool Server::handleClientEvent(int epoll_fd, int fd, uint32_t ev)
{
	RequestState &req = globalFDS.request_state_map[fd];
	req.last_activity = std::chrono::steady_clock::now();

	if (ev & (EPOLLHUP | EPOLLERR))
	{
		delFromEpoll(epoll_fd, fd);
		return true;
	}

	if (ev & EPOLLIN)
	{
		staticHandler->handleClientRead(epoll_fd, fd);
		if (!processMethod(req, epoll_fd))
			return true;
	}

	if (req.state != RequestState::STATE_CGI_RUNNING &&
		req.state != RequestState::STATE_PREPARE_CGI &&
		(ev & EPOLLOUT))
	{
		staticHandler->handleClientWrite(epoll_fd, fd);
	}

	if ((req.state == RequestState::STATE_CGI_RUNNING ||
		req.state == RequestState::STATE_PREPARE_CGI) &&
		(ev & EPOLLOUT) && !req.response_buffer.empty())
	{
		char send_buffer[8192];
		size_t chunk_size = std::min(req.response_buffer.size(), sizeof(send_buffer));

		std::copy(req.response_buffer.begin(),
				req.response_buffer.begin() + chunk_size,
				send_buffer);

		ssize_t written = write(fd, send_buffer, chunk_size);

		if (written > 0)
		{
			req.response_buffer.erase(
				req.response_buffer.begin(),
				req.response_buffer.begin() + written
			);
			if (req.response_buffer.empty())
				delFromEpoll(epoll_fd, fd);
		}
		else if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
			delFromEpoll(epoll_fd, fd);
	}

	return true;
}

bool Server::handleCGIEvent(int epoll_fd, int fd, uint32_t ev)
{
	int client_fd = globalFDS.svFD_to_clFD_map[fd];
	RequestState &req = globalFDS.request_state_map[client_fd];

	if (ev & EPOLLIN)
		cgiHandler->handleCGIRead(epoll_fd, fd);

	if (!processMethod(req, epoll_fd))
		return true;

	if (fd == req.cgi_out_fd && (ev & EPOLLHUP))
	{
		if (!req.cgi_output_buffer.empty())
			finalizeCgiResponse(req, epoll_fd, client_fd);
		cgiHandler->cleanupCGI(req);
	}

	if (fd == req.cgi_in_fd)
	{
		if (ev & EPOLLOUT)
			cgiHandler->handleCGIWrite(epoll_fd, fd, ev);

		if (ev & (EPOLLHUP | EPOLLERR))
		{
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
			close(fd);
			req.cgi_in_fd = -1;
			globalFDS.svFD_to_clFD_map.erase(fd);
		}
	}

	return true;
}

void Server::finalizeCgiResponse(RequestState &req, int epoll_fd, int client_fd)
{
	std::string output(req.cgi_output_buffer.begin(), req.cgi_output_buffer.end());
	size_t header_end = output.find("\r\n\r\n");
	std::string response;

	// Baue die Response basierend auf CGI-Output
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

	// Überprüfe die Größe der finalen Response
	if (response.length() > req.response_buffer.max_size())
	{
		// Wenn die Response zu groß ist, sende eine Fehlermeldung
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
		// Sichere Zuweisung der Response
		req.response_buffer.clear();
		req.response_buffer.insert(req.response_buffer.begin(),
								response.begin(),
								response.end());
	}

	req.state = RequestState::STATE_SENDING_RESPONSE;
	modEpoll(epoll_fd, client_fd, EPOLLOUT);
}

bool Server::processMethod(RequestState &req, int epoll_fd)
{
	if (req.request_buffer.empty())
		return true;

	std::string method = requestHandler->getMethod(req.request_buffer);
	if (!isMethodAllowed(req, method))
	{
		std::string error_response = getErrorHandler()->generateErrorResponse(405, "Method Not Allowed", req);

		// Überprüfe die Größe der Error-Response
		if (error_response.length() > req.response_buffer.max_size())
		{
			// Fallback zu einer minimalen Error-Response
			error_response =
				"HTTP/1.1 405 Method Not Allowed\r\n"
				"Content-Type: text/plain\r\n"
				"Content-Length: 18\r\n"
				"Connection: close\r\n\r\n"
				"Method not allowed.";
		}

		req.response_buffer.clear();
		req.response_buffer.insert(req.response_buffer.begin(),
								error_response.begin(),
								error_response.end());

		req.state = RequestState::STATE_SENDING_RESPONSE;
		modEpoll(epoll_fd, req.client_fd, EPOLLOUT);
		return false;
	}
	return true;
}
