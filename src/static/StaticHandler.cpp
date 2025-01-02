#include "StaticHandler.hpp"

StaticHandler::StaticHandler(Server& _server) : server(_server) {}

void StaticHandler::handleClientRead(int epfd, int fd)
{
    RequestState &req = server.getGlobalFds().request_state_map[fd];
    char buf[1024];
    ssize_t n = read(fd, buf, sizeof(buf));

    if (n == 0)
    {
        server.delFromEpoll(epfd, fd);
        return;
    }

    if (n < 0)
    {
        if (errno != EAGAIN && errno != EWOULDBLOCK)
        {
            server.delFromEpoll(epfd, fd);
        }
        return;
    }

    req.request_buffer.insert(req.request_buffer.end(), buf, buf + n);

    if (req.request_buffer.size() > 4)
    {
        std::string req_str(req.request_buffer.begin(), req.request_buffer.end());
        if (req_str.find("\r\n\r\n") != std::string::npos)
        {
            Logger::file("parsing");
            server.getRequestHandler()->parseRequest(req);
            // If parseRequest transitions the state to STATE_HTTP_PROCESS, EPOLLOUT will handle it
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

    if (req.state == RequestState::STATE_HTTP_PROCESS || req.state == RequestState::STATE_SENDING_RESPONSE)
    {
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0)
        {
            server.delFromEpoll(epfd, fd);
            return;
        }

        ssize_t n = send(fd, req.response_buffer.data(), req.response_buffer.size(), MSG_NOSIGNAL);

        if (n > 0)
        {
            req.response_buffer.erase(
                req.response_buffer.begin(),
                req.response_buffer.begin() + n
            );

            req.last_activity = now; // Update last activity timestamp

            if (req.response_buffer.empty())
            {
                if (req.state == RequestState::STATE_HTTP_PROCESS)
                {
                    req.state = RequestState::STATE_SENDING_RESPONSE;
                }
                else
                {
                    server.delFromEpoll(epfd, fd);
                }
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

