#include "Server.hpp"

bool Server::handleNewConnection(int epoll_fd, int server_fd)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	Context ctx;

	while (true)
	{
		int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
		if (client_fd <= 0)
		{
			if (errno == EAGAIN || errno == EWOULDBLOCK)
			{
				// No more connections to accept (normal behavior in non-blocking mode)
				break;
			}
			else
			{
				Logger::errorLog("Failed to accept connection: " + std::string(strerror(errno)) + " Server_fd: " +  std::to_string(server_fd));
				return false;
			}
		}

		Logger::file("New client_fd: " + std::to_string(client_fd) + " accepted on server_fd: " + std::to_string(server_fd));

		if (setNonBlocking(client_fd) < 0) {
			Logger::errorLog("Failed to set non-blocking mode for client_fd: " + std::to_string(client_fd));
			close(client_fd);
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
		globalFDS.context_map[client_fd] = ctx;

		logRequestBodyMapFDs();
		//logContext(ctx, "New Connection");
	}
	//logContext(ctx, "Exit New Connection");
	return true;
}

//bool Server::handleRead(Context& ctx, std::vector<ServerBlock> &configs) {
//    char buffer[DEFAULT_REQUESTBUFFER_SIZE];
//    ssize_t bytes;

//    Logger::file("Starting handleRead for client_fd: " + std::to_string(ctx.client_fd));

//    if ((bytes = read(ctx.client_fd, buffer, sizeof(buffer))) > 0) {
//        Logger::file("Read " + std::to_string(bytes) + " bytes from client " + std::to_string(ctx.client_fd));
//        ctx.input_buffer.append(buffer, bytes);

//		// check header complete
//        if (!ctx.headers_complete) {
//            Logger::file("Headers incomplete - parsing headers for client " + std::to_string(ctx.client_fd));
//            parseHeaders(ctx, bytes);
//        }

//		//	check received body
//        if (ctx.headers_complete && ctx.content_length > 0) {
//            ctx.body_received += bytes;
//			Logger::file("body received: " + std::to_string(ctx.body_received) + " max length?: " + std::to_string(bytes) + " header?: " + std::to_string(bytes - ctx.body_received));
//			ctx.headers_complete = true;
//	//ctx.content_length = ctx.input_buffer.size();
//            Logger::file("Received body data for client " + std::to_string(ctx.client_fd) +
//                        " - Total received: " + std::to_string(ctx.body_received) +
//                        "/" + std::to_string(ctx.content_length));

//            ctx.body += ctx.input_buffer;
//            ctx.input_buffer.clear();
//        }
//		Logger::file("haeder complete: " + std::to_string(ctx.headers_complete));
//		// check request complete
//        if (isRequestComplete(ctx)) {
//            Logger::file("Request complete for client " + std::to_string(ctx.client_fd) +
//                        " - Determining request type");
//            determineType(ctx, configs);
//            Logger::file("Modifying epoll events for client " + std::to_string(ctx.client_fd) +
//                        " to include EPOLLOUT");
//            modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
//            return true;
//        } else if (ctx.headers_complete) // haeaders complete, but wait for more body
//		{
//            Logger::file("Headers complete but still waiting for body data from client " +
//                        std::to_string(ctx.client_fd));
//            return true;
//        }
//    }

//    if (bytes < 0) {
//        if (errno != EAGAIN && errno != EWOULDBLOCK) {
//            std::string error_msg = "Read error on fd " + std::to_string(ctx.client_fd) +
//                                  ": " + std::string(strerror(errno));
//            Logger::errorLog(error_msg);
//            Logger::file("Fatal read error for client " + std::to_string(ctx.client_fd) +
//                        ": " + std::string(strerror(errno)));
//            return false;
//        }
//        Logger::file("Non-blocking read would block for client " + std::to_string(ctx.client_fd));
//        return false;
//    } else if (bytes == 0) {
//        Logger::file("Client " + std::to_string(ctx.client_fd) + " closed connection");
//        return true;
//    }

//    Logger::file("Finished handleRead for client " + std::to_string(ctx.client_fd));
//    return true;
//}

