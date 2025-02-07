#include "Server.hpp"

bool Server::handleNewConnection(int epoll_fd, int server_fd, std::vector<ServerBlock> &configs)
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
				break;
			}
			else
			{
				Logger::errorLog("Failed to accept connection: " + std::string(strerror(errno)) + " Server_fd: " +  std::to_string(server_fd));
				return false;
			}
		}

		Logger::file("New client_fd: " + std::to_string(client_fd) + " accepted on server_fd: " + std::to_string(server_fd));

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
		Logger::file("Bitte noch einen ordnetlichen Log vorher damit wir wissen wann das passiert dingesnd dahineter");
		log_global_fds(globalFDS);

		logRequestBodyMapFDs();
		//logContext(ctx, "New Connection");
	}
	//logContext(ctx, "Exit New Connection");
	return true;
}

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
			if (ctx.error_code != 0)
			{
				Logger::file("WRITE with error");
				result = errorsHandler(ctx);
			}

			break;
		case CGI:
			//Logger::file("WRITE with cgi");
			result = true;  // Handle CGI response
			if (ctx.error_code != 0)
				result = errorsHandler(ctx);
			break;
		case ERROR:
			result = errorsHandler(ctx);
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
		ctx.error_code = 0;
		if (ctx.keepAlive)
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
		else
			result = false;
	}
	if (result == false)
	{

		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
		Logger::file("handleWrite after result");
	}

	return result;
}

void Server::parseChunkedBody(Context& ctx)
{
	Logger::file("parseChunkedBody");
	while (!ctx.input_buffer.empty())
	{
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

			Logger::file("PARSING_BODY CHUNKED: content_length " + std::to_string(ctx.content_length));
			Logger::file("PARSING_BODY CHUNKED: current_body_length " + std::to_string(ctx.req.current_body_length));
			Logger::file("PARSING_BODY CHUNKED: client_max_body_size " + std::to_string(ctx.location.client_max_body_size));
		// Check for chunk end
			Logger::file("PARSING_BODY CHUNKED: ctx.req.current_body_length" + std::to_string(ctx.req.current_body_length));
			Logger::file("PARSING_BODY CHUNKED: ctx.req.expected_body_length" + std::to_string(ctx.req.expected_body_length));
		if (ctx.req.current_body_length == ctx.req.expected_body_length) {
			if (ctx.input_buffer.size() >= 2 && ctx.input_buffer.substr(0, 2) == "\r\n") {
				ctx.input_buffer.erase(0, 2);
				ctx.req.chunked_state.processing = false;
			} else {
			Logger::file("Need more data");
				return; // Need more data
			}
		}
	}
}

// bool Server::handleRead(Context& ctx, std::vector<ServerBlock> &configs)
// {
// 	char buffer[DEFAULT_REQUESTBUFFER_SIZE];
// 	ssize_t bytes;

// 	//Logger::errorLog("max body handleread() " + std::to_string(ctx.client_max_body_size));

// 	Logger::file("handleRead > hier nochmal pruefen.!!");
// 	bytes = read(ctx.client_fd, buffer, sizeof(buffer));
// 	if (bytes < 0)
// 	{
// 		Logger::errorLog("read fail, wait for enxt event;");
// 		return true;
// 	}
// 	else if (bytes == 0)
// 		return true;


// 	Logger::file("handleRead() - type: " + requestTypeToString(ctx.type));
// 	Logger::file("handleRead() - after bytes  ");
// 	// Update last activity time
// 	ctx.content_length = 0;
// 	ctx.last_activity = std::chrono::steady_clock::now();

// 	if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE)
// 	{
// 					Logger::file("handleRead() - PARSING_COMPLETE reset  ");
// 		ctx.input_buffer.clear();
// 		ctx.headers.clear();
// 		ctx.method.clear();
// 		ctx.path.clear();
// 		ctx.version.clear();
// 		ctx.headers_complete = false;
// 		ctx.content_length = 0;
// 		ctx.error_code = 0;
// 		ctx.req.parsing_phase = RequestBody::PARSING_HEADER;
// 		ctx.req.current_body_length = 0;
// 		ctx.req.expected_body_length = 0;
// 		ctx.req.received_body.clear();
// 		ctx.req.chunked_state.processing = false;
// 		ctx.req.is_upload_complete = false;
// 		ctx.type = RequestType::INITIAL;
// 	}
// 	// Append new data to input buffer
// 	ctx.input_buffer.append(buffer, bytes);

