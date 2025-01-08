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
		return (handleNewConnection(epoll_fd, fd, *conf));

	else if (globalFDS.svFD_to_clFD_map.find(fd) != globalFDS.svFD_to_clFD_map.end())
		return (handleCGIEvent(epoll_fd, fd, ev));

	else if (globalFDS.request_state_map.find(fd) != globalFDS.request_state_map.end())
		return (handleClientEvent(epoll_fd, fd, ev));

	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
	return (false);
}
