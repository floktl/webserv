#include "StaticHandler.hpp"

StaticHandler::StaticHandler(Server& _server) : server(_server) {}

void StaticHandler::handleClientRead(int epfd, int fd)
{
	auto &fds = server.getGlobalFds();
	auto it = fds.request_state_map.find(fd);

	if (it == fds.request_state_map.end())
	{
		std::cerr << "Invalid fd: " << fd << " not found in request_state_map" << std::endl;
		server.delFromEpoll(epfd, fd);
		return;
	}

	RequestState &req = it->second;

	char buf[1024];
	ssize_t n = read(fd, buf, sizeof(buf));

	if (n == 0)
	{
		// Client disconnected
		std::cerr << "Client disconnected on fd: " << fd << std::endl;
		server.delFromEpoll(epfd, fd);
		return;
	}

	if (n < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			perror("read");
			server.delFromEpoll(epfd, fd);
		}
		return;
	}

	const size_t MAX_REQUEST_SIZE = 8192;
	if (req.request_buffer.size() + n > MAX_REQUEST_SIZE)
	{
		std::cerr << "Request too large for fd: " << fd << std::endl;
		server.delFromEpoll(epfd, fd);
		return;
	}

	req.request_buffer.insert(req.request_buffer.end(), buf, buf + n);

	if (req.request_buffer.size() > 4)
	{
		std::string req_str(req.request_buffer.begin(), req.request_buffer.end());
		size_t headers_end = req_str.find("\r\n\r\n");

		if (headers_end != std::string::npos)
		{
			try
			{
				server.getRequestHandler()->parseRequest(req);
			}
			catch (const std::exception &e)
			{
				std::cerr << "Error parsing request on fd: " << fd << " - " << e.what() << std::endl;
				server.delFromEpoll(epfd, fd);
			}
		}
	}
}


constexpr std::chrono::seconds RequestState::TIMEOUT_DURATION;

void StaticHandler::handleClientWrite(int epfd, int fd)
{
	RequestState &req = server.getGlobalFds().request_state_map[fd];

	// Check for timeout
	auto now = std::chrono::steady_clock::now();
	if (now - req.last_activity > RequestState::TIMEOUT_DURATION)
	{
		server.delFromEpoll(epfd, fd);
		return;
	}

	if (req.state == RequestState::STATE_SENDING_RESPONSE)
	{
		int error = 0;
		socklen_t len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0)
		{
			server.delFromEpoll(epfd, fd);
			return;
		}

		if (!req.response_buffer.empty())
		{
			// Create a temporary buffer for sending
			char send_buffer[8192];
			size_t chunk_size = std::min(req.response_buffer.size(), sizeof(send_buffer));

			// Copy data from deque to send buffer
			std::copy(req.response_buffer.begin(),
					req.response_buffer.begin() + chunk_size,
					send_buffer);

			ssize_t n = send(fd, send_buffer, chunk_size, MSG_NOSIGNAL);

			if (n > 0)
			{
				req.response_buffer.erase(
					req.response_buffer.begin(),
					req.response_buffer.begin() + n
				);

				req.last_activity = now;

				if (req.response_buffer.empty())
				{
					server.delFromEpoll(epfd, fd);
				}
				else
				{
					server.modEpoll(epfd, fd, EPOLLOUT);
				}
			}
			else if (n < 0)
			{
				if (errno != EAGAIN && errno != EWOULDBLOCK)
				{
					server.delFromEpoll(epfd, fd);
				}
				else
				{
					server.modEpoll(epfd, fd, EPOLLOUT);
				}
			}
		}
	}
}
