#include "Server.hpp"

int Server::runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs)
{
	struct epoll_event events[MAX_EVENTS];
	int incoming_fd = -1;
	int server_fd = -1;
	int client_fd = -1;
	int eventNum;

	while (!g_shutdown_requested) {
		eventNum = epoll_wait(epoll_fd, events, MAX_EVENTS, TIMEOUT_MS);
		if (eventNum < 0) {
			if (errno == EINTR)
				break;
			Logger::errorLog("Epoll error: " + std::string(strerror(errno)));
			break;
		}
		else if (eventNum == 0) {
			checkAndCleanupTimeouts();
			continue;
		}
		for (int eventIter = 0; eventIter < eventNum; eventIter++) {
			incoming_fd = events[eventIter].data.fd;

			auto ctx_iter = globalFDS.context_map.find(incoming_fd);
			bool is_active_upload = false;

			if (ctx_iter != globalFDS.context_map.end()) {
				Context& ctx = ctx_iter->second;
				if (isMultipart(ctx) &&
					ctx.req.parsing_phase == RequestBody::PARSING_BODY &&
					!ctx.req.is_upload_complete) {
					is_active_upload = true;
				}
			}

			if (!is_active_upload && findServerBlock(configs, incoming_fd)) {
				server_fd = incoming_fd;
				acceptNewConnection(epoll_fd, server_fd, configs);
			}
			else {
				client_fd = incoming_fd;
				handleAcceptedConnection(epoll_fd, client_fd, events[eventIter].events, configs);
			}
		}
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
		if (ev & EPOLLIN) {
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
		{
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
			return true;
		}
		return status;
	}
	return true;
}

bool Server::extractFileContent(const std::string& boundary, const std::string& buffer, std::vector<char>& output, Context& ctx)
{
	if (boundary.empty()) {
		Logger::errorLog("Empty boundary");
		return false;
	}

	std::string boundaryStr = boundary;
	std::string finalBoundaryStr = boundaryStr + "--";

	size_t boundaryPos = buffer.find(boundaryStr);
	size_t finalBoundaryPos = buffer.find(finalBoundaryStr);

	bool finalBoundaryFound = (finalBoundaryPos != std::string::npos);
	if (finalBoundaryFound) {
		ctx.req.is_upload_complete = true;
		ctx.final_boundary_found = true;
	}

	if (!ctx.found_first_boundary) {
		if (boundaryPos != std::string::npos) {
			ctx.found_first_boundary = true;

			size_t headers_end = buffer.find("\r\n\r\n", boundaryPos);
			if (headers_end != std::string::npos) {
				size_t content_start = headers_end + 4;

				if (finalBoundaryFound) {
					if (finalBoundaryPos > content_start) {
						size_t content_end = finalBoundaryPos;
						if (content_end >= 2 && buffer[content_end-1] == '\n' && buffer[content_end-2] == '\r') {
							content_end -= 2;
						}

						if (content_start < content_end) {
							output.insert(output.end(),
										buffer.begin() + content_start,
										buffer.begin() + content_end);
						}
					}
				}
				else if (content_start < buffer.size()) {
					output.insert(output.end(),
								buffer.begin() + content_start,
								buffer.end());
				}
			}
		}
		return true;
	}
	else {
		if (finalBoundaryFound) {
			size_t end_pos = finalBoundaryPos;

			if (end_pos >= 2 && buffer[end_pos-1] == '\n' && buffer[end_pos-2] == '\r') {
				end_pos -= 2;
			}

			if (end_pos > 0) {
				output.insert(output.end(),
							buffer.begin(),
							buffer.begin() + end_pos);
			}
		}
		else if (boundaryPos != std::string::npos) {
			if (boundaryPos > 0) {
				size_t end_pos = boundaryPos;

				if (end_pos >= 2 && buffer[end_pos-1] == '\n' && buffer[end_pos-2] == '\r') {
					end_pos -= 2;
				}

				output.insert(output.end(),
							buffer.begin(),
							buffer.begin() + end_pos);
			}

			size_t headers_end = buffer.find("\r\n\r\n", boundaryPos);
			if (headers_end != std::string::npos) {
				size_t content_start = headers_end + 4;

				if (content_start < buffer.size()) {
					output.insert(output.end(),
								buffer.begin() + content_start,
								buffer.end());
				}
			}
		}
		else {
			output.insert(output.end(), buffer.begin(), buffer.end());
		}
	}


	return true;
}

bool Server::parseCGIBody(Context& ctx)
{
	//Logger::cyan(std::string(ctx.read_buffer.begin(), ctx.read_buffer.end()));
	ctx.body += ctx.read_buffer;
	//Logger::magenta(ctx.body);

	bool finished = false;

	if (ctx.headers.count("Content-Length")) {
		unsigned long expected_length = std::stol(ctx.headers["Content-Length"]);
		if (ctx.body.length() >= expected_length) {
			finished = true;
			//Logger::green("Body complete based on Content-Length");
		}
	} else if (ctx.read_buffer.empty()) {
		finished = true;
		//Logger::green("Body complete (no Content-Length, empty buffer)");
	}

	if (finished) {
		//Logger::green("POST body complete, switching to CGI execution mode");
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		return true;
	} else {
		//Logger::green("Waiting for more POST data...");
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
		return true;
	}
}

// Handles Reading Request Data from the Client and Processing It Accordingly
bool Server::handleRead(Context& ctx, std::vector<ServerBlock>& configs) {
	Logger::green("handleRead");
	if (!ctx.is_multipart || ctx.req.parsing_phase != RequestBody::PARSING_BODY) {
		ctx.read_buffer.clear();
	}

	ctx.write_buffer.clear();

	char buffer[DEFAULT_REQUESTBUFFER_SIZE];
	std::memset(buffer, 0, sizeof(buffer));
	ssize_t bytes = read(ctx.client_fd, buffer, sizeof(buffer));

	if (bytes < 0) {
		if (ctx.multipart_fd_up_down > 0) {
			close(ctx.multipart_fd_up_down);
			ctx.multipart_fd_up_down = -1;
			ctx.write_buffer.clear();
		}
		return Logger::errorLog("Read error");
	}
	if (bytes == 0 && !ctx.req.is_upload_complete) {
		return true;
	}

	ctx.last_activity = std::chrono::steady_clock::now();
	if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE && ctx.multipart_fd_up_down < 0)
		resetContext(ctx);
	ctx.read_buffer = std::string(buffer, bytes);

	if (!ctx.headers_complete) {
		if (!parseBareHeaders(ctx, configs)) {
			Logger::errorLog("Header parsing failed");
			return false;
		}
		if (ctx.error_code)
			return false;
		if (!determineType(ctx, configs)) {
			Logger::errorLog("Failed to determine request type");
			return false;
		}
		if (ctx.error_code)
			return false;
	}

	if (ctx.headers_complete && ctx.is_multipart && !ctx.ready_for_ping_pong && ctx.type != CGI) {
		if (!parseContentDisposition(ctx)) {
			Logger::errorLog("Content Disposition parsing failed");
			return false;
		}
		if (ctx.error_code)
			return false;

		if (!ctx.multipart_file_path_up_down.empty()) {
			prepareUploadPingPong(ctx);
			if (ctx.error_code)
				return false;

			ctx.ready_for_ping_pong = true;
		}
	}

	if (ctx.headers_complete && ctx.ready_for_ping_pong) {
		handleSessionCookies(ctx);

		if (ctx.method == "GET" || ctx.method == "DELETE") {
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
			Logger::yellow("session cookiesssssssssssssssssssssssssssss");
			return true;
		}
	}

	Logger::magenta("BIER !!        !!!!!!! "+ ctx.path);
	if (ctx.type == CGI) {
		Logger::magenta("Wir sind im CGI Parsing");
		if (ctx.method == "POST")
		{
			Logger::magenta("Wir parsen den naechsten teil des Bodies in ctx. body");
			if (parseCGIBody(ctx)) {
				return true;
			}
			return updateErrorStatus(ctx, 500, "Internal Server Error");
		}
		Logger::magenta("Wir haben das CGI Parsing beendet und geben frei");
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		return true;
	}

	if (ctx.is_download && ctx.multipart_fd_up_down > 0) {
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		return true;
	}

	if (ctx.is_multipart && ctx.multipart_fd_up_down > 0 && ctx.multipart_file_path_up_down != "" && ctx.headers_complete && ctx.ready_for_ping_pong) {
		ctx.final_boundary_found = (ctx.read_buffer.find(ctx.boundary + "--") != std::string::npos);
		extractFileContent(ctx.boundary, ctx.read_buffer, ctx.write_buffer, ctx);
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		return true;
	}
	return false;
}