bool Server::handleWrite(Context& ctx)
{
	Logger::file("entering handleWrite()");
	bool result = false;
	switch (ctx.type)
	{
		case INITIAL:
			break;
		case STATIC:
			//Logger::file("WRITE with static");
			result = staticHandler(ctx);
			break;
		case CGI:
			//Logger::file("WRITE with cgi");
			result = true;  // Handle CGI response
			break;
		case ERROR:
			//Logger::file("WRITE with error");
			result = errorsHandler(ctx, 0);
			break;
		default:
			break;
	}

	// If write was successful, reset for next request
	if (result)
	{
		ctx.input_buffer.clear();
		ctx.body.clear();
		ctx.headers_complete = false;
		ctx.content_length = 0;
		ctx.body_received = 0;
		ctx.headers.clear();

		if (ctx.keepAlive)
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
		else
			result = false;
	}
	//Logger::file("handleWrite after result");
	if (result == false)
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);

	return result;
}

void Server::parseChunkedBody(Context& ctx) {
	while (!ctx.input_buffer.empty()) {
		if (!ctx.req.chunked_state.processing) {
			// Looking for the chunk size line
			size_t pos = ctx.input_buffer.find("\r\n");
			if (pos == std::string::npos) {
				return; // Need more data
			}

			// Parse chunk size
			std::string chunk_size_str = ctx.input_buffer.substr(0, pos);
			size_t chunk_size;
			std::stringstream ss;
			ss << std::hex << chunk_size_str;
			ss >> chunk_size;

			if (chunk_size == 0) {
				// Final chunk
				ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
				ctx.req.is_upload_complete = true;
				ctx.input_buffer.clear();
				return;
			}

			// Remove the chunk size line
			ctx.input_buffer.erase(0, pos + 2);
			ctx.req.chunked_state.buffer = "";
			ctx.req.chunked_state.processing = true;
			ctx.req.expected_body_length += chunk_size;
		}

		// Process chunk data
		size_t remaining = ctx.req.expected_body_length - ctx.req.current_body_length;
		size_t available = std::min(remaining, ctx.input_buffer.size());

		if (available > 0) {
			ctx.req.received_body += ctx.input_buffer.substr(0, available);
			ctx.req.current_body_length += available;
			ctx.input_buffer.erase(0, available);
		}

		// Check for chunk end
		if (ctx.req.current_body_length == ctx.req.expected_body_length) {
			if (ctx.input_buffer.size() >= 2 && ctx.input_buffer.substr(0, 2) == "\r\n") {
				ctx.input_buffer.erase(0, 2);
				ctx.req.chunked_state.processing = false;
			} else {
				return; // Need more data
			}
		}
	}
}

