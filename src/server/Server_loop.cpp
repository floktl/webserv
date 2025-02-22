#include "Server.hpp"

size_t Server::findBodyStart(const std::string& buffer, Context& ctx) {
	const std::string boundaryMarker = "\r\n\r\n";
	size_t pos = buffer.find(boundaryMarker);

	if (pos != std::string::npos) {
		if (isMultipartUpload(ctx)) {
			std::string contentType = ctx.headers["Content-Type"];
			size_t boundaryPos = contentType.find("boundary=");
			if (boundaryPos != std::string::npos) {
				size_t boundaryStart = boundaryPos + 9;
				size_t boundaryEnd = contentType.find(';', boundaryStart);
				if (boundaryEnd == std::string::npos) {
					boundaryEnd = contentType.length();
				}

				std::string boundary = "--" + contentType.substr(boundaryStart, boundaryEnd - boundaryStart);
				ctx.boundary = boundary;
				size_t boundaryInBuffer = buffer.find(boundary);
				if (boundaryInBuffer != std::string::npos) {
					return boundaryInBuffer + boundary.length() + 2;
				}
			}
		}
		return pos + boundaryMarker.length();
	}

	return 0;
}

std::string Server::extractBodyContent(const char* buffer, ssize_t bytes, Context& ctx) {
	std::string cleanBody;

	if (ctx.headers_complete) {
		std::string temp(buffer, bytes);
		size_t bodyStart = findBodyStart(temp, ctx);

		if (bodyStart < static_cast<size_t>(bytes)) {
			cleanBody.assign(buffer + bodyStart, buffer + bytes);

// Now remove the boundary and header
			const std::string boundaryMarker = "\r\n\r\n";
			size_t boundaryPos = cleanBody.find(boundaryMarker);
			if (boundaryPos != std::string::npos) {
				cleanBody = cleanBody.substr(boundaryPos + boundaryMarker.length());
			}

// Removing the content disposition header (if available)
			size_t contentDispositionPos = cleanBody.find("Content-Disposition: form-data;");
			if (contentDispositionPos != std::string::npos) {
				size_t endPos = cleanBody.find("\r\n", contentDispositionPos);
				if (endPos != std::string::npos) {
					cleanBody.erase(contentDispositionPos, endPos - contentDispositionPos + 2);
				}
			}

// Removing the content type header (if available)
			size_t contentTypePos = cleanBody.find("Content-Type:");
			if (contentTypePos != std::string::npos) {
				size_t endPos = cleanBody.find("\r\n", contentTypePos);
				if (endPos != std::string::npos) {
					cleanBody.erase(contentTypePos, endPos - contentTypePos + 2);
				}
			}

// Removing the boundary at the end
			size_t boundaryEndPos = cleanBody.rfind("--" + ctx.headers["Content-Type"].substr(ctx.headers["Content-Type"].find("boundary=") + 9));
			if (boundaryEndPos != std::string::npos) {
				cleanBody.erase(boundaryEndPos);
			}
		}
	}

	return cleanBody;
}


bool Server::isMultipartUpload(Context& ctx) {
	bool isMultipartUpload = ctx.method == "POST" &&
		ctx.headers.find("Content-Type") != ctx.headers.end() &&
		ctx.headers["Content-Type"].find("multipart/form-data") != std::string::npos;

	return isMultipartUpload;
}

bool Server::prepareMultipartUpload(Context& ctx) {
	if (ctx.error_code)
	{
		ctx.uploaded_file_path = "";
		ctx.upload_fd = -1;
		return false;
	}
// If (! DetermineType (CTX, configs)) {
// Logger :: Errorlog ("Failed to Determine Request Type");
// return false;
// None
	std::string req_root = retreiveReqRoot(ctx);
	Logger::magenta(ctx.uploaded_file_path);
	if (ctx.uploaded_file_path == ctx.location.upload_store)
	{
		updateErrorStatus(ctx, 400, "Bad Request");
		return false;
	}
	ctx.uploaded_file_path = concatenatePath(req_root, ctx.uploaded_file_path);

	prepareUploadPingPong(ctx);

	if (ctx.error_code != 0) {
		Logger::errorLog("Upload preparation failed");
		return false;
	}
	return true;
}

bool Server::readingTheBody(Context& ctx, const char* buffer, ssize_t bytes) {
	ctx.had_seq_parse = true;

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

		if (ctx.boundary.empty()) {
			Logger::errorLog("Could not determine boundary from headers");
			return false;
		}
	}

	ctx.write_buffer.clear();
	bool extraction_result = extractFileContent(ctx.boundary, std::string(buffer, bytes), ctx.write_buffer, ctx);

	if (ctx.upload_fd > 0 && !ctx.write_buffer.empty()) {
		ctx.write_pos = 0;
		ctx.write_len = ctx.write_buffer.size();
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
	} else if (ctx.upload_fd > 0) {
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
	}

	return extraction_result;
}

