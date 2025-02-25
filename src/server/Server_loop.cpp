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

bool Server::extractFileContent(const std::string& boundary, const std::string& buffer, std::vector<char>& output, Context& ctx)
{
	if (boundary.empty()) {
		return false;
	}

	std::string finalBoundaryStr =  "--" + boundary + "--";
	if (ctx.final_boundary_found)
	{
		finalBoundaryStr = boundary + "--";
	}
	Logger::magenta(boundary);
	Logger::magenta(finalBoundaryStr);
	size_t boundaryPos = buffer.find(boundary);
	size_t finalBoundaryPos = buffer.find(finalBoundaryStr);
	bool final_boundary_found = (finalBoundaryPos != std::string::npos);

	if (final_boundary_found) {
		ctx.req.is_upload_complete = true;
	}

	Logger::magenta("in extract final_boundary_found " + std::to_string(final_boundary_found));
	if (!ctx.found_first_boundary && boundaryPos != std::string::npos && final_boundary_found) {
		size_t headers_end = buffer.find("\r\n\r\n", boundaryPos);
		if (headers_end != std::string::npos) {
			size_t content_start = headers_end + 4;
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
				ctx.found_first_boundary = true;
	Logger::magenta("i.found_first_boundary && boundaryPos != std::string::npos && final_boundary_found" + std::to_string(final_boundary_found));
				return true;
			}
		}
	}

	// Standard case: First boundary
	if (!ctx.found_first_boundary) {
		if (boundaryPos != std::string::npos) {
			ctx.found_first_boundary = true;
			size_t headers_end = buffer.find("\r\n\r\n", boundaryPos);
			if (headers_end != std::string::npos) {
				size_t content_start = headers_end + 4;
				if (final_boundary_found && finalBoundaryPos < content_start) {
					Logger::magenta("inal_boundary_found && finalBoundaryPos < content_start " + std::to_string(final_boundary_found));

					return true;
				}
				if (content_start < buffer.size()) {
					output.insert(output.end(),
								buffer.begin() + content_start,
								buffer.end());
				}
			}
		}
		Logger::magenta("iStandard case: First bounda " + std::to_string(final_boundary_found));

		return true;
	}
	// Subsequent boundaries
	else {
		if (final_boundary_found) {
			size_t end_pos = finalBoundaryPos;
			if (end_pos > 0) {
				// Back up to exclude CRLF before boundary if it exists
				if (end_pos >= 2 && buffer[end_pos-1] == '\n' && buffer[end_pos-2] == '\r') {
					end_pos -= 2;
				}

				output.insert(output.end(),
							buffer.begin(),
							buffer.begin() + end_pos);
			}
		Logger::magenta("else {	if (final_boundary_found) {" + std::to_string(final_boundary_found));
			return true;
		}

		if (boundaryPos != std::string::npos) {
			if (boundaryPos > 0) {
				size_t end_pos = boundaryPos;
				// Back up to exclude CRLF before boundary if it exists
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
		Logger::magenta("easdasdas" + std::to_string(final_boundary_found));
	return true;
}
// Handles Reading Request Data from the Client and Processing It Accordingly
// Modified handleRead function to properly handle multipart uploads
bool Server::handleRead(Context& ctx, std::vector<ServerBlock>& configs)
{
	Logger::magenta("READ");
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
			ctx.write_pos = 0;
			ctx.write_len = 0;
		}
		return Logger::errorLog("Read error");
	}
	if (bytes == 0 && !ctx.req.is_upload_complete)
	{
		return true;
	}

	ctx.last_activity = std::chrono::steady_clock::now();
	if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE && ctx.multipart_fd_up_down < 0)
		resetContext(ctx);
	ctx.read_buffer = std::string(buffer, bytes);

	//Logger::cyan("read_buffer:\n" + Logger::logReadBuffer(ctx.read_buffer));
	Logger::cyan("read_buffer:\n" + std::string(ctx.read_buffer.begin(), ctx.read_buffer.end()));

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

	if (ctx.headers_complete && ctx.is_multipart && !ctx.ready_for_ping_pong) {
		if (!parseContentDisposition(ctx)) {
			Logger::errorLog("Content Disposition parsing failed");
			return false;
		}
		if (ctx.error_code)
			return false;

		// If we have found the filename from Content-Disposition, prepare for upload
		if (!ctx.multipart_file_path_up_down.empty()) {
			prepareUploadPingPong(ctx);
			if (ctx.error_code)
				return false;

			ctx.ready_for_ping_pong = true;
			ctx.req.parsing_phase = RequestBody::PARSING_BODY;

			// Check for final boundary before attempting to extract content
			ctx.final_boundary_found = (ctx.read_buffer.find(ctx.boundary + "--") != std::string::npos);

			// Process initial data if we have it
			extractFileContent(ctx.boundary, ctx.read_buffer, ctx.write_buffer, ctx);

			// If final boundary is found, complete the upload
			if (ctx.final_boundary_found) {
				// Write any pending data first
				if (!ctx.write_buffer.empty() && ctx.multipart_fd_up_down >= 0) {
					ssize_t written = write(ctx.multipart_fd_up_down, ctx.write_buffer.data(), ctx.write_buffer.size());
					if (written > 0) {
						ctx.req.current_body_length += written;
					}
				}
				return completeUpload(ctx);
			}

			// If we have data to write, switch to write mode
			if (!ctx.write_buffer.empty() && ctx.multipart_fd_up_down >= 0) {
				modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
				return true;
			}
		} else {
			// Keep waiting for more data to find Content-Disposition
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
			return true;
		}
	}


	if (ctx.headers_complete && ctx.ready_for_ping_pong) {
		handleSessionCookies(ctx);

		if (ctx.method == "GET" || ctx.method == "DELETE") {
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
			return true;
		}

		if (ctx.is_multipart && ctx.req.parsing_phase == RequestBody::PARSING_BODY) {
			// Check for final boundary before extracting content
			ctx.final_boundary_found = (ctx.read_buffer.find("--" + ctx.boundary + "--") != std::string::npos);

			// Process the incoming data chunk
			extractFileContent(ctx.boundary, ctx.read_buffer, ctx.write_buffer, ctx);

			// If final boundary was found, complete the upload
			if (ctx.final_boundary_found) {
				// Write any pending data first
				if (!ctx.write_buffer.empty() && ctx.multipart_fd_up_down >= 0) {
					ssize_t written = write(ctx.multipart_fd_up_down, ctx.write_buffer.data(), ctx.write_buffer.size());
					if (written > 0) {
						ctx.req.current_body_length += written;
					}
				}
				return completeUpload(ctx);
			}

			// If we have data to write, trigger a write operation
			if (!ctx.write_buffer.empty() && ctx.multipart_fd_up_down >= 0) {
				modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
				return true;
			}
		}
	} else if (isMultipart(ctx) && ctx.req.parsing_phase == RequestBody::PARSING_BODY) {
		// Check for final boundary before extracting content
		ctx.final_boundary_found = (ctx.read_buffer.find("--" + ctx.boundary + "--") != std::string::npos);

		// Process continuing chunks of multipart data
		extractFileContent(ctx.boundary, ctx.read_buffer, ctx.write_buffer, ctx);

		// If final boundary was found, complete the upload
		if (ctx.final_boundary_found) {
			// Write any pending data first
			if (!ctx.write_buffer.empty() && ctx.multipart_fd_up_down >= 0) {
				ssize_t written = write(ctx.multipart_fd_up_down, ctx.write_buffer.data(), ctx.write_buffer.size());
				if (written > 0) {
					ctx.req.current_body_length += written;
				}
			}
			return completeUpload(ctx);
		}

		// If we have data to write, trigger a write event
		if (!ctx.write_buffer.empty() && ctx.multipart_fd_up_down >= 0) {
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
			return true;
		}
	}

	//Logger::green("Write_Buffer:\n" + Logger::logWriteBuffer(ctx.write_buffer));

	return finalizeRequest(ctx);
}

