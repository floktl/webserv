#include "Server.hpp"

int Server::runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs)
{
	struct epoll_event events[MAX_EVENTS];
	int incoming_fd = -1;
	int server_fd = -1;
	int client_fd = -1;
	int eventNum;

	while (!g_shutdown_requested)
	{
		eventNum = epoll_wait(epoll_fd, events, MAX_EVENTS, TIMEOUT_MS);
		if (eventNum < 0)
		{
			if (errno == EINTR)
				break;
			Logger::errorLog("Epoll error: " + std::string(strerror(errno)));
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

			if (globalFDS.cgi_pipe_to_client_fd.find(incoming_fd) != globalFDS.cgi_pipe_to_client_fd.end()) {
				handleCgiPipeEvent(incoming_fd);
				continue;
			}

			auto ctx_iter = globalFDS.context_map.find(incoming_fd);
			bool is_active_upload = false;

			if (ctx_iter != globalFDS.context_map.end())
			{
				Context& ctx = ctx_iter->second;
				if (isMultipart(ctx) && ctx.req.parsing_phase == RequestBody::PARSING_BODY &&
						!ctx.req.is_upload_complete)
					is_active_upload = true;
			}

			if (!is_active_upload && findServerBlock(configs, incoming_fd))
			{
				server_fd = incoming_fd;
				acceptNewConnection(epoll_fd, server_fd, configs);
			}
			else
			{
				client_fd = incoming_fd;
				handleAcceptedConnection(epoll_fd, client_fd, events[eventIter].events, configs);
			}
		}
		//checkAndCleanupTimeouts();
	}
	close(epoll_fd);
	epoll_fd = -1;
	return EXIT_SUCCESS;
}


// Runs the Main Event Loop, Handling Incoming Connections and Processing Events via Epoll
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

// Handles to Existing Client Connection by Processing Read/Write Events and Error Handling
bool Server::handleAcceptedConnection(int epoll_fd, int client_fd, uint32_t ev, std::vector<ServerBlock> &configs)
{
	std::map<int, Context>::iterator contextIter = globalFDS.context_map.find(client_fd);
	bool		status = false;
	if (contextIter != globalFDS.context_map.end())
	{
		Context		&ctx = contextIter->second;
		ctx.last_activity = std::chrono::steady_clock::now();
		if (ev & EPOLLIN){
			Logger::white("EPOLLIN");
			status = handleRead(ctx, configs);
		}
		if ((ev & EPOLLOUT))
		{
			Logger::white("EPOLLOUT");
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
			return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		return status;
	}
	return true;
}

// Handles Reading Request Data from the Client and Processing It Accordingly
bool Server::handleRead(Context& ctx, std::vector<ServerBlock>& configs)
{
	Logger::green("handleRead " + std::to_string(ctx.client_fd));
	char buffer[DEFAULT_REQUESTBUFFER_SIZE];

	if (!ctx.is_multipart || ctx.req.parsing_phase != RequestBody::PARSING_BODY)
		ctx.read_buffer.clear();
	//Logger::red("ctx.write_buffer.clear(); handleRead");
	ctx.write_buffer.clear();

	std::memset(buffer, 0, sizeof(buffer));
	//Logger::magenta("read handleRead");
	ssize_t bytes = read(ctx.client_fd, buffer, sizeof(buffer));
	if (bytes < 0)
	{
		if (ctx.multipart_fd_up_down > 0)
		{
			close(ctx.multipart_fd_up_down);
			ctx.multipart_fd_up_down = -1;
			//Logger::red("ctx.write_buffer.clear(); handleRead 2");
			ctx.write_buffer.clear();
		}
		return Logger::errorLog("Read error");
	}
	if (bytes == 0 && !ctx.req.is_upload_complete)
	{
		Logger::green("return truew 168");
		return true;
	}

	ctx.last_activity = std::chrono::steady_clock::now();
	if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE && ctx.multipart_fd_up_down < 0)
		resetContext(ctx);
	ctx.read_buffer = std::string(buffer, bytes);

	if (!ctx.headers_complete)
	{
		if (!parseBareHeaders(ctx, configs))
			return Logger::errorLog("Header parsing failed");
		if (!determineType(ctx, configs))
			return Logger::errorLog("Failed to determine request type on port: " + std::to_string(ctx.port));
	}
	Logger::yellow("headers_complete " + std::to_string(ctx.headers_complete));
	Logger::yellow("is_multipart " + std::to_string(ctx.is_multipart));
	Logger::yellow("ready_for_ping_pong " + std::to_string(ctx.ready_for_ping_pong));
	if (ctx.headers_complete && ctx.is_multipart && !ctx.ready_for_ping_pong
		&& (!parseContentDisposition(ctx) || !prepareUploadPingPong(ctx)))
			return Logger::errorLog("return at preppingpiong 189");

	if (ctx.headers_complete && ctx.ready_for_ping_pong)
	{
		handleSessionCookies(ctx);
		if (ctx.method == "GET" || ctx.method == "DELETE")
			return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
	}

	if (ctx.type == CGI && !ctx.is_multipart)
	{
		if (ctx.method == "POST" && !ctx.is_multipart && parseCGIBody(ctx))
				return true;
		return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
	}

	if (ctx.is_download && ctx.multipart_fd_up_down > 0)
		return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
	if (ctx.is_multipart && ctx.multipart_fd_up_down > 0 && ctx.multipart_file_path_up_down != ""
			&& ctx.headers_complete && ctx.ready_for_ping_pong)
	{
		ctx.final_boundary_found = (ctx.read_buffer.find(ctx.boundary + "--") != std::string::npos);
		extractFileContent(ctx.boundary, ctx.read_buffer, ctx.write_buffer, ctx);
		return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
	}
	Logger::green("true 214");
	return true;
}