// Runs the Main Event Loop, Handling Incoming Connections and Processing Events via Epoll
// Int server :: Runeventloop (inte epoll_fd, std :: Vector <serverblock> & configs) {
// Const int Max_events = 64;
// Struct epoll_event events [max_events];
// Const int timeout_ms = 1000;
// int incoming_fd = -1;
// int server_fd = -1;
// int client_fd = -1;
// // uint32_t eV;
// int eventNum;

// while (! g_shutdown_requested) {
// Eventnum = epoll_wait (epoll_fd, events, max_events, timeout_ms);
// if (eventnum <0 &&! g_shutdown_requested) {
// Logger :: Errorlog ("Epoll Error:" + Std :: String (streror (errno))));
// if (errno == entered)
// Continue;
// Break;
// None
// else if (eventnum == 0 &&! g_shutdown_requested) {
// CheckandCleanupTimeouts ();
// Continue;
// None

// if (! g_shutdown_requested)
// None
// for (int eventter = 0; eventiter <eventnum; eventiter ++) {
// incoming_fd = events [Eventiter] .data.fd;

// auto ctx_iter = globalfds.context_map.find (incoming_fd);
// BOOL is_active_upload = false;

// if (ctx_iter! = globalfds.context_map.end ()) {
// Context & CTX = CTX_ITER-> Second;
// If (Ismulti Partupload (CTX) &&
// ctx.req.parsing_phase == Requestbody :: Parsing_Body &&
// ! ctx.req.is_upload_complete) {
// is_active_upload = true;
// None
// None

// if (is_active_upload &&! g_shutdown_requested) {
// Handleacceptedconnection (Epoll_FD, Incoming_FD, Events [Eventiter] .events, configs);
// Continue;
// None

// If (FindSerlock (configs, incoming_fd) &&! g_shutdown_requested) {
// server_fd = incoming_fd;
// Acceptnewconnection (Epoll_FD, Server_FD, Configs);
// None
// else if (! g_shutdown_requested) {
// client_fd = incoming_fd;
// Handleacceptedconnection (Epoll_FD, Client_FD, Events [Eventiter] .Events, configs);
// None
// None
// None
// None
// if (g_shutdown_requested) {
// This-> cleanup ();
// None
// if (epoll_fd> 0) {
// close (epoll_fd);
// epoll_fd = -1;
// globalfds.epoll_fd = -1;
// None
// epoll_fd = -1;
// return exit_success;
// None


