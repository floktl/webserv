#include "Server.hpp"

bool Server::handleNewConnection(int epoll_fd, int fd, const ServerBlock &conf)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(fd, (struct sockaddr*)&client_addr, &client_len);

	if (client_fd < 0)
	{
		return true;
	}

	setNonBlocking(client_fd);
	modEpoll(epoll_fd, client_fd, EPOLLIN);

	RequestState req;
	req.client_fd = client_fd;
	req.cgi_in_fd = -1;
	req.cgi_out_fd = -1;
	req.cgi_pid = -1;
	req.state = RequestState::STATE_READING_REQUEST;
	req.cgi_done = false;
	req.associated_conf = &conf;
	globalFDS.request_state_map[client_fd] = req;

	return true;
}

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
		clientHandler->handleClientRead(epoll_fd, fd);
		if (!clientHandler->processMethod(req, epoll_fd))
			return true;
	}

	if (req.state != RequestState::STATE_CGI_RUNNING &&
		req.state != RequestState::STATE_PREPARE_CGI &&
		(ev & EPOLLOUT))
	{
		clientHandler->handleClientWrite(epoll_fd, fd);
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

	if (!clientHandler->processMethod(req, epoll_fd))
		return true;

	if (fd == req.cgi_out_fd && (ev & EPOLLHUP))
	{
		if (!req.cgi_output_buffer.empty())
			cgiHandler->finalizeCgiResponse(req, epoll_fd, client_fd);
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
