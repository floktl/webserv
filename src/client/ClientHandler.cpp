#include "ClientHandler.hpp"

ClientHandler::ClientHandler(Server& _server) : server(_server) {}

void ClientHandler::handleClientRead(int epfd, int fd)
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

    //const size_t MAX_REQUEST_SIZE = 8192;
    char buf[1024];

    while (true)
    {
        ssize_t n = read(fd, buf, sizeof(buf));

        if (n == 0)
        {
            std::cerr << "Client disconnected on fd: " << fd << std::endl;
            server.delFromEpoll(epfd, fd);
            return;
        }

        if (n < 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
            {
                // No more data to read; return and wait for next event
                break;
            }
            else
            {
                perror("read");
                server.delFromEpoll(epfd, fd);
                return;
            }
        }

        //if (req.request_buffer.size() + n > MAX_REQUEST_SIZE)
        //{
        //    std::cerr << "Request too large for fd: " << fd << std::endl;
        //    server.delFromEpoll(epfd, fd);
        //    return;
        //}

        // Append data to the request buffer
        req.request_buffer.insert(req.request_buffer.end(), buf, buf + n);

        // Check if we have received the headers
        if (req.request_buffer.size() > 4)
        {
            std::string req_str(req.request_buffer.begin(), req.request_buffer.end());
            size_t headers_end = req_str.find("\r\n\r\n");

            if (headers_end != std::string::npos)
            {
                try
                {
                    server.getRequestHandler()->parseRequest(req);

                    // If this is a POST request, ensure we have received the full body
					if (req.method == "POST" && req.received_body.size() < req.content_length)
					{
						std::cerr << "POST body incomplete. Waiting for more data. Received: "
								<< req.received_body.size() << ", Expected: " << req.content_length << std::endl;
						continue; // Keep reading
					}

                }
                catch (const std::exception &e)
                {
                    std::cerr << "Error parsing request on fd: " << fd << " - " << e.what() << std::endl;
                    server.delFromEpoll(epfd, fd);
                    return;
                }
            }
        }
    }
}



constexpr std::chrono::seconds RequestState::TIMEOUT_DURATION;

void ClientHandler::handleClientWrite(int epfd, int fd)
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

bool ClientHandler::processMethod(RequestState &req, int epoll_fd)
{
	if (req.request_buffer.empty())
		return true;

	std::string method = server.getRequestHandler()->getMethod(req.request_buffer);
	if (!isMethodAllowed(req, method))
	{
		std::string error_response = server.getErrorHandler()->generateErrorResponse(405, "Method Not Allowed", req);

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
		server.modEpoll(epoll_fd, req.client_fd, EPOLLOUT);
		return false;
	}
	return true;
}

bool ClientHandler::isMethodAllowed(const RequestState &req, const std::string &method) const
{
	const Location* loc = server.getRequestHandler()->findMatchingLocation(req.associated_conf, req.location_path);
	if (!loc)
		return false;

	if (method == "GET"    && loc->allowGet)    return true;
	if (method == "POST"   && loc->allowPost)   return true;
	if (method == "DELETE" && loc->allowDelete) return true;
	if (method == "COOKIE" && loc->allowCookie) return true;
	return false;
}