/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:40:26 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/16 10:02:26 by fkeitel          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"

void handleClientRead(int epfd, int fd)
{
	std::stringstream ss;
	ss << "Handling client read on fd " << fd;
	Logger::file(ss.str());

	RequestState &req = g_requests[fd];
	char buf[1024];
	ssize_t n = read(fd, buf, sizeof(buf));

	if (n == 0)
	{
		Logger::file("Client closed connection");
		delFromEpoll(epfd, fd);
		return;
	}

	if (n < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			Logger::file("Read error: " + std::string(strerror(errno)));
			delFromEpoll(epfd, fd);
		}
		return;
	}

	ss.str("");
	ss << "Read " << n << " bytes from client";
	Logger::file(ss.str());

	req.request_buffer.insert(req.request_buffer.end(), buf, buf + n);

	if (req.request_buffer.size() > 4)
	{
		std::string req_str(req.request_buffer.begin(), req.request_buffer.end());
		if (req_str.find("\r\n\r\n") != std::string::npos) {
			Logger::file("Complete request received, parsing");
			parseRequest(req);

			if (!needsCGI(req.associated_conf, req.requested_path))
			{
				Logger::file("Processing as normal request");
				buildResponse(req);
				req.state = RequestState::STATE_SENDING_RESPONSE;
				modEpoll(epfd, fd, EPOLLOUT);
			}
			else
			{
				Logger::file("Request requires CGI processing");
			}
		}
	}
}


void handleClientWrite(int epfd, int fd)
{
	std::stringstream ss;
	ss << "Handling client write on fd " << fd;
	Logger::file(ss.str());

	RequestState &req = g_requests[fd];
	if (req.state == RequestState::STATE_SENDING_RESPONSE)
	{
		ss.str("");
		ss << "Attempting to write response, buffer size: " << req.response_buffer.size()
		<< ", content: " << std::string(req.response_buffer.begin(), req.response_buffer.end());
		Logger::file(ss.str());

		int error = 0;
		socklen_t len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0)
		{
			Logger::file("Socket error detected: " + std::string(strerror(error)));
			delFromEpoll(epfd, fd);
			return;
		}

		ssize_t n = send(fd, req.response_buffer.data(), req.response_buffer.size(), MSG_NOSIGNAL);

		if (n > 0)
		{
			ss.str("");
			ss << "Successfully wrote " << n << " bytes to client";
			Logger::file(ss.str());

			req.response_buffer.erase(
				req.response_buffer.begin(),
				req.response_buffer.begin() + n
			);

			if (req.response_buffer.empty())
			{
				Logger::file("Response fully sent, closing connection");
				delFromEpoll(epfd, fd);
			} else {
				modEpoll(epfd, fd, EPOLLOUT);
			}
		}
		else if (n < 0)
		{
			if (errno != EAGAIN && errno != EWOULDBLOCK)
			{
				ss.str("");
				ss << "Write error: " << strerror(errno);
				Logger::file(ss.str());
				delFromEpoll(epfd, fd);
			}
			else
			{
				modEpoll(epfd, fd, EPOLLOUT);
			}
		}
	}
}
