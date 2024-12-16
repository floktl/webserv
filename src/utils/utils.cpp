/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/16 09:18:09 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/16 12:44:26 by fkeitel          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "utils.hpp"

int setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void modEpoll(int epfd, int fd, uint32_t events)
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


void delFromEpoll(int epfd, int fd)
{
	std::stringstream ss;
	ss << "Removing fd " << fd << " from epoll";
	Logger::file(ss.str());

	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);

	std::map<int,int>::iterator it = globalFDS.svFD_to_clFD_map.find(fd);
	if (it != globalFDS.svFD_to_clFD_map.end())
	{
		int client_fd = it->second;
		RequestState &req = globalFDS.request_state_map[client_fd];

		ss.str("");
		ss << "Found associated client fd " << client_fd;
		Logger::file(ss.str());

		if (fd == req.cgi_in_fd)
		{
			Logger::file("Closing CGI input pipe");
			req.cgi_in_fd = -1;
		}
		if (fd == req.cgi_out_fd)
		{
			Logger::file("Closing CGI output pipe");
			req.cgi_out_fd = -1;
		}

		if (req.cgi_in_fd == -1 && req.cgi_out_fd == -1 &&
			req.state != RequestState::STATE_SENDING_RESPONSE)
		{
			Logger::file("Cleaning up request state");
			globalFDS.request_state_map.erase(client_fd);
		}

		globalFDS.svFD_to_clFD_map.erase(it);
	}
	else
	{
		RequestState &req = globalFDS.request_state_map[fd];
		Logger::file("Cleaning up client connection resources");

		if (req.cgi_in_fd != -1)
		{
			ss.str("");
			ss << "Cleaning up CGI input pipe " << req.cgi_in_fd;
			Logger::file(ss.str());
			epoll_ctl(epfd, EPOLL_CTL_DEL, req.cgi_in_fd, NULL);
			close(req.cgi_in_fd);
			globalFDS.svFD_to_clFD_map.erase(req.cgi_in_fd);
		}
		if (req.cgi_out_fd != -1)
		{
			ss.str("");
			ss << "Cleaning up CGI output pipe " << req.cgi_out_fd;
			Logger::file(ss.str());
			epoll_ctl(epfd, EPOLL_CTL_DEL, req.cgi_out_fd, NULL);
			close(req.cgi_out_fd);
			globalFDS.svFD_to_clFD_map.erase(req.cgi_out_fd);
		}

		globalFDS.request_state_map.erase(fd);
	}

	close(fd);
	ss.str("");
	ss << "Closed fd " << fd;
	Logger::file(ss.str());
}
