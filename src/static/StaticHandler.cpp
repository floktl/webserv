#include "StaticHandler.hpp"

StaticHandler::StaticHandler(Server& _server) : server(_server) {}

void StaticHandler::handleClientRead(int epfd, int fd)
{
    // Start handling a read request from the client

    // Get the associated request state for this client
    RequestState &req = server.getGlobalFds().request_state_map[fd];

    // Read up to 1024 bytes from the client socket
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf));

    // If the client closed the connection, clean up and remove it from epoll
    if (n == 0)
	{
        server.delFromEpoll(epfd, fd);
        return;
    }

    // If an error occurred, remove the client unless it's a recoverable error (EAGAIN or EWOULDBLOCK)
    if (n < 0)
	{
        if (errno != EAGAIN && errno != EWOULDBLOCK)
            server.delFromEpoll(epfd, fd);
        return;
    }

    // Successfully read data; append it to the request buffer
    req.request_buffer.insert(req.request_buffer.end(), buf, buf + n);

    // Check if the request buffer contains a complete HTTP request (indicated by "\r\n\r\n")
    if (req.request_buffer.size() > 4) {
        std::string req_str(req.request_buffer.begin(), req.request_buffer.end());
        if (req_str.find("\r\n\r\n") != std::string::npos) {
            // A complete request has been received; start parsing it
            server.getRequestHandler()->parseRequest(req);
        }
    }
}


constexpr std::chrono::seconds RequestState::TIMEOUT_DURATION;

void StaticHandler::handleClientWrite(int epfd, int fd)
{
    // Get the associated request state for this client
    RequestState &req = server.getGlobalFds().request_state_map[fd];

    // Check if the client connection has timed out
    auto now = std::chrono::steady_clock::now();
    if (now - req.last_activity > RequestState::TIMEOUT_DURATION)
	{
        // Timeout occurred; remove client from epoll
        server.delFromEpoll(epfd, fd);
        return;
    }

    // Handle sending the response if the request state indicates it's ready to send
    if (req.state == RequestState::STATE_SENDING_RESPONSE)
	{
        // Check for socket errors before sending data
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0)
		{
            // If there's a socket error, clean up and remove the client
            server.delFromEpoll(epfd, fd);
            return;
        }

        // Attempt to send the response buffer data to the client
        ssize_t n = send(fd, req.response_buffer.data(), req.response_buffer.size(), MSG_NOSIGNAL);

        if (n > 0)
		{
            // Successfully sent data; remove the sent portion from the buffer
            req.response_buffer.erase(
                req.response_buffer.begin(),
                req.response_buffer.begin() + n);

            // Update the last activity timestamp
            req.last_activity = now;

            // If the buffer is empty, the response is complete; remove client from epoll
            if (req.response_buffer.empty())
                server.delFromEpoll(epfd, fd);
            else  // Otherwise, continue monitoring for write readiness
                server.modEpoll(epfd, fd, EPOLLOUT);
        }
		else if (n < 0)
		{
            // If send failed and the error is not recoverable, clean up and remove the client
            if (errno != EAGAIN && errno != EWOULDBLOCK)
                server.delFromEpoll(epfd, fd);
            else // Continue monitoring for write readiness on recoverable errors
                server.modEpoll(epfd, fd, EPOLLOUT);
        }
    }
}