int Server::runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs) {
	const int max_events = 64;
	struct epoll_event events[max_events];
	const int timeout_ms = 1000;
	int incoming_fd = -1;
	int server_fd = -1;
	int client_fd = -1;
// uint32_t eV;
	int eventNum;

	while (!g_shutdown_requested) {
		eventNum = epoll_wait(epoll_fd, events, max_events, timeout_ms);
		log_global_fds(globalFDS);
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
				if (isMultipartUpload(ctx) &&
					ctx.req.parsing_phase == RequestBody::PARSING_BODY &&
					!ctx.req.is_upload_complete) {
					is_active_upload = true;
				}
			}

			if (is_active_upload) {
				handleAcceptedConnection(epoll_fd, incoming_fd, events[eventIter].events, configs);
				continue;
			}

			if (findServerBlock(configs, incoming_fd)) {
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
		ctx.had_seq_parse = false;
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

bool Server::extractFileContent(const std::string& boundary, const std::string& buffer, std::vector<char>& output, Context& ctx) {

	if (boundary.empty()) {
		return false;
	}

	size_t pos = buffer.find(boundary);

	bool final_boundary = (buffer.find("--" + boundary + "--") != std::string::npos);

	if (!ctx.found_first_boundary) {
		if (pos != std::string::npos) {
			ctx.found_first_boundary = true;

			size_t headers_end = buffer.find("\r\n\r\n", pos);
			if (headers_end != std::string::npos) {
				size_t content_start = headers_end + 4;

				if (final_boundary && buffer.find("--" + boundary + "--") < content_start) {
					return true;
				}

				if (content_start < buffer.size()) {
					output.insert(output.end(),
								buffer.begin() + content_start,
								buffer.end());
				}
			}
		}
		return true;
	}

	else {
		if (final_boundary) {
			size_t end_pos = buffer.find("--" + boundary + "--");
			if (end_pos > 0) {
				output.insert(output.end(),
							buffer.begin(),
							buffer.begin() + end_pos - 2);
			}
			return true;
		}

		if (pos != std::string::npos) {
			if (pos > 0) {
				output.insert(output.end(),
							buffer.begin(),
							buffer.begin() + pos - 2);
			}

			size_t headers_end = buffer.find("\r\n\r\n", pos);
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

// Handles Reading Request Data from the Client and Processing It Accordingly
bool Server::handleRead(Context& ctx, std::vector<ServerBlock>& configs)
{

	if (!isMultipartUpload(ctx) || ctx.req.parsing_phase != RequestBody::PARSING_BODY) {
		ctx.read_buffer.clear();
	}

	ctx.write_buffer.clear();

	char buffer[DEFAULT_REQUESTBUFFER_SIZE];
	std::memset(buffer, 0, sizeof(buffer));
	ssize_t bytes = read(ctx.client_fd, buffer, sizeof(buffer));

	if (bytes <= 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			if (ctx.upload_fd > 0) {
				close(ctx.upload_fd);
				ctx.upload_fd = -1;
				ctx.write_buffer.clear();
				ctx.write_pos = 0;
				ctx.write_len = 0;
			}
			return Logger::errorLog("Read error: " + std::string(strerror(errno)));
		}
		return false;
	}

	ctx.last_activity = std::chrono::steady_clock::now();

	if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE && ctx.upload_fd < 0)
		resetContext(ctx);

	if (isMultipartUpload(ctx) && ctx.req.parsing_phase == RequestBody::PARSING_BODY) {
		ctx.read_buffer = std::string(buffer, bytes);
	} else {
		ctx.read_buffer = std::string(buffer, bytes);
	}

// Logger :: cyan ("read_ buffer: \ n" + logger :: logreadbuffer (ctx.read_ buffer));

	if (!ctx.headers_complete) {
		if (!handleParsingPhase(ctx, configs)) {
			Logger::errorLog("Header parsing failed");
			return false;
		}
		if (ctx.error_code)
		{
			return false;
		}

		if (ctx.headers_complete) {
			handleSessionCookies(ctx);

			if (!determineType(ctx, configs)) {
				Logger::errorLog("Failed to determine request type");
				return false;
			}
			if (ctx.method == "GET" || ctx.method == "DELETE") {
				modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
				return true;
			}

			if (isMultipartUpload(ctx)) {
				ctx.req.parsing_phase = RequestBody::PARSING_BODY;

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

				if (ctx.uploaded_file_path.empty()) {
					parseMultipartHeaders(ctx);
				}

				if (!ctx.uploaded_file_path.empty() && ctx.upload_fd < 0) {
					if (!prepareMultipartUpload(ctx)) {
						return false;
					}
				}

				readingTheBody(ctx, buffer, bytes);
			}
		}
	} else if (isMultipartUpload(ctx) && ctx.req.parsing_phase == RequestBody::PARSING_BODY) {

		if (ctx.read_buffer.size() > ctx.boundary.size() * 2) {
			for (size_t i = 0; i <= ctx.read_buffer.size() - ctx.boundary.size(); i++) {
				if (ctx.read_buffer.compare(i, ctx.boundary.size(), ctx.boundary) == 0) {
					break;
				}
			}
		}

		extractFileContent(ctx.boundary, ctx.read_buffer, ctx.write_buffer, ctx);
	}

// Logger :: Magenta ("Write_Buffer: \ n" + Logger :: Logwritebuffer (ctx.write_ buffer));

	if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE && ctx.had_seq_parse) {
		std::cout << std::endl;
	}

	if (isMultipartUpload(ctx) && ctx.req.parsing_phase == RequestBody::PARSING_BODY
		&& !ctx.write_buffer.empty() && ctx.upload_fd >= 0) {
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		return true;
	}

	if (isMultipartUpload(ctx) && ctx.read_buffer.find("--" + ctx.boundary + "--") != std::string::npos) {
		if (ctx.upload_fd >= 0) {
			if (!ctx.write_buffer.empty()) {
				ssize_t written = write(ctx.upload_fd, ctx.write_buffer.data(), ctx.write_buffer.size());
				if (written > 0) {
					ctx.req.current_body_length += written;
				}
			}

			close(ctx.upload_fd);
			ctx.upload_fd = -1;
		}

		ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
		ctx.req.is_upload_complete = true;
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		return true;
	}

	return finalizeRequest(ctx);
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

bool Server::doMultipartWriting(Context& ctx) {

	bool final_boundary_found = false;
	if (!ctx.read_buffer.empty()) {
		final_boundary_found = (ctx.read_buffer.find("--" + ctx.boundary + "--") != std::string::npos);
	}
	if (ctx.write_buffer.empty()) {
		if (final_boundary_found) {
			return completeUpload(ctx);
		}

		if (ctx.req.current_body_length >= ctx.req.expected_body_length) {
			return completeUpload(ctx);
		}

		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
		return true;
	}

	if (ctx.upload_fd < 0) {
		ctx.upload_fd = open(ctx.uploaded_file_path.c_str(), O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
		if (ctx.upload_fd < 0) {
			Logger::errorLog("Failed to open file: " + std::string(strerror(errno)));
			return updateErrorStatus(ctx, 500, "Failed to open upload file");
		}
	}

	ssize_t written = write(ctx.upload_fd,
						ctx.write_buffer.data(),
						ctx.write_buffer.size());

	if (written < 0) {
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
			return true;
		}
		Logger::errorLog("Write error: " + std::string(strerror(errno)));
		close(ctx.upload_fd);
		ctx.upload_fd = -1;
		return updateErrorStatus(ctx, 500, "Failed to write to upload file");
	}

	ctx.req.current_body_length += written;
	Logger::progressBar(ctx.req.current_body_length, ctx.req.expected_body_length, "Upload: 8");
	ctx.write_buffer.clear();

	bool size_nearly_complete = (ctx.req.expected_body_length > 0 &&
								ctx.req.current_body_length >= ctx.req.expected_body_length - DEFAULT_REQUESTBUFFER_SIZE);

	if (size_nearly_complete) {
		if (!ctx.near_completion_count) {
			ctx.near_completion_count = 1;
		} else {
			ctx.near_completion_count++;
		}
	}

	if (final_boundary_found) {
		return completeUpload(ctx);
	} else if (ctx.req.current_body_length >= ctx.req.expected_body_length) {
		return completeUpload(ctx);
	} else if (size_nearly_complete && ctx.near_completion_count >= 2) {
		return completeUpload(ctx);
	}

	modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
	return true;
}

// New Helper Method to Complete The Upload Process
bool Server::completeUpload(Context& ctx) {
	Logger::progressBar(ctx.req.expected_body_length, ctx.req.expected_body_length, "Completed Upload: 8");
	std::cout << std::endl;

	if (ctx.upload_fd >= 0) {
		close(ctx.upload_fd);
		ctx.upload_fd = -1;
	}

	ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
	ctx.req.is_upload_complete = true;
	ctx.found_first_boundary = false;

	std::string response;
	if (!ctx.location.return_code.empty() && !ctx.location.return_url.empty()) {
		response = "HTTP/1.1 " + ctx.location.return_code + " Redirect\r\n";
		response += "Location: " + ctx.location.return_url + "\r\n";
		response += "Content-Length: 0\r\n";
		response += "Connection: close\r\n\r\n";
	} else {
		response = "HTTP/1.1 200 OK\r\n";
		response += "Content-Type: text/html\r\n";
		response += "Connection: close\r\n";
		response += "Content-Length: 135\r\n\r\n";
		response += "<html><body><h1>Upload successful</h1><script>window.location.href='/';</script></body></html>";
	}

	ssize_t sent = write(ctx.client_fd, response.c_str(), response.length());
	if (sent <= 0) {
		Logger::errorLog("Failed to send completion response");
	}

	ctx.keepAlive = false;
	delFromEpoll(ctx.epoll_fd, ctx.client_fd);

	return false;
}

void Server::initializeWritingActions(Context& ctx)
{
	if (ctx.write_pos == SIZE_MAX || ctx.write_len == SIZE_MAX) {
		ctx.write_pos = 0;
		ctx.write_len = 0;
	}

	if (ctx.write_len > ctx.write_buffer.size()) {
		ctx.write_len = ctx.write_buffer.size();
	}
}

bool Server::handleWrite(Context& ctx) {

	bool result = false;
	initializeWritingActions(ctx);
	if (ctx.upload_fd > 0) {
		if (!ctx.write_buffer.empty()) {
			result = doMultipartWriting(ctx);
		} else {
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
			return true;
		}
	}
	else
	{
		Logger::yellow("normal write");
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
				result = executeCgi(ctx);
				break;
			case ERROR:
				return getErrorHandler()->generateErrorResponse(ctx);
		}
		if (result)
			resetContext(ctx);
	}

	if (ctx.error_code != 0)
		return getErrorHandler()->generateErrorResponse(ctx);

	if (result)
	{
		if (ctx.keepAlive)
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
		else
			result = false;
	}

	if (!result)
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
	return result;
}