bool Server::parseContentDisposition(Context& ctx)
{
	if (!ctx.is_multipart) {
		return true;
	}

	if (ctx.boundary.empty()) {
		auto content_type_it = ctx.headers.find("Content-Type");
		if (content_type_it != ctx.headers.end()) {
			size_t boundary_pos = content_type_it->second.find("boundary=");
			if (boundary_pos != std::string::npos) {
				ctx.boundary = "--" + content_type_it->second.substr(boundary_pos + 9);
				size_t end_pos = ctx.boundary.find(';');
				if (end_pos != std::string::npos) {
					ctx.boundary = ctx.boundary.substr(0, end_pos);
				}
			}
		}
	}

	size_t boundary_start = ctx.read_buffer.find(ctx.boundary);
	if (boundary_start == std::string::npos) {
		return true;
	}

	size_t disp_pos = ctx.read_buffer.find("Content-Disposition: form-data", boundary_start);
	if (disp_pos == std::string::npos) {
		return true;
	}

	size_t filename_pos = ctx.read_buffer.find("filename=\"", disp_pos);
	if (filename_pos != std::string::npos) {
		filename_pos += 10;
		size_t filename_end = ctx.read_buffer.find("\"", filename_pos);
		if (filename_end != std::string::npos) {
			std::string filename = ctx.read_buffer.substr(filename_pos, filename_end - filename_pos);
			ctx.headers["Content-Disposition-Filename"] = filename;
			ctx.multipart_file_path_up_down = concatenatePath(ctx.location.upload_store, filename);

			size_t headers_end = ctx.read_buffer.find("\r\n\r\n", disp_pos);
			if (headers_end != std::string::npos) {
				ctx.header_offset = headers_end + 4;
			} else {
				ctx.header_offset = 0;
			}
		}
	}

	return true;
}