// 	// Handle based on current parsing phase
// 	switch(ctx.req.parsing_phase) {
// 		case RequestBody::PARSING_HEADER:
// 			if (!ctx.headers_complete) {
// 				if (!parseHeaders(ctx)) {
// 					return true;
// 				}
// 					Logger::file("handleRead() - parseHeaders  ");
// 			auto headersit = ctx.headers.find("Content-Length");
// 			if (headersit != ctx.headers.end())
// 			{
// 					Logger::file("handleRead() -headersit  ");
// 				long long content_length = std::stoull(headersit->second);
// 					Logger::file("handleRead() - content_length " + std::to_string(content_length));

// 				getMaxBodySizeFromConfig(ctx, configs);
// 				Logger::file("clientmax: " + std::to_string(ctx.client_max_body_size));
// 				if (content_length > ctx.client_max_body_size)
// 				{
// 					Logger::file("handleRead() - Content length exceeds limit: " + std::to_string(content_length));
// 					Logger::errorLog("Max payload: " + std::to_string(ctx.client_max_body_size) + " Content length: " + std::to_string(content_length));
// 					return updateErrorStatus(ctx, 413, "Payload too large");
// 				}
// 			}
// 				auto it = ctx.headers.find("Transfer-Encoding");
// 				if (it != ctx.headers.end() && it->second == "chunked")
// 				{
// 					ctx.req.parsing_phase = RequestBody::PARSING_BODY;
// 					ctx.req.chunked_state.processing = true;
// 					if (!ctx.input_buffer.empty()) {
// 						parseChunkedBody(ctx);
// 					}
// 				}
// 				else if (ctx.content_length > 0 && ctx.headers_complete)
// 				{
// 					if (ctx.content_length > ctx.client_max_body_size)
// 					{
// 						logContext(ctx, "Max payghvjgjghjload");
// 						log_global_fds(globalFDS);
// 						Logger::errorLog("MAx payload: " + std::to_string(ctx.client_max_body_size) + " Content length: " + std::to_string(ctx.content_length));
// 						return updateErrorStatus(ctx, 413, "Payload too large");
// 					}
// 					ctx.req.parsing_phase = RequestBody::PARSING_BODY;
// 					ctx.req.expected_body_length = ctx.content_length;
// 					ctx.req.current_body_length = 0;

// 					// Process any remaining data in input buffer as body
// 					if (!ctx.input_buffer.empty()) {
// 						ctx.req.received_body += ctx.input_buffer;
// 						ctx.req.current_body_length += ctx.input_buffer.length();
// 						ctx.input_buffer.clear();

// 						// Check if we've received the complete body
// 						if (ctx.req.current_body_length >= ctx.req.expected_body_length) {
// 							ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
// 							ctx.req.is_upload_complete = true;
// 						}
// 					}
// 				} else {
// 					ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
// 				}
// 			}
// 			break;

// 		case RequestBody::PARSING_BODY:
// 			if (ctx.req.chunked_state.processing) {
// 				parseChunkedBody(ctx);
// 			} else {
// 			Logger::file("PARSING_BODY: content_length " + std::to_string(ctx.content_length));
// 			Logger::file("PARSING_BODY: current_body_length " + std::to_string(ctx.req.current_body_length));
// 			Logger::file("PARSING_BODY: client_max_body_size " + std::to_string(ctx.location.client_max_body_size));
// 				// Standard body processing
// 				ctx.req.received_body += ctx.input_buffer;
// 				ctx.req.current_body_length += ctx.input_buffer.length();
// 				ctx.input_buffer.clear();

// 				if (ctx.req.current_body_length >= ctx.req.expected_body_length) {
// 					ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
// 					ctx.req.is_upload_complete = true;

// 					// Trim excess data if any
// 					if (ctx.req.current_body_length > ctx.req.expected_body_length) {
// 						ctx.req.received_body = ctx.req.received_body.substr(
// 							0, ctx.req.expected_body_length);
// 						ctx.req.current_body_length = ctx.req.expected_body_length;
// 					}
// 				}
// 			}
// 			break;

// 		case RequestBody::PARSING_COMPLETE:
// 			break;
// 	}

// 	// Handle request completion
// 	if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE) {
// 				logContext(ctx, "ich haue meines penis auf marvins glatze");
// 		determineType(ctx, configs);
// 		logContext(ctx, "ich haue meines penis auf marvins popo");
// 		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
// 	} else if (ctx.error_code != 0)
// 	{
// 		// Still need more data, ensure we're watching for it
// 		Logger::errorLog("epoll to in");
// 		return false;
// 	}

// 	return true;
// }














bool Server::resetContext(Context& ctx) {
	Logger::file("resetContext");
	ctx.input_buffer.clear();
	ctx.headers.clear();
	ctx.method.clear();
	ctx.path.clear();
	ctx.version.clear();
	ctx.headers_complete = false;
	ctx.content_length = 0;
	ctx.error_code = 0;
	ctx.req.parsing_phase = RequestBody::PARSING_HEADER;
	ctx.req.current_body_length = 0;
	ctx.req.expected_body_length = 0;
	ctx.req.received_body.clear();
	ctx.req.chunked_state.processing = false;
	ctx.req.is_upload_complete = false;
	ctx.type = RequestType::INITIAL;
	return true;
}

bool Server::handleContentLength(Context& ctx, const std::vector<ServerBlock>& configs) {
	Logger::file("handleContentLength");
	auto headersit = ctx.headers.find("Content-Length");
	if (headersit != ctx.headers.end()) {
		long long content_length = std::stoull(headersit->second);
		getMaxBodySizeFromConfig(ctx, configs);
		if (content_length > ctx.client_max_body_size) {
			Logger::file("handleRead() - Content length exceeds limit: " + std::to_string(content_length));
			Logger::errorLog("Max payload: " + std::to_string(ctx.client_max_body_size) + " Content length: " + std::to_string(content_length));
			return updateErrorStatus(ctx, 413, "Payload too large");
		}
	}
	return true;
}

bool Server::handleTransferEncoding(Context& ctx) {
	Logger::file("handleTransferEncoding");
	auto it = ctx.headers.find("Transfer-Encoding");
	if (it != ctx.headers.end() && it->second == "chunked") {
		ctx.req.parsing_phase = RequestBody::PARSING_BODY;
		ctx.req.chunked_state.processing = true;
		if (!ctx.input_buffer.empty()) {
			parseChunkedBody(ctx);
		}
		return true;
	}
	return false;
}

bool Server::handleStandardBody(Context& ctx) {
	Logger::file("handleStandardBody");
	if (ctx.content_length > 0 && ctx.headers_complete) {
		if (ctx.content_length > ctx.client_max_body_size) {
			logContext(ctx, "Max payghvjgjghjload");
			log_global_fds(globalFDS);
			Logger::errorLog("MAx payload: " + std::to_string(ctx.client_max_body_size) + " Content length: " + std::to_string(ctx.content_length));
			return updateErrorStatus(ctx, 413, "Payload too large");
		}

		ctx.req.parsing_phase = RequestBody::PARSING_BODY;
		ctx.req.expected_body_length = ctx.content_length;
		ctx.req.current_body_length = 0;

		if (!ctx.input_buffer.empty()) {
			ctx.req.received_body += ctx.input_buffer;
			ctx.req.current_body_length += ctx.input_buffer.length();
			ctx.input_buffer.clear();

			if (ctx.req.current_body_length >= ctx.req.expected_body_length) {
				ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
				ctx.req.is_upload_complete = true;
			}
		}
		return true;
	}
	ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
	return true;
}