bool Server::handleRead(Context& ctx, std::vector<ServerBlock> &configs) {
	char buffer[DEFAULT_REQUESTBUFFER_SIZE];
	ssize_t bytes;

	Logger::file("Starting handleRead for client_fd: " + std::to_string(ctx.client_fd));

	bytes = read(ctx.client_fd, buffer, sizeof(buffer));
	if (bytes == 0) {
		Logger::file("Client closed connection: " + std::to_string(ctx.client_fd));
		return false;
	}

	if (bytes < 0) {
		// Would block, wait for next event
		return true;
	}

	// Update last activity time
	ctx.last_activity = std::chrono::steady_clock::now();

	if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE) {
		Logger::file("Starting new request, resetting context");
		ctx.input_buffer.clear();
		ctx.headers.clear();
		ctx.method.clear();
		ctx.path.clear();
		ctx.version.clear();
		ctx.headers_complete = false;
		ctx.content_length = 0;
		ctx.req.parsing_phase = RequestBody::PARSING_HEADER;
		ctx.req.current_body_length = 0;
		ctx.req.expected_body_length = 0;
		ctx.req.received_body.clear();
		ctx.req.chunked_state.processing = false;
		ctx.req.is_upload_complete = false;
		ctx.type = RequestType::INITIAL;
	}

	// Append new data to input buffer
	ctx.input_buffer.append(buffer, bytes);
	Logger::file("Read " + std::to_string(bytes) + " bytes from client " + std::to_string(ctx.client_fd));

	// Handle based on current parsing phase
	switch(ctx.req.parsing_phase) {
		case RequestBody::PARSING_HEADER:
			if (!ctx.headers_complete) {
				if (!parseHeaders(ctx, bytes)) {
					Logger::file("Headers not finished");
					return true;
				}

				Logger::file("Headers FINISHED!!!");
				auto it = ctx.headers.find("Transfer-Encoding");
				if (it != ctx.headers.end() && it->second == "chunked") {
					Logger::file("Start chunked Body parsing");
					ctx.req.parsing_phase = RequestBody::PARSING_BODY;
					ctx.req.chunked_state.processing = true;
					if (!ctx.input_buffer.empty()) {
						parseChunkedBody(ctx);
					}
				} else if (ctx.content_length > 0) {
					Logger::file("Start unchunked Body parsing, Content-Length: " +
							std::to_string(ctx.content_length));
					ctx.req.parsing_phase = RequestBody::PARSING_BODY;
					ctx.req.expected_body_length = ctx.content_length;
					ctx.req.current_body_length = 0;

					// Process any remaining data in input buffer as body
					if (!ctx.input_buffer.empty()) {
						ctx.req.received_body += ctx.input_buffer;
						ctx.req.current_body_length += ctx.input_buffer.length();
						Logger::file("Processed " + std::to_string(ctx.input_buffer.length()) +
								" bytes of body data, total: " +
								std::to_string(ctx.req.current_body_length) + "/" +
								std::to_string(ctx.req.expected_body_length));
						ctx.input_buffer.clear();

						// Check if we've received the complete body
						if (ctx.req.current_body_length >= ctx.req.expected_body_length) {
							Logger::file("Body parsing complete!");
							ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
							ctx.req.is_upload_complete = true;
						}
					}
				} else {
					Logger::file("No body expected, request complete!");
					ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
				}
			}
			break;

		case RequestBody::PARSING_BODY:
			Logger::file("Continue body parsing...");
			if (ctx.req.chunked_state.processing) {
				parseChunkedBody(ctx);
			} else {
				// Standard body processing
				ctx.req.received_body += ctx.input_buffer;
				ctx.req.current_body_length += ctx.input_buffer.length();
				Logger::file("Processed " + std::to_string(ctx.input_buffer.length()) +
						" bytes of body data, total: " +
						std::to_string(ctx.req.current_body_length) + "/" +
						std::to_string(ctx.req.expected_body_length));
				ctx.input_buffer.clear();

				if (ctx.req.current_body_length >= ctx.req.expected_body_length) {
					Logger::file("Body parsing complete!");
					ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
					ctx.req.is_upload_complete = true;

					// Trim excess data if any
					if (ctx.req.current_body_length > ctx.req.expected_body_length) {
						ctx.req.received_body = ctx.req.received_body.substr(
							0, ctx.req.expected_body_length);
						ctx.req.current_body_length = ctx.req.expected_body_length;
					}
				}
			}
			break;

		case RequestBody::PARSING_COMPLETE:
			break;
	}

	// Handle request completion
	if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE) {
		Logger::file("Request complete for client " + std::to_string(ctx.client_fd) +
					", determining type");
		determineType(ctx, configs);
		logContext(ctx, "Parsed");
		Logger::file("THE BODY:\n" + ctx.req.received_body);
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
	} else {
		// Still need more data, ensure we're watching for it
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
	}

	return true;
}

bool Server::parseHeaders(Context& ctx, ssize_t bytes) {
	(void)bytes;
	size_t header_end = ctx.input_buffer.find("\r\n\r\n");
	if (header_end == std::string::npos) {
		Logger::file("Headers not complete yet, waiting for more data");
		return false;
	}

	std::string headers = ctx.input_buffer.substr(0, header_end);
	std::istringstream stream(headers);
	std::string line;

	// Parse request line
	if (std::getline(stream, line)) {
		if (line.back() == '\r') line.pop_back();
		std::istringstream request_line(line);
		request_line >> ctx.method >> ctx.path >> ctx.version;
		Logger::file("Parsed request line: " + ctx.method + " " + ctx.path);
	}

	// Parse headers
	while (std::getline(stream, line)) {
		if (line.empty() || line == "\r") break;
		if (line.back() == '\r') line.pop_back();

		size_t colon = line.find(':');
		if (colon != std::string::npos) {
			std::string key = line.substr(0, colon);
			std::string value = line.substr(colon + 2);
			ctx.headers[key] = value;

			if (key == "Content-Length") {
				ctx.content_length = std::stoul(value);
				Logger::file("Content-Length set to: " + value);
			}
		}
	}

	// Remove headers from input buffer, leaving any body data
	ctx.input_buffer.erase(0, header_end + 4);
	ctx.headers_complete = true;

	Logger::file("Headers parsed successfully, Content-Length: " +
				std::to_string(ctx.content_length));

	return true;
}

