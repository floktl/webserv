#include "Server.hpp"

// Modifies the Epoll Event Set for A Given File Descriptor (FD), Adding Or Updating Events
bool Server::modEpoll(int epoll_fd, int fd, uint32_t events)
{
	if (fd <= 0)
	{
		Logger::errorLog("WARNING: Attempt to modify epoll for invalid fd: " + std::to_string(fd));
		return true;
	}
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = fd;

	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
	{
		if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0)
			return true;
	}
	return true;
}

// Removes A Client File Descriptor From the Epoll Instance and Close IT
bool Server::delFromEpoll(int epfd, int client_fd)
{
	if (epfd <= 0 || client_fd <= 0)
	{
		Logger::errorLog("WARNING: Attempt to delete invalid fd: " + std::to_string(client_fd));
		return true;
	}
	if (findServerBlock(configData, client_fd))
	{
		Logger::errorLog("Prevented removal of server socket: " + std::to_string(client_fd));
		return true;
	}
	auto ctx_it = globalFDS.context_map.find(client_fd);
	if (ctx_it != globalFDS.context_map.end())
	{
		epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
		globalFDS.clFD_to_svFD_map.erase(client_fd);
		close(client_fd);
		client_fd =-1;
		resetContext(globalFDS.context_map[client_fd]);
		globalFDS.context_map.erase(client_fd);
	}
	else
		globalFDS.clFD_to_svFD_map.erase(client_fd);
	return true;
}

// Searches for A Server Block Matching A Given File Descriptor
bool Server::findServerBlock(const std::vector<ServerBlock> &configs, int fd)
{
	for (const auto &conf : configs)
	{
		if (conf.server_fd == fd)
			return true;
	}
	return false;
}

// Sets a file descriptor to non-blocking fashion
int Server::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Checks and removes inactive connections that Extred the Timeout Threshold
void Server::checkAndCleanupTimeouts()
{
	auto now = std::chrono::steady_clock::now();

	auto it = globalFDS.context_map.begin();
	while (it != globalFDS.context_map.end())
	{
		Context& ctx = it->second;
		auto duration = std::chrono::duration_cast<std::chrono::seconds>(
			now - ctx.last_activity).count();

		if (duration > Context::TIMEOUT_DURATION.count())
		{
			delFromEpoll(ctx.epoll_fd, ctx.client_fd);
			it = globalFDS.context_map.erase(it);
		}
		else
			++it;
	}
}