bool Server::processParsingBody(Context& ctx) {
	Logger::file("processParsingBody");
	if (ctx.req.chunked_state.processing) {
		parseChunkedBody(ctx);
	} else {
		Logger::file("PARSING_BODY: content_length " + std::to_string(ctx.content_length));
		Logger::file("PARSING_BODY: current_body_length " + std::to_string(ctx.req.current_body_length));
		Logger::file("PARSING_BODY: client_max_body_size " + std::to_string(ctx.client_max_body_size));
		Logger::file("PARSING_BODY: client_max_body_size " + std::to_string(ctx.location.client_max_body_size));

		ctx.req.received_body += ctx.input_buffer;
		ctx.req.current_body_length += ctx.input_buffer.length();
		ctx.input_buffer.clear();

		if (ctx.req.current_body_length >= ctx.req.expected_body_length) {
			ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
			ctx.req.is_upload_complete = true;

			if (ctx.req.current_body_length > ctx.req.expected_body_length) {
				ctx.req.received_body = ctx.req.received_body.substr(0, ctx.req.expected_body_length);
				ctx.req.current_body_length = ctx.req.expected_body_length;
			}
		}
	}
	return true;
}

bool Server::handleParsingPhase(Context& ctx, const std::vector<ServerBlock>& configs) {
	Logger::file("handleParsingPhase");
	switch(ctx.req.parsing_phase) {
		case RequestBody::PARSING_HEADER:
			Logger::file("in PARSING_HEADER");
			if (!ctx.headers_complete) {
				if (!parseHeaders(ctx, configs)) {
					return true;
				}
				if (!handleContentLength(ctx, configs)) return false;
				if (!handleTransferEncoding(ctx)) {
					return handleStandardBody(ctx);
				}
			}
			break;

		case RequestBody::PARSING_BODY:
			Logger::file("in PARSING_BODY");
			return processParsingBody(ctx);

		case RequestBody::PARSING_COMPLETE:
			Logger::file("in PARSING_COMPLETE");
			break;
	}
	return true;
}

bool Server::finalizeRequest(Context& ctx, const std::vector<ServerBlock>& configs) {
	Logger::file("finalizeRequest");
	if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE) {
		logContext(ctx, "ich haue meines penis auf marvins glatze");
		determineType(ctx, configs);
		logContext(ctx, "ich haue meines penis auf marvins popo");
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
	} else if (ctx.error_code != 0) {
		Logger::errorLog("epoll to in");
		return false;
	}
	return true;
}

bool Server::handleRead(Context& ctx, std::vector<ServerBlock>& configs) {
    char buffer[DEFAULT_REQUESTBUFFER_SIZE];
    ssize_t bytes = read(ctx.client_fd, buffer, sizeof(buffer));

    if (bytes < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            Logger::errorLog("Read error: " + std::string(strerror(errno)));
            return false;
        }
        return true;  // Wait for next EPOLLIN event
    }

    if (bytes == 0) {
        return true;  // Connection closed normally
    }

    ctx.last_activity = std::chrono::steady_clock::now();

    if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE) {
        resetContext(ctx);
    }

    ctx.input_buffer.append(buffer, bytes);

    if (!handleParsingPhase(ctx, configs)) {
        return false;
    }

    // Stay in EPOLLIN until we get all the data
    if (ctx.req.parsing_phase == RequestBody::PARSING_BODY &&
        ctx.req.current_body_length < ctx.req.expected_body_length) {
        modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
        return true;
    }

    return finalizeRequest(ctx, configs);
}













	int extractPort(const std::string& header) {
	Logger::file("Starting port extraction from header");
	size_t host_pos = header.find("Host: ");
	if (host_pos != std::string::npos) {
		Logger::file("Found Host header");
		size_t port_pos = header.find(":", host_pos + 6);
		if (port_pos != std::string::npos) {
		size_t end_pos = header.find("\r\n", port_pos);
		if (end_pos != std::string::npos) {
			std::string port_str = header.substr(port_pos + 1, end_pos - port_pos - 1);
			port_str = trim(port_str);
			Logger::file("Extracted port string: '" + port_str + "'");
			try {
			int port = std::stoi(port_str);
			// Valid port range check instead of isalnum
			if (port <= 0 || port > 65535) {
				Logger::red("Invalid port number: '" + port_str + "'");
				Logger::errorLog("Port number out of valid range (1-65535)");
				return -1;
			}
			Logger::file("Valid port found: " + std::to_string(port));
			return port;
			} catch (const std::exception& e) {
			Logger::file("Failed to convert port string to integer: " + std::string(e.what()));
			return -1;
			}
		}
		} else {
		Logger::file("No port specified, using default port 80");
		return 80;
		}
	}
	Logger::file("No Host header found");
	return -1;
	}
	std::string extractHostname(const std::string& header) {
	std::string hostname;
	size_t host_pos = header.find("Host: ");
	if (host_pos != std::string::npos) {
		size_t start = host_pos + 6;
		size_t end_pos = header.find("\r\n", start);
		if (end_pos != std::string::npos) {
		size_t colon_pos = header.find(":", start);
		if (colon_pos != std::string::npos && colon_pos < end_pos) {
			hostname = header.substr(start, colon_pos - start);
		} else {
			hostname = header.substr(start, end_pos - start);
		}
		}
	}
	return hostname; // Returns empty string if no Host header found
	}

void Server::parseNewCookie(Context& ctx, std::string value)
{
	Logger::errorLog("parseNewCookie: " + value);
    ctx.setCookie = value;
}

void Server::parseCookies(Context& ctx, std::string value)
{
	Logger::errorLog("parseCookies: " + value);
	ctx.cookies = value;
}

bool Server::parseHeaders(Context& ctx, const std::vector<ServerBlock>& configs)
{
    size_t header_end = ctx.input_buffer.find("\r\n\r\n");
    if (header_end == std::string::npos) {
        return true;
    }

    std::string headers = ctx.input_buffer.substr(0, header_end);
    std::istringstream stream(headers);
    std::string line;

    if (!std::getline(stream, line)) {
        updateErrorStatus(ctx, 400, "Bad Request - Empty Request");
        return false;
    }

    if (line.back() == '\r') line.pop_back();
    std::istringstream request_line(line);
    request_line >> ctx.method >> ctx.path >> ctx.version;
    if (ctx.method.empty() || ctx.path.empty() || ctx.version.empty()) {
        updateErrorStatus(ctx, 400, "Bad Request - Invalid Request Line");
        return false;
    }

    while (std::getline(stream, line)) {
        if (line.empty() || line == "\r") break;
        if (line.back() == '\r') line.pop_back();

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            value = value.substr(value.find_first_not_of(" "));
            ctx.headers[key] = value;

            if (key == "Content-Length") {
                try {
                    ctx.content_length = std::stoull(value);
                } catch (const std::exception& e) {
                    updateErrorStatus(ctx, 400, "Bad Request - Invalid Content-Length");
                    return false;
                }
            }
            ctx.setCookie = "";
            ctx.cookies.clear();
            if (key == "Set-Cookie") {
                parseNewCookie(ctx, value);
            }
            if (key == "Cookie") {
                parseCookies(ctx, value);
            }
        }
    }

    std::string requested_host = extractHostname(ctx.input_buffer);
    int requested_port = extractPort(ctx.input_buffer);
    Logger::file("\n" + ctx.input_buffer);
    Logger::file("Parsed request - Host: " + requested_host + " Port: " + std::to_string(requested_port));
    bool server_match_found = false;
    for (const auto &config : configs)
    {
		if (((config.name == requested_host && config.port == requested_port) ||
			(config.name == "localhost" && (requested_host == "localhost" || requested_host == "127.0.0.1")) ||
			(requested_host.empty() && config.port == requested_port)))
		{
			Logger::file("configname: " + config.name + " requested_host: " + requested_host);
			server_match_found = true;
			break;
		}
    }
    if (server_match_found)
    {
        Logger::file("Connection accepted - Server: " + requested_host + " Port: " + std::to_string(requested_port));
    }
    else
    {
		Logger::errorLog("No matching server block found - Closing connection");
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
        return false;
    }

    ctx.input_buffer.erase(0, header_end + 4);
    ctx.headers_complete = true;

    if (ctx.method == "POST" && ctx.headers.find("Content-Length") == ctx.headers.end() &&
        ctx.headers.find("Transfer-Encoding") == ctx.headers.end()) {
        updateErrorStatus(ctx, 411, "Length Required");
        return false;
    }

    return true;
}

bool Server::sendWrapper(Context& ctx, std::string http_response)
{
	// Attempt to send the response
	Logger::file("sendwrapper()");
	ssize_t bytes_sent = send(ctx.client_fd, http_response.c_str(), http_response.size(), MSG_NOSIGNAL);
	if (bytes_sent < 0) {
		Logger::errorLog("Error sending response to client_fd: " + std::to_string(ctx.client_fd) + " - " + std::string(strerror(errno)));
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
		return false;
	}
	if (ctx.type == ERROR || !ctx.keepAlive)
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);

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
bool Server::errorsHandler(Context& ctx)
{
	std::string errorResponse = getErrorHandler()->generateErrorResponse(ctx);
	Logger::errorLog("ErrorHandler: Errorcode: " + std::to_string(ctx.error_code) + " " + ctx.error_message);
	return (sendWrapper(ctx, errorResponse));
}

bool Server::redirectAction(Context& ctx)
{
    int return_code = 301;
    std::string return_url = "/";

    if (!ctx.location.return_code.empty())
        return_code = std::stoi(ctx.location.return_code);  // Convert string to int
    if (!ctx.location.return_url.empty())
        return_url = ctx.location.return_url;

    // Use string streams for proper string concatenation
    std::stringstream log_message;
    log_message << "Handling redirect: code=" << return_code << " url=" << return_url;
    Logger::file(log_message.str());

    // Prepare redirect response
    std::stringstream response;
    response << "HTTP/1.1 " << return_code << " Moved Permanently\r\n"
            << "Location: " << return_url << "\r\n"
            << "Connection: " << (ctx.keepAlive ? "keep-alive" : "close") << "\r\n"
            << "Content-Length: 0\r\n"
            << "\r\n";

    // Send the redirect response
    if (!sendWrapper(ctx, response.str())) {
        Logger::errorLog("Failed to send redirect response");
        return false;
    }

    // Reset the context for potential next request if keep-alive
    if (ctx.keepAlive) {
        ctx.input_buffer.clear();
        ctx.body.clear();
        ctx.headers_complete = false;
        ctx.content_length = 0;
        ctx.body_received = 0;
        ctx.headers.clear();
        ctx.error_code = 0;
        ctx.method.clear();
        ctx.path.clear();
        ctx.version.clear();
        ctx.type = INITIAL;

        // Modify epoll to watch for next request
        modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
    }

    return true;
}

bool Server::handleStaticUpload(Context& ctx)
{
	Logger::file("here is upload static function" + std::to_string(ctx.client_max_body_size));
	// Extract filename
	std::regex filenameRegex(R"(Content-Disposition:.*filename=\"([^\"]+)\")");
	std::smatch matches;

	std::string filename;
	if (std::regex_search(ctx.req.received_body, matches, filenameRegex) && matches.size() > 1) {
		filename = matches[1].str();
	} else {
		return updateErrorStatus(ctx, 422, "Unprocessable Entity");
	}

	// Find content boundaries
	size_t contentStart = ctx.req.received_body.find("\r\n\r\n");
	if (contentStart == std::string::npos) {
		Logger::errorLog("[Upload] ERROR: Failed to find content start boundary");
		return updateErrorStatus(ctx, 422, "Unprocessable Entity");
	}
	contentStart += 4;

	size_t contentEnd = ctx.req.received_body.rfind("\r\n--");
	if (contentEnd == std::string::npos) {
		Logger::errorLog("[Upload] ERROR: Failed to find content end boundary");
		return false;
	}

	// Extract content
	std::string fileContent = ctx.req.received_body.substr(contentStart, contentEnd - contentStart);
	////Logger::file("[Upload] Extracted file content size: " + std::to_string(fileContent.size()));

	// Build full path and save
	std::string uploadPath = concatenatePath(ctx.location_path, ctx.location.upload_store);
	if (!dirWritable(uploadPath))
	{
		return updateErrorStatus(ctx, 403, "Forbidden");
	}
	std::string filePath = uploadPath + "/" + filename;

	std::ofstream outputFile(filePath, std::ios::binary);
	if (!outputFile) {
		Logger::errorLog("[Upload] ERROR: Failed to open output file: " + filePath);
		return false;
	}

	outputFile.write(fileContent.c_str(), fileContent.size());
	outputFile.close();

	if (outputFile) {
		Logger::file("[Upload] File successfully saved");
		return redirectAction(ctx);
	} else {
		Logger::file("[Upload] ERROR: Failed to write file");
	}
	// redirect
	return false;
}