bool Server::parseContentDisposition(Context& ctx)
{
	if (!ctx.is_multipart) {
		return true; // Not needed for non-multipart requests
	}

	// Extract boundary if not done yet
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

	// Find the boundary in the buffer
	size_t boundary_start = ctx.read_buffer.find(ctx.boundary);
	if (boundary_start == std::string::npos) {
		return true; // Not found yet, need more data
	}

	// Find Content-Disposition
	size_t disp_pos = ctx.read_buffer.find("Content-Disposition: form-data", boundary_start);
	if (disp_pos == std::string::npos) {
		return true; // Not found yet, need more data
	}

	// Extract filename
	size_t filename_pos = ctx.read_buffer.find("filename=\"", disp_pos);
	if (filename_pos != std::string::npos) {
		filename_pos += 10; // Length of 'filename="'
		size_t filename_end = ctx.read_buffer.find("\"", filename_pos);
		if (filename_end != std::string::npos) {
			std::string filename = ctx.read_buffer.substr(filename_pos, filename_end - filename_pos);
			ctx.headers["Content-Disposition-Filename"] = filename;
			ctx.multipart_file_path_up_down = concatenatePath(ctx.location.upload_store, filename);

			// Find the end of headers to locate the start of content
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

bool Server::doMultipartWriting(Context& ctx) {
	// First check if upload is already marked as complete
	if (ctx.req.is_upload_complete) {
		return completeUpload(ctx);
	}

	// Check for final boundary in the read buffer
	bool final_boundary_found = false;
	if (!ctx.read_buffer.empty()) {
		final_boundary_found = (ctx.read_buffer.find("--" + ctx.boundary + "--") != std::string::npos);
	}

	// If buffer is empty, check if we should finalize
	if (ctx.write_buffer.empty()) {
		if (final_boundary_found || ctx.req.is_upload_complete) {
			return completeUpload(ctx);
		}

		if (ctx.req.current_body_length >= ctx.req.expected_body_length) {
			return completeUpload(ctx);
		}

		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
		return true;
	}

	// Open the file if not already open
	if (ctx.multipart_fd_up_down < 0) {
		ctx.multipart_fd_up_down = open(ctx.multipart_file_path_up_down.c_str(), O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
		if (ctx.multipart_fd_up_down < 0) {
			Logger::errorLog("Failed to open file");
			return updateErrorStatus(ctx, 500, "Failed to open upload file");
		}
	}

	// Write the data
	ssize_t written = write(ctx.multipart_fd_up_down,
						ctx.write_buffer.data(),
						ctx.write_buffer.size());

	if (written < 0) {
		Logger::errorLog("Write error");
		close(ctx.multipart_fd_up_down);
		ctx.multipart_fd_up_down = -1;
		return updateErrorStatus(ctx, 500, "Failed to write to upload file");
	}

	ctx.req.current_body_length += written;
	Logger::progressBar(ctx.req.current_body_length, ctx.req.expected_body_length, "Upload: 8");
	ctx.write_buffer.clear();

	// Check if we're close to completion
	bool size_nearly_complete = (ctx.req.expected_body_length > 0 &&
								ctx.req.current_body_length >= ctx.req.expected_body_length - DEFAULT_REQUESTBUFFER_SIZE);

	if (size_nearly_complete) {
		if (!ctx.near_completion_count) {
			ctx.near_completion_count = 1;
		} else {
			ctx.near_completion_count++;
		}
	}

	// Determine if we should complete the upload
	if (final_boundary_found || ctx.req.is_upload_complete) {
		return completeUpload(ctx);
	} else if (ctx.req.current_body_length >= ctx.req.expected_body_length) {
		return completeUpload(ctx);
	} else if (size_nearly_complete && ctx.near_completion_count >= 2) {
		return completeUpload(ctx);
	}

	// Continue reading
	modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
	return true;
}

// New Helper Method to Complete The Upload Process
bool Server::completeUpload(Context& ctx) {
	Logger::progressBar(ctx.req.expected_body_length, ctx.req.expected_body_length, "Completed Upload: 8");
	std::cout << std::endl;

	if (ctx.multipart_fd_up_down >= 0) {
		close(ctx.multipart_fd_up_down);
		ctx.multipart_fd_up_down = -1;
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
	Logger::green("handleWrite");
	bool result = false;
	initializeWritingActions(ctx);

		Logger::yellow("fd: " + std::to_string(ctx.multipart_fd_up_down));
	if (ctx.multipart_fd_up_down > 0) {
		if (ctx.is_download) {
		Logger::yellow("downloadHandler");
			result = downloadHandler(ctx);
		} else if (!ctx.write_buffer.empty()) {
			// Handle upload
			result = doMultipartWriting(ctx);
		} else {
			// If the upload is complete, finish the process
			if (ctx.req.is_upload_complete) {
				return completeUpload(ctx);
			}
			// Otherwise, continue reading
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
			return true;
		}
	}
	else {
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

bool Server::downloadHandler(Context &ctx) {
	std::string fullPath = ctx.approved_req_path;

	// Check if file is already open
	if (ctx.multipart_fd_up_down < 0) {
		ctx.multipart_fd_up_down = open(fullPath.c_str(), O_RDONLY);
		if (ctx.multipart_fd_up_down < 0) {
			return updateErrorStatus(ctx, 404, "Not found");
		}

		// Get file size
		struct stat file_stat;
		if (fstat(ctx.multipart_fd_up_down, &file_stat) < 0) {
			close(ctx.multipart_fd_up_down);
			ctx.multipart_fd_up_down = -1;
			return updateErrorStatus(ctx, 500, "Internal Server Error");
		}

		// Prepare HTTP headers (only on first call)
		std::string contentType = determineContentType(fullPath);
		std::stringstream headers;
		headers << "HTTP/1.1 200 OK\r\n"
				<< "Content-Type: " << contentType << "\r\n"
				<< "Content-Length: " << file_stat.st_size << "\r\n"
				<< "Connection: " << (ctx.keepAlive ? "keep-alive" : "close") << "\r\n";

		// Add filename for download (derive from path)
		size_t lastSlash = fullPath.find_last_of("/\\");
		std::string filename = (lastSlash != std::string::npos) ?
								fullPath.substr(lastSlash + 1) : fullPath;
		headers << "Content-Disposition: attachment; filename=\"" << filename << "\"\r\n";

		// Add cookies if needed
		for (const auto& cookiePair : ctx.setCookies) {
			Cookie cookie;
			cookie.name = cookiePair.first;
			cookie.value = cookiePair.second;
			cookie.path = "/";
			headers << generateSetCookieHeader(cookie) << "\r\n";
		}

		headers << "\r\n";

		// Store headers in a temporary buffer
		ctx.tmp_buffer = headers.str();

		// Reset tracking variables
		ctx.req.expected_body_length = file_stat.st_size;
		ctx.req.current_body_length = 0;
		ctx.write_buffer.clear();
	}

	// First, check if we need to send headers
	if (!ctx.tmp_buffer.empty()) {
		ssize_t sent = write(ctx.client_fd, ctx.tmp_buffer.c_str(), ctx.tmp_buffer.length());
		if (sent <= 0) {
			close(ctx.multipart_fd_up_down);
			ctx.multipart_fd_up_down = -1;
			return updateErrorStatus(ctx, 500, "Failed to send headers");
		}
		ctx.tmp_buffer.clear();
	}

	// Read chunk from file into buffer
	ctx.write_buffer.resize(DEFAULT_REQUESTBUFFER_SIZE);
	ssize_t bytes_read = read(ctx.multipart_fd_up_down, ctx.write_buffer.data(), DEFAULT_REQUESTBUFFER_SIZE);

	if (bytes_read < 0) {
		close(ctx.multipart_fd_up_down);
		ctx.multipart_fd_up_down = -1;
		return updateErrorStatus(ctx, 500, "Failed to read file");
	}

	// If no more data, we're done
	if (bytes_read == 0) {
		close(ctx.multipart_fd_up_down);
		ctx.multipart_fd_up_down = -1;
		ctx.is_download = false;
		Logger::progressBar(ctx.req.expected_body_length, ctx.req.expected_body_length, "Download Complete");
		return true;
	}

	// Resize buffer to match what we read
	ctx.write_buffer.resize(bytes_read);

	// Write the chunk to the client
	ssize_t bytes_sent = write(ctx.client_fd, ctx.write_buffer.data(), bytes_read);
	if (bytes_sent <= 0) {
		close(ctx.multipart_fd_up_down);
		ctx.multipart_fd_up_down = -1;
		return updateErrorStatus(ctx, 500, "Failed to send file data");
	}

	// Update progress
	ctx.req.current_body_length += bytes_sent;
	Logger::progressBar(ctx.req.current_body_length, ctx.req.expected_body_length, "Download");

	// If we didn't send all bytes, adjust our position in the file
	if (bytes_sent < bytes_read) {
		lseek(ctx.multipart_fd_up_down, -(bytes_read - bytes_sent), SEEK_CUR);
	}

	// Schedule more reading/writing
	modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);

	// If we're done, clean up
	if (ctx.req.current_body_length >= ctx.req.expected_body_length) {
		close(ctx.multipart_fd_up_down);
		ctx.multipart_fd_up_down = -1;
		ctx.is_download = false;
		return true;
	}

	return true;
}