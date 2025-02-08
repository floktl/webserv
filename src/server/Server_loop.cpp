#include "Server.hpp"

int Server::runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs)
{
	const int			max_events = 64;
	struct epoll_event	events[max_events];
	const int			timeout_ms = 1000;
	int					incoming_fd = -1;
	int					server_fd = -1;
	int					client_fd = -1;
	uint32_t			ev;
	int					eventNum;

	while (true)
	{
		eventNum = epoll_wait(epoll_fd, events, max_events, timeout_ms);
		if (eventNum < 0)
		{
			Logger::errorLog("Epoll error: " + std::string(strerror(errno)));
			if (errno == EINTR)
				continue;
			break;
		}
		else if (eventNum == 0)
		{
			checkAndCleanupTimeouts();
			continue;
		}

		for (int eventIter = 0; eventIter < eventNum; eventIter++)
		{
			incoming_fd = events[eventIter].data.fd;
			if (incoming_fd <= 0)
			{
				Logger::errorLog("WARNING: Invalid fd " + std::to_string(incoming_fd) + " in epoll event");
				continue;
			}
			ev = events[eventIter].events;
			if (findServerBlock(configs, incoming_fd))
			{
				server_fd = incoming_fd;
				acceptNewConnection(epoll_fd, server_fd, configs);
			}
			else
			{
				client_fd = incoming_fd;
				handleAcceptedConnection(epoll_fd, client_fd, ev, configs);
			}
		}
	}
	close(epoll_fd);
	epoll_fd = -1;
	return EXIT_SUCCESS;
}

bool Server::acceptNewConnection(int epoll_fd, int server_fd, std::vector<ServerBlock> &configs)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	Context ctx;

	if (server_fd <= 0)
		return false;
	while (true)
	{
		int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
		if (client_fd <= 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			else
				return false;
		}

		if (setNonBlocking(client_fd) < 0)
		{
			Logger::errorLog("Failed to set non-blocking mode for client_fd: " + std::to_string(client_fd));
			close(client_fd);
			client_fd = -1;
			continue;
		}
		modEpoll(epoll_fd, client_fd, EPOLLIN | EPOLLET);
		globalFDS.clFD_to_svFD_map[client_fd] = server_fd;
		ctx.server_fd = server_fd;
		ctx.client_fd = client_fd;
		ctx.epoll_fd = epoll_fd;
		ctx.type = RequestType::INITIAL;
		ctx.last_activity = std::chrono::steady_clock::now();
		ctx.doAutoIndex = "";
		ctx.keepAlive = true;
		ctx.error_code = 0;
		globalFDS.context_map[client_fd] = ctx;
		getMaxBodySizeFromConfig(ctx, configs);
	}
	return true;
}

bool Server::handleAcceptedConnection(int epoll_fd, int client_fd, uint32_t ev, std::vector<ServerBlock> &configs)
{
	std::map<int, Context>::iterator contextIter = globalFDS.context_map.find(client_fd);
	bool		status = false;

	if (contextIter != globalFDS.context_map.end())
	{
		Context		&ctx = contextIter->second;
		ctx.last_activity = std::chrono::steady_clock::now();
		if (ev & EPOLLIN)
		{
			status = handleRead(ctx, configs);
			if (status == false && ctx.error_code != 0)
				return (getErrorHandler()->generateErrorResponse(ctx));
		}
		if ((ev & EPOLLOUT))
		{
			status = handleWrite(ctx);
			if (status == false)
				delFromEpoll(epoll_fd, client_fd);
			return status;
		}
		if (ev & (EPOLLHUP | EPOLLERR))
		{
			status = false;
			delFromEpoll(epoll_fd, client_fd);
		}
		if (status == false && ctx.error_code != 0)
			return (getErrorHandler()->generateErrorResponse(ctx));
		return status;
	}
	return true;
}

bool Server::handleWrite(Context& ctx)
{
	bool result = false;

	switch (ctx.type)
	{
		case INITIAL:
			break;
		case STATIC:
			result = staticHandler(ctx);
			if (ctx.error_code != 0)
				result = getErrorHandler()->generateErrorResponse(ctx);
			break;
		case CGI:
			result = true;
			if (ctx.error_code != 0)
				result = getErrorHandler()->generateErrorResponse(ctx);
			break;
		case ERROR:
			result = getErrorHandler()->generateErrorResponse(ctx);
			break;
		default:
			break;
	}

	if (result)
	{
		ctx.input_buffer.clear();
		ctx.body.clear();
		ctx.headers_complete = false;
		ctx.content_length = 0;
		ctx.body_received = 0;
		ctx.headers.clear();
		ctx.error_code = 0;
		if (ctx.keepAlive)
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
		else
			result = false;
	}
	if (result == false)
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
	return result;
}

bool Server::handleRead(Context& ctx, std::vector<ServerBlock>& configs)
{
    char buffer[DEFAULT_REQUESTBUFFER_SIZE];
    ssize_t bytes = read(ctx.client_fd, buffer, sizeof(buffer));

    if (bytes < 0)
	{
        if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
            Logger::errorLog("Read error: " + std::string(strerror(errno)));
            return false;
        }
        return true;
    }

    if (bytes == 0)
        return true;

    ctx.last_activity = std::chrono::steady_clock::now();

    if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE)
        resetContext(ctx);

    ctx.input_buffer.append(buffer, bytes);

    if (!handleParsingPhase(ctx, configs))
        return false;

    if (ctx.req.parsing_phase == RequestBody::PARSING_BODY &&
        ctx.req.current_body_length < ctx.req.expected_body_length)
	{
        modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
        return true;
    }
    return finalizeRequest(ctx, configs);
}