void Server::handleSessionCookies(Context& ctx) {
	bool hasExistingSession = false;
	for (const auto& cookie : ctx.cookies) {
		if (cookie.first == "WEBSERV_SESSION") {
			hasExistingSession = true;
			break;
		}
	}

	if (!hasExistingSession) {
		std::stringstream sessionId;
		sessionId << "s" << ctx.server_fd
				<< "c" << ctx.client_fd
				<< "t" << std::chrono::duration_cast<std::chrono::milliseconds>(
						ctx.last_activity.time_since_epoch()).count();
		ctx.setCookies.push_back(std::make_pair("WEBSERV_SESSION", sessionId.str()));
	}
}



bool Server::handleWrite(Context& ctx) {
	Logger::green("handleWrite");
	bool result = false;

	if (ctx.is_download && ctx.multipart_fd_up_down > 0) {

		return buildDownloadResponse(ctx);
	}

	if (ctx.is_multipart && ctx.multipart_fd_up_down > 0 && !ctx.multipart_file_path_up_down.empty() &&
		ctx.headers_complete && ctx.ready_for_ping_pong) {

		bool upload_complete = false;

		if (ctx.final_boundary_found ||
			(ctx.req.expected_body_length > 0 && ctx.req.current_body_length >= ctx.req.expected_body_length)) {
			upload_complete = true;
		}

		if (!ctx.write_buffer.empty()) {
			//Logger::magenta("write in handleWrite");
			ssize_t written = write(ctx.multipart_fd_up_down, ctx.write_buffer.data(), ctx.write_buffer.size());
			if (written < 0) {
				Logger::errorLog("Error writing to file: " + std::string(strerror(errno)));
				close(ctx.multipart_fd_up_down);
				ctx.multipart_fd_up_down = -1;
				return false;
			}

			if (written > 0) {
				ctx.req.current_body_length += written;

				Logger::progressBar(ctx.req.current_body_length, ctx.req.expected_body_length, "(" + std::to_string(ctx.multipart_fd_up_down) + ") Upload 8");

				ctx.write_buffer.clear();
			}

			if (ctx.final_boundary_found ||
				(ctx.req.expected_body_length > 0 && ctx.req.current_body_length >= ctx.req.expected_body_length)) {
				upload_complete = true;
				Logger::progressBar(ctx.req.expected_body_length, ctx.req.expected_body_length, "(" + std::to_string(ctx.multipart_fd_up_down) + ") Upload 8");
			}
		}

		if (!upload_complete) {
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
			return true;
		}

		close(ctx.multipart_fd_up_down);
		ctx.multipart_fd_up_down = -1;
		ctx.req.is_upload_complete = true;

		ctx.type = REDIRECT;
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		return true;
	}

	switch (ctx.type) {
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
			Logger::magenta(std::to_string(ctx.cgi_terminated));
			if (ctx.cgi_terminated)
			{
				delFromEpoll(ctx.epoll_fd, ctx.client_fd);
			}
			if (result)
			{
				return result;
			}
			break;
		case ERROR:
			return getErrorHandler()->generateErrorResponse(ctx);
	}

	if (ctx.error_code != 0)
		return getErrorHandler()->generateErrorResponse(ctx);

	if (result) {
		ctx.doAutoIndex = "";
		if (ctx.keepAlive)
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
		else
			result = false;
	}

	if (!result)
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
	return result;
}