bool Server::handleWrite(Context& ctx)
{
	Logger::green("handleWrite");
	bool result = false;

	if (ctx.is_download && ctx.multipart_fd_up_down > 0)
		return buildDownloadResponse(ctx);
	if (ctx.is_multipart && ctx.multipart_fd_up_down > 0 && !ctx.multipart_file_path_up_down.empty() &&
		ctx.headers_complete && ctx.ready_for_ping_pong)
	{

		bool upload_complete = false;

		if (ctx.final_boundary_found || (ctx.req.expected_body_length > 0
				&& ctx.req.current_body_length >= ctx.req.expected_body_length))
			upload_complete = true;

		if (!ctx.write_buffer.empty())
		{
			//Logger::magenta("write handleWrite");
			ssize_t written = write(ctx.multipart_fd_up_down, ctx.write_buffer.data(), ctx.write_buffer.size());
			if (written < 0)
			{
				Logger::errorLog("Error writing to file: " + std::string(strerror(errno)));
				close(ctx.multipart_fd_up_down);
				ctx.multipart_fd_up_down = -1;
				return false;
			}

			if (written > 0)
			{
				ctx.req.current_body_length += written;
				Logger::progressBar(ctx.req.current_body_length, ctx.req.expected_body_length, "(" + std::to_string(ctx.multipart_fd_up_down) + ") Upload 8");
				//Logger::red("ctx.write_buffer.clear(); handleWrite");
				ctx.write_buffer.clear();
			}

			if (ctx.final_boundary_found
				|| (ctx.req.expected_body_length > 0 && ctx.req.current_body_length >= ctx.req.expected_body_length))
			{
				upload_complete = true;
				Logger::progressBar(ctx.req.expected_body_length, ctx.req.expected_body_length, "("
						+ std::to_string(ctx.multipart_fd_up_down) + ") Upload 8");
			}
		}

		if (!upload_complete)
			return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);

		close(ctx.multipart_fd_up_down);
		ctx.multipart_fd_up_down = -1;
		ctx.req.is_upload_complete = true;

		ctx.type = REDIRECT;
		return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
	}

	switch (ctx.type)
	{
		case INITIAL:
			result = true;
			break;
		case STATIC:
			result = staticHandler(ctx);
			break;
		case REDIRECT:
			result = redirectAction(ctx);
			break;
		case CGI:
			if(!ctx.cgi_executed)
			{
				result = executeCgi(ctx);
				if (result)
				{
					ctx.cgi_output_phase = true;
					return result;
				}
			}
			result = sendCgiResponse(ctx);
			if (ctx.cgi_terminated)
				delFromEpoll(ctx.epoll_fd, ctx.client_fd);
			if (result)
				return result;
			break;
		case ERROR:
			return getErrorHandler()->generateErrorResponse(ctx);
	}

	if (ctx.error_code != 0)
		return getErrorHandler()->generateErrorResponse(ctx);
	if (result)
	{
		ctx.doAutoIndex = "";
		if (ctx.keepAlive)
			return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
		else
			result = false;
	}
	if (!result)
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
	return result;
}