bool Server::deleteHandler(Context &ctx)
{
    // Log the current path and upload store location
    Logger::file("path: " + ctx.path);
    Logger::file("upload_store: " + ctx.location.upload_store);

    std::string filename = mergePathsToFilename(ctx.path, ctx.location.upload_store);
    if (filename.empty()) {
        return updateErrorStatus(ctx, 404, "Not found");
    }
	filename = concatenatePath(ctx.location.upload_store, filename);

    Logger::file("filename: " + filename);
	if (dirReadable(ctx.location.upload_store) && dirWritable(ctx.location.upload_store))
	{
		if (geteuid() == 0)
		{
			Logger::blue("[WARNING] Running as root - access() will not reflect real permissions!");
		}
		else if (access(filename.c_str(), W_OK) != 0)
		{
			Logger::file("[ERROR] Write permission denied for file: " + filename);
			return updateErrorStatus(ctx, 403, "Forbidden");
		}
		Logger::file("is okayyyyyy");
	}

		Logger::file("deleting file: " + filename);
	if (unlink(filename.c_str()) != 0)
	{
		return updateErrorStatus(ctx, 500, "Internal Server Error");
	}
	return true;
}

bool Server::staticHandler(Context& ctx)
{
	//if (ctx.return == "irgendwas mit re di rect ")
	//{
	//	Logger::file("in static POST Response generating");
	//}
	//else
	Logger::file("tsdfdsfdsfype " + requestTypeToString(ctx.type));
	Logger::file("METHOD " + ctx.method);
	if (ctx.method == "POST")
	{
		if (!handleStaticUpload(ctx))
		{
			return false;
		}
		return true;
	}
	else if (ctx.method == "GET")
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
			ctx.error_code = 0;
			return sendWrapper(ctx, response);
		}
		else
		{
			buildStaticResponse(ctx);
			if (ctx.type == ERROR)
				return false;
			if (!ctx.keepAlive)
			{
			//delFromEpoll(ctx.epoll_fd, ctx.client_fd);
			}
			return true;
		}
	}
	else if (ctx.method == "DELETE")
	{
	Logger::file("DELETE deleteHandler()");
		return deleteHandler(ctx);
	}
	return false;
}

void Server::buildStaticResponse(Context &ctx)
{
	// Konstruiere den vollständigen Dateipfad
	std::string fullPath = ctx.approved_req_path;
	//Logger::file("assciated full req path: " + fullPath);
	// Öffne und lese die Datei
	std::ifstream file(fullPath, std::ios::binary);
	if (!file) {
		Logger::file("buildStaticResponse");
		updateErrorStatus(ctx, 404, "Not found");
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
	ctx.error_code= 0;
	ctx.type = INITIAL;
}

bool Server::queueResponse(Context& ctx, const std::string& response)
{
	ctx.output_buffer += response;
	modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
	return true;
}

bool Server::buildAutoIndexResponse(Context& ctx, std::stringstream* response)
{

	//Logger::file("buildAutoIndexResponse: " + ctx.doAutoIndex);
	DIR* dir = opendir(ctx.doAutoIndex.c_str());
	if (!dir) {
		return updateErrorStatus(ctx, 500, "Internal Server Error");
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

	for (const auto& entry : entries)
	{
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
