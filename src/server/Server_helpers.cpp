#include "Server.hpp"

void Server::modEpoll(int epfd, int fd, uint32_t events)
{
	struct epoll_event ev;
	std::memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.fd = fd;

	if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) < 0)
	{
		if (errno == ENOENT)
		{
			if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0)
			{
				Logger::file("epoll_ctl ADD failed: " + std::string(strerror(errno)));
			}
		}
		else
		{
			Logger::file("epoll_ctl MOD failed: " + std::string(strerror(errno)));
		}
	}
}

void Server::setTimeout(int t) {
	timeout = t;
}

int Server::getTimeout() const {
	return timeout;
}

void Server::delFromEpoll(int epfd, int fd)
{
	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);

	std::map<int,int>::iterator it = globalFDS.svFD_to_clFD_map.find(fd);
	if (it != globalFDS.svFD_to_clFD_map.end())
	{
		int client_fd = it->second;
		RequestState &req = globalFDS.request_state_map[client_fd];

		if (fd == req.cgi_in_fd)
		{
			req.cgi_in_fd = -1;
		}
		if (fd == req.cgi_out_fd)
		{
			req.cgi_out_fd = -1;
		}

		if (req.cgi_in_fd == -1 && req.cgi_out_fd == -1 &&
			req.state != RequestState::STATE_SENDING_RESPONSE)
		{
			globalFDS.request_state_map.erase(client_fd);
		}

		globalFDS.svFD_to_clFD_map.erase(it);
	}
	else
	{
		RequestState &req = globalFDS.request_state_map[fd];

		if (req.cgi_in_fd != -1)
		{
			epoll_ctl(epfd, EPOLL_CTL_DEL, req.cgi_in_fd, NULL);
			close(req.cgi_in_fd);
			globalFDS.svFD_to_clFD_map.erase(req.cgi_in_fd);
		}
		if (req.cgi_out_fd != -1)
		{
			epoll_ctl(epfd, EPOLL_CTL_DEL, req.cgi_out_fd, NULL);
			close(req.cgi_out_fd);
			globalFDS.svFD_to_clFD_map.erase(req.cgi_out_fd);
		}

		globalFDS.request_state_map.erase(fd);
	}

	close(fd);
}

bool Server::isMethodAllowed(const RequestState &req, const std::string &method) const
{
	const Location* loc = requestHandler->findMatchingLocation(req.associated_conf, req.location_path);
	if (!loc)
		return false;

	if (method == "GET"    && loc->allowGet)    return true;
	if (method == "POST"   && loc->allowPost)   return true;
	if (method == "DELETE" && loc->allowDelete) return true;
	if (method == "COOKIE" && loc->allowCookie) return true;
	return false;
}

const ServerBlock* Server::findServerBlock(const std::vector<ServerBlock> &configs, int fd)
{
	for (const auto &conf : configs)
	{
		if (conf.server_fd == fd)
			return &conf;
	}
	return nullptr;
}

int Server::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void Server::setTaskStatus(enum RequestState::Task new_task, int client_fd)
{
	if (client_fd < 0)
	{
		Logger::file("Error: Invalid client_fd " + std::to_string(client_fd));
		return;
	}

	auto it = globalFDS.request_state_map.find(client_fd);

	if (it != globalFDS.request_state_map.end())
	{
		it->second.task = new_task;

		std::string taskName = (new_task == RequestState::PENDING ? "PENDING" :
							new_task == RequestState::IN_PROGRESS ? "IN_PROGRESS" :
							new_task == RequestState::COMPLETED ? "COMPLETED" :
							"UNKNOWN");

	}
	else
	{
		Logger::file("Error: client_fd " + std::to_string(client_fd) +
					" not found in request_state_map");
	}
}


enum RequestState::Task Server::getTaskStatus(int client_fd)
{
	auto it = globalFDS.request_state_map.find(client_fd);

	if (it != globalFDS.request_state_map.end())
	{
		return it->second.task;
	}
	else
	{
		Logger::file("Error: client_fd " + std::to_string(client_fd) + " not found in request_state_map");
		return RequestState::Task::PENDING;
	}
}