//bool Server::handleReadError(Context& ctx, ssize_t bytes) {
//    if (bytes < 0) {
//        if (errno != EAGAIN && errno != EWOULDBLOCK) {
//            std::string error_msg = "Read error on fd " + std::to_string(ctx.client_fd) +
//                                  ": " + std::string(strerror(errno));
//            Logger::errorLog(error_msg);
//            return false;
//        }
//        Logger::file("Non-blocking read would block for client " +
//                    std::to_string(ctx.client_fd));
//        return false;
//    }

//    Logger::file("Client " + std::to_string(ctx.client_fd) + " closed connection");
//    return true;
//}

//bool Server::parseHeaders(Context& ctx, ssize_t bytes)
//{
//	(void)bytes;
//	size_t header_end = ctx.input_buffer.find("\r\n\r\n");
//	if (header_end == std::string::npos) {
//		return false;
//	}

//	std::string headers = ctx.input_buffer.substr(0, header_end);
//	std::istringstream stream(headers);
//	std::string line;

//	if (std::getline(stream, line)) {
//		if (line.back() == '\r') line.pop_back();
//		std::istringstream request_line(line);
//		request_line >> ctx.method >> ctx.path >> ctx.version;
//	}
//	while (std::getline(stream, line)) {
//		if (line.empty() || line == "\r") break;
//		if (line.back() == '\r') line.pop_back();

//		size_t colon = line.find(':');
//		if (colon != std::string::npos) {
//			std::string key = line.substr(0, colon);
//			std::string value = line.substr(colon + 2);
//			ctx.headers[key] = value;

//				Logger::file("Header key: " + key);
//			if (key == "Content-Length") {
//				Logger::file("Content-Length wird gesetzt!");
//				ctx.content_length = std::stoul(value);
//			}
//		}
//	}
//	// Determine keepAlive status
//	ctx.keepAlive = (ctx.version == "HTTP/1.1") ?
//	(ctx.headers["Connection"] != "close") :
//	(ctx.headers["Connection"] == "keep-alive");
//	//Logger::file("checked headers keep alive: " + std::to_string(ctx.keepAlive));
//	ctx.input_buffer.erase(0, header_end + 4);
//	ctx.headers_complete = true;

//	return true;
//}

bool Server::sendWrapper(Context& ctx, std::string http_response)
{
	// Attempt to send the response
	ssize_t bytes_sent = send(ctx.client_fd, http_response.c_str(), http_response.size(), MSG_NOSIGNAL);
	if (bytes_sent < 0) {
		Logger::errorLog("Error sending response to client_fd: " + std::to_string(ctx.client_fd) + " - " + std::string(strerror(errno)));
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
		return false;
	}

	if (bytes_sent == static_cast<ssize_t>(http_response.size())) {
		//Logger::file("Successfully sent full response to client_fd: " + std::to_string(ctx.client_fd));
		if (!ctx.keepAlive) {
			//Logger::file("Closing connection for client_fd: " + std::to_string(ctx.client_fd));
			delFromEpoll(ctx.epoll_fd, ctx.client_fd);
		}
	} else {
		//Logger::file("Partial response sent to client_fd: " + std::to_string(ctx.client_fd) + " - Sent: " + std::to_string(bytes_sent) + " of " + std::to_string(http_response.size()) + " bytes");
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
	}

	return true;
}
bool detectMultipartFormData(Context &ctx)
{
	if (ctx.headers["Content-Type"] == "Content-Type: multipart/form-data")
	{
		ctx.is_multipart = true;
		////Logger::file("[detectMultipartFormData] multipart/form-data erkannt.");
		return true;
	}

	ctx.is_multipart = false;
	////Logger::file("[detectMultipartFormData] Kein multipart/form-data.");
	return false;
}
bool Server::errorsHandler(Context& ctx, uint32_t ev)
{
	(void)ev;
	std::string errorResponse = getErrorHandler()->generateErrorResponse(ctx);
		//Logger::file("sendwrap in errorshandler");
	return (sendWrapper(ctx, errorResponse));
}

bool Server::staticHandler(Context& ctx)
{
	std::string response;
	if (!ctx.doAutoIndex.empty())
	{
		std::stringstream ss;
		buildAutoIndexResponse(ctx, &ss);
		if (ctx.type == ERROR)
			return false;
		response = ss.str();

		// Reset context for next request
		ctx.doAutoIndex = "";
		ctx.headers_complete = false;
		ctx.content_length = 0;
		ctx.body_received = 0;
		ctx.input_buffer.clear();
		ctx.body.clear();
		ctx.method.clear();
		ctx.path.clear();
		ctx.version.clear();
		ctx.headers.clear();
		ctx.type = INITIAL;
		//Logger::file(" before sendwrapper autoindex");
		return sendWrapper(ctx, response);
	}
	else
	{
		//Logger::file("before static resp");
		buildStaticResponse(ctx);
		if (ctx.type == ERROR)
			return false;
		//Logger::file("after static resp");
		//Logger::file("after static resp KEEP ALIVE!!!!: " + std::to_string(ctx.keepAlive));
		//if (!ctx.keepAlive)
		//{
		////Logger::file("REMOVEING EPOLL");
		//	delFromEpoll(ctx.epoll_fd, ctx.client_fd);
		//}
		return true;
	}
}

void Server::buildStaticResponse(Context &ctx) {
	// Konstruiere den vollständigen Dateipfad
	std::string fullPath = ctx.approved_req_path;
	//Logger::file("assciated full req path: " + fullPath);
	// Öffne und lese die Datei
	std::ifstream file(fullPath, std::ios::binary);
	if (!file) {
		ctx.type = ERROR;
		ctx.error_code = 404;
		ctx.error_message = "File not found";
		return;
	}

	// Bestimme den Content-Type basierend auf der Dateiendung
	std::string contentType = "text/plain";
	size_t dot_pos = fullPath.find_last_of('.');
	if (dot_pos != std::string::npos) {
		std::string ext = fullPath.substr(dot_pos + 1);
		if (ext == "html" || ext == "htm") contentType = "text/html";
		else if (ext == "css") contentType = "text/css";
		else if (ext == "js") contentType = "application/javascript";
		else if (ext == "jpg" || ext == "jpeg") contentType = "image/jpeg";
		else if (ext == "png") contentType = "image/png";
		else if (ext == "gif") contentType = "image/gif";
		else if (ext == "pdf") contentType = "application/pdf";
		else if (ext == "json") contentType = "application/json";
	}

	// Lese den Dateiinhalt
	std::stringstream content;
	content << file.rdbuf();
	std::string content_str = content.str();

	// Erstelle die HTTP Response
	std::stringstream http_response;
	http_response << "HTTP/1.1 200 OK\r\n"
				<< "Content-Type: " << contentType << "\r\n"
				<< "Content-Length: " << content_str.length() << "\r\n"
				<< "Connection: " << (ctx.keepAlive ? "keep-alive" : "close") << "\r\n"
				<< "\r\n"
				<< content_str;

	// Send response
			//Logger::file("sendwrap in buildstatic resp");
	sendWrapper(ctx, http_response.str());

	// Reset context für nächste Anfrage
	ctx.headers_complete = false;
	ctx.content_length = 0;
	ctx.body_received = 0;
	ctx.input_buffer.clear();
	ctx.body.clear();
	ctx.method.clear();
	ctx.path.clear();
	ctx.version.clear();
	ctx.headers.clear();
	ctx.type = INITIAL;
}

bool Server::queueResponse(Context& ctx, const std::string& response)
{
	ctx.output_buffer += response;
	modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
	return true;
}

