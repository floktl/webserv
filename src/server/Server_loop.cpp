#include "Server.hpp"

int Server::runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs)
{
	const int max_events = 64;
	struct epoll_event events[max_events];
	const int timeout_ms = 5000;

	while (true)
	{
		int n = epoll_wait(epoll_fd, events, max_events, timeout_ms);

		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			break;
		}
		else if (n == 0)
		{
			// handle timeout
			continue;
		}

		for (int i = 0; i < n; i++)
		{
			int fd = events[i].data.fd;
			uint32_t ev = events[i].events;

			dispatchEvent(epoll_fd, fd, ev, configs);
		}
	}
	close(epoll_fd);
	return EXIT_SUCCESS;
}

bool Server::dispatchEvent(int epoll_fd, int fd, uint32_t ev, std::vector<ServerBlock> &configs)
{
	if (const ServerBlock* conf = findServerBlock(configs, fd))
	{
		handleNewConnection(epoll_fd, fd, *conf);
		return true;
	}

	if (globalFDS.svFD_to_clFD_map.find(fd) != globalFDS.svFD_to_clFD_map.end())
	{
		handleCGIEvent(epoll_fd, fd, ev);
		return true;
	}

	if (globalFDS.request_state_map.find(fd) != globalFDS.request_state_map.end())
	{

		handleClientEvent(epoll_fd, fd, ev);
		return true;
	}

	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
	return false;
}

void Server::handleNewConnection(int epoll_fd, int fd, const ServerBlock &conf)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(fd, (struct sockaddr*)&client_addr, &client_len);

	if (client_fd < 0)
	{
		return;
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
}
