#include "StaticHandler.hpp"

StaticHandler::StaticHandler(Server& _server) : server(_server) {}

void StaticHandler::handleClientRead(int epfd, int fd) {
	std::stringstream ss;
	ss << "Handling client read on fd " << fd;
	//Logger::file(ss.str());

	RequestState &req = server.getGlobalFds().request_state_map[fd];
	char buf[1024];
	ssize_t n = read(fd, buf, sizeof(buf));

	if (n == 0) {
		//Logger::file("Client closed connection");
		server.delFromEpoll(epfd, fd);
		return;
	}

	if (n < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			//Logger::file("Read error: " + std::string(strerror(errno)));
			server.delFromEpoll(epfd, fd);
		}
		return;
	}

	ss.str("");
	ss << "Read " << n << " bytes from client";
	//Logger::file(ss.str());

	req.request_buffer.insert(req.request_buffer.end(), buf, buf + n);

	if (req.request_buffer.size() > 4) {
		std::string req_str(req.request_buffer.begin(), req.request_buffer.end());
		if (req_str.find("\r\n\r\n") != std::string::npos) {
			//Logger::file("Complete request received, parsing");
			server.getRequestHandler()->parseRequest(req);

			// if (!server.getCgiHandler()->needsCGI(req.associated_conf, req.requested_path))
			// {
			// 	//Logger::file("Processing as normal request");
			// 	server.getRequestHandler()->buildResponse(req);
			// 	req.state = RequestState::STATE_SENDING_RESPONSE;
			// 	req.last_activity = std::chrono::steady_clock::now();
			// 	server.modEpoll(epfd, fd, EPOLLOUT);
			// }
			// else
			// {
			// 	//Logger::file("Request requires CGI processing");
			// }
		}
	}
}

constexpr std::chrono::seconds RequestState::TIMEOUT_DURATION;

void StaticHandler::handleClientWrite(int epfd, int fd)
{
	std::stringstream ss;
	ss << "Handling client write on fd " << fd;
	//Logger::file(ss.str());

	RequestState &req = server.getGlobalFds().request_state_map[fd];

	// Check for timeout
	auto now = std::chrono::steady_clock::now();
	if (now - req.last_activity > RequestState::TIMEOUT_DURATION)
	{
		//Logger::file("Timeout detected for fd " + std::to_string(fd) + ", closing connection.");
		server.delFromEpoll(epfd, fd);
		return;
	}

	if (req.state == RequestState::STATE_SENDING_RESPONSE)
	{
		ss.str("");
		ss << "Attempting to write response, buffer size: " << req.response_buffer.size()
			<< ", content: " << std::string(req.response_buffer.begin(), req.response_buffer.end());
		//Logger::file(ss.str());

		int error = 0;
		socklen_t len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0)
		{
			//Logger::file("Socket error detected: " + std::string(strerror(error)));
			server.delFromEpoll(epfd, fd);
			return;
		}

		ssize_t n = send(fd, req.response_buffer.data(), req.response_buffer.size(), MSG_NOSIGNAL);

		if (n > 0)
		{
			ss.str("");
			ss << "Successfully wrote " << n << " bytes to client";
			//Logger::file(ss.str());

			req.response_buffer.erase(
				req.response_buffer.begin(),
				req.response_buffer.begin() + n
			);

			req.last_activity = now; // Update last activity timestamp

			if (req.response_buffer.empty())
			{
				//Logger::file("Response fully sent, closing connection");
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
				ss.str("");
				ss << "Write error: " << strerror(errno);
				//Logger::file(ss.str());
				server.delFromEpoll(epfd, fd);
			}
			else
			{
				server.modEpoll(epfd, fd, EPOLLOUT);
			}
		}
	}
}