bool Server::buildAutoIndexResponse(Context& ctx, std::stringstream* response) {

	//Logger::file("buildAutoIndexResponse: " + ctx.doAutoIndex);
	DIR* dir = opendir(ctx.doAutoIndex.c_str());
	if (!dir) {
		ctx.type = ERROR;
		ctx.error_code = 500;
		ctx.error_message = "Internal Server Error";
		return false;
	}

	std::vector<DirEntry> entries;
	struct dirent* dir_entry;
	while ((dir_entry = readdir(dir)) != NULL) {
		std::string name = dir_entry->d_name;
		//Logger::file("Found directory entry: " + name);
		if (name == "." || (name == ".." && ctx.location_path == "/"))
			continue;

		if (name == ".." && ctx.location_path != "/") {
			entries.push_back({"..", true, 0, 0});
			continue;
		}

		std::string fullPath = ctx.doAutoIndex;
		if (fullPath.back() != '/')
			fullPath += '/';
		fullPath += name;

		struct stat statbuf;
		if (stat(fullPath.c_str(), &statbuf) != 0) {
			Logger::errorLog("stat failed for path: " + fullPath + " with error: " + std::string(strerror(errno)));
			continue;
		}

		entries.push_back({
			name,
			S_ISDIR(statbuf.st_mode),
			statbuf.st_mtime,
			statbuf.st_size
		});
	}
	closedir(dir);
	//Logger::file("Before sorting, number of entries: " + std::to_string(entries.size()));

	std::sort(entries.begin(), entries.end(),
		[](const DirEntry& a, const DirEntry& b) -> bool {
			if (a.name == "..") return true;
			if (b.name == "..") return false;
			if (a.isDir != b.isDir) return a.isDir > b.isDir;
			return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
		});
	//Logger::file("After sorting, number of entries: " + std::to_string(entries.size()));

	std::stringstream content;
	content << "<!DOCTYPE html>\n"
			<< "<html>\n"
			<< "<head>\n"
			<< "    <meta charset=\"UTF-8\">\n"
			<< "    <title>Index of " << ctx.location_path << "</title>\n"
			<< "    <style>\n"
			<< "        body {\n"
			<< "            font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", Roboto, Arial, sans-serif;\n"
			<< "            max-width: 1200px;\n"
			<< "            margin: 0 auto;\n"
			<< "            padding: 20px;\n"
			<< "            color: #333;\n"
			<< "            user-select: none;\n"
			<< "        }\n"
			<< "        h1 {\n"
			<< "            border-bottom: 1px solid #eee;\n"
			<< "            padding-bottom: 10px;\n"
			<< "            color: #444;\n"
			<< "        }\n"
			<< "        .listing {\n"
			<< "            display: table;\n"
			<< "            width: 100%;\n"
			<< "            border-collapse: collapse;\n"
			<< "        }\n"
			<< "        .entry {\n"
			<< "            display: table-row;\n"
			<< "        }\n"
			<< "        .entry:hover {\n"
			<< "            background-color: #f5f5f5;\n"
			<< "        }\n"
			<< "        .entry > div {\n"
			<< "            display: table-cell;\n"
			<< "            padding: 8px;\n"
			<< "            border-bottom: 1px solid #eee;\n"
			<< "        }\n"
			<< "        .name { width: 50%; }\n"
			<< "        .modified {\n"
			<< "            width: 30%;\n"
			<< "            color: #666;\n"
			<< "        }\n"
			<< "        .size {\n"
			<< "            width: 20%;\n"
			<< "            text-align: right;\n"
			<< "            color: #666;\n"
			<< "        }\n"
			<< "        a {\n"
			<< "            color: #0366d6;\n"
			<< "            text-decoration: none;\n"
			<< "        }\n"
			<< "        a:hover { text-decoration: underline; }\n"
			<< "        .directory { color: #6f42c1; }\n"
			<< "    </style>\n"
			<< "</head>\n"
			<< "<body>\n"
			<< "    <h1>Index of " << ctx.location_path << "</h1>\n"
			<< "    <div class=\"listing\">\n";

	for (const auto& entry : entries) {
		std::string displayName = entry.name;
		std::string className = entry.isDir ? "directory" : "";

		char timeStr[50];
		if (entry.name == "..") {
			strcpy(timeStr, "-");
		} else {
			struct tm* tm = localtime(&entry.mtime);
			strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", tm);
		}

		std::string sizeStr;
		if (entry.isDir || entry.name == "..") {
			sizeStr = "-";
		} else {
			off_t size = entry.size;
			if (size < 1024)
				sizeStr = std::to_string(size) + "B";
			else if (size < 1024 * 1024)
				sizeStr = std::to_string(size / 1024) + "K";
			else if (size < 1024 * 1024 * 1024)
				sizeStr = std::to_string(size / (1024 * 1024)) + "M";
			else
				sizeStr = std::to_string(size / (1024 * 1024 * 1024)) + "G";
		}

		content << "        <div class=\"entry\">\n"
				<< "            <div class=\"name\">"
				<< "<a href=\"" << (entry.name == ".." ? "../" : entry.name + (entry.isDir ? "/" : "")) << "\" "
				<< "class=\"" << className << "\">"
				<< displayName << (entry.isDir ? "/" : "") << "</a></div>\n"
				<< "            <div class=\"modified\">" << timeStr << "</div>\n"
				<< "            <div class=\"size\">" << sizeStr << "</div>\n"
				<< "        </div>\n";
	}

	content << "    </div>\n"
			<< "</body>\n"
			<< "</html>\n";

	std::string content_str = content.str();

	*response << "HTTP/1.1 200 OK\r\n"
			<< "Content-Type: text/html; charset=UTF-8\r\n"
			<< "Connection: " << (ctx.keepAlive ? "keep-alive" : "close") << "\r\n"
			<< "Content-Length: " << content_str.length() << "\r\n\r\n"
			<< content_str;

	return true;
}