bool Server::buildDownloadResponse(Context &ctx) {
	struct stat file_stat;
	if (stat(ctx.multipart_file_path_up_down.c_str(), &file_stat) != 0) {
		return updateErrorStatus(ctx, 404, "File not found");
	}

	std::string filename = ctx.multipart_file_path_up_down;
	size_t last_slash = filename.find_last_of("/\\");
	if (last_slash != std::string::npos) {
		filename = filename.substr(last_slash + 1);
	}

	std::string contentType = determineContentType(ctx.multipart_file_path_up_down);

	// First time - send headers
	if (!ctx.download_headers_sent) {
		std::stringstream response;
		response << "HTTP/1.1 200 OK\r\n"
				<< "Content-Type: " << contentType << "\r\n"
				<< "Content-Length: " << file_stat.st_size << "\r\n"
				<< "Content-Disposition: attachment; filename=\"" << filename << "\"\r\n"
				<< "Connection: " << (ctx.keepAlive ? "keep-alive" : "close") << "\r\n";

		for (const auto& cookiePair : ctx.setCookies) {
			Cookie cookie;
			cookie.name = cookiePair.first;
			cookie.value = cookiePair.second;
			cookie.path = "/";
			response << generateSetCookieHeader(cookie) << "\r\n";
		}

		response << "\r\n";

		//Logger::magenta("send headers");
		if (send(ctx.client_fd, response.str().c_str(), response.str().size(), MSG_NOSIGNAL) < 0) {
			close(ctx.multipart_fd_up_down);
			ctx.multipart_fd_up_down = -1;
			return updateErrorStatus(ctx, 500, "Failed to send headers");
		}

		ctx.download_headers_sent = true;
		ctx.download_phase = true; // Set to read phase first
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		return true;
	}

	// Alternate between reading and sending
	if (ctx.download_phase) {
		char buffer[DEFAULT_REQUESTBUFFER_SIZE];
		//Logger::magenta("reading chunk from file");
		ssize_t bytes_read = read(ctx.multipart_fd_up_down, buffer, sizeof(buffer));

		if (bytes_read < 0) {
			close(ctx.multipart_fd_up_down);
			ctx.multipart_fd_up_down = -1;
			return updateErrorStatus(ctx, 500, "Failed to read file");
		}

		if (bytes_read == 0) {
			//Logger::yellow("finished download");
			close(ctx.multipart_fd_up_down);
			ctx.multipart_fd_up_down = -1;

			if (ctx.keepAlive) {
				resetContext(ctx);
				modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
			} else {
				delFromEpoll(ctx.epoll_fd, ctx.client_fd);
			}
			return true;
		}

		ctx.write_buffer.assign(buffer, buffer + bytes_read);
		ctx.download_phase = false;
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		return true;
	} else {
		if (ctx.write_buffer.size() > 0) {
			//Logger::magenta("sending chunk to client");
			if (send(ctx.client_fd, ctx.write_buffer.data(), ctx.write_buffer.size(), MSG_NOSIGNAL) < 0) {
				close(ctx.multipart_fd_up_down);
				ctx.multipart_fd_up_down = -1;
				return updateErrorStatus(ctx, 500, "Failed to send file content");
			}
			ctx.req.current_body_length += ctx.write_buffer.size();
			Logger::progressBar(ctx.req.current_body_length, ctx.req.expected_body_length, "(" + std::to_string(ctx.multipart_fd_up_down) + ") Download 8");
			ctx.write_buffer.clear();
			ctx.download_phase = true; // Switch back to read phase
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
			return true;
		}
	}

	return false;
}

