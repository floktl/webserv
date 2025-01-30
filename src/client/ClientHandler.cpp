#include "ClientHandler.hpp"

ClientHandler::ClientHandler(Server& _server) : server(_server) {}

//void ClientHandler::handleClientRead(int epfd, int fd)
//{
//	auto &fds = server.getGlobalFds();
//	auto it = fds.context_map.find(fd);

//	if (it == fds.context_map.end())
//	{
//		////Logger::file("[handleClientRead] Invalid fd: " + std::to_string(fd) + " not found in context_map.");
//		server.delFromEpoll(epfd, fd);
//		return;
//	}

//	RequestBody &req = it->second;
//	char buf[1024];

//	while (true)
//	{
//		ssize_t n = read(fd, buf, sizeof(buf));

//		if (n == 0)
//		{
//			////Logger::file("[handleClientRead] Client disconnected on fd: " + std::to_string(fd));
//			server.delFromEpoll(epfd, fd);
//			return;
//		}

//		if (n < 0)
//		{
//			if (errno == EAGAIN || errno == EWOULDBLOCK)
//				break;
//			else
//			{
//				////Logger::file("[handleClientRead] read error: "  + std::string(strerror(errno)));
//				server.delFromEpoll(epfd, fd);
//				return;
//			}
//		}

//		req.request_buffer.insert(req.request_buffer.end(), buf, buf + n);

//		try
//		{
//			server.getRequestHandler()->parseRequest(req);
//		}
//		catch (const std::exception &e)
//		{
//			////Logger::file("[handleClientRead] Error parsing request on fd: " + std::to_string(fd) + " - " + e.what());
//			server.delFromEpoll(epfd, fd);
//			return;
//		}

//	if (!req.clientMethodChecked) {

//		if (!processMethod(req, epfd))
//		{
//			req.clientMethodChecked = true;
//		}
//	}
//		if (req.method == "POST" && req.received_body.size() < req.content_length)
//		{
//			////Logger::file("[handleClientRead] POST body incomplete. Waiting for more data. " + "Received: " + std::to_string(req.received_body.size()) + ", Expected: " + std::to_string(req.content_length));
//			server.modEpoll(epfd, fd, EPOLLIN);
//			break;
//		}
//	}
//}

//void ClientHandler::handleClientWrite(int epfd, int fd)
//{
//	RequestBody &req = server.getGlobalFds().context_map[fd];

//	auto now = std::chrono::steady_clock::now();
//	if (now - req.last_activity > RequestBody::TIMEOUT_DURATION)
//	{
//		////Logger::file("[handleClientWrite] Timeout reached for fd: "  + std::to_string(fd));
//		server.delFromEpoll(epfd, fd);
//		return;
//	}

//	if (req.state == RequestBody::STATE_SENDING_RESPONSE)
//	{
//		int error = 0;
//		socklen_t len = sizeof(error);
//		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0)
//		{
//			////Logger::file("[handleClientWrite] getsockopt error on fd: " + std::to_string(fd));
//			server.delFromEpoll(epfd, fd);
//			return;
//		}

//		if (!req.response_buffer.empty())
//		{
//			char send_buffer[8192];
//			size_t chunk_size = std::min(req.response_buffer.size(),
//										sizeof(send_buffer));

//			std::copy(req.response_buffer.begin(),
//					req.response_buffer.begin() + chunk_size,
//					send_buffer);

//			ssize_t n = send(fd, send_buffer, chunk_size, MSG_NOSIGNAL);

//			if (n > 0)
//			{
//				req.response_buffer.erase(
//					req.response_buffer.begin(),
//					req.response_buffer.begin() + n
//				);

//				req.last_activity = now;

//				if (req.response_buffer.empty())
//				{
//					////Logger::file("[handleClientWrite] Response fully sent. Removing fd: " + std::to_string(fd) + " from epoll.");
//					server.delFromEpoll(epfd, fd);
//				}
//				else
//				{
//					//////Logger::file("[handleClientWrite] Partially sent response. " + "Still have " + std::to_string(req.response_buffer.size())+ " bytes left to send on fd: " + std::to_string(fd));
//					server.modEpoll(epfd, fd, EPOLLOUT);
//				}
//			}
//			else if (n < 0)
//			{
//				if (errno != EAGAIN && errno != EWOULDBLOCK)
//				{
//					////Logger::file("[handleClientWrite] send() error on fd: " + std::to_string(fd) + ": " + std::string(strerror(errno)));
//					server.delFromEpoll(epfd, fd);
//				}
//				else
//				{
//					// Wieder versuchen
//					server.modEpoll(epfd, fd, EPOLLOUT);
//				}
//			}
//		}
//	}
//}


//bool ClientHandler::processMethod(RequestBody &req, int epoll_fd)
//{

//	////Logger::file("[processMethod] Extracted method: " + req.method);

//	if (!isMethodAllowed(req, req.method))
//	{
//		std::string error_response = server.getErrorHandler()->generateErrorResponse(405, "Method Not Allowed", req);

//		if (error_response.length() > req.response_buffer.max_size())
//		{
//			error_response =
//				"HTTP/1.1 405 Method Not Allowed\r\n"
//				"Content-Type: text/plain\r\n"
//				"Content-Length: 18\r\n"
//				"Connection: close\r\n\r\n"
//				"Method not allowed.";
//		}

//		req.response_buffer.clear();
//		req.response_buffer.insert(req.response_buffer.begin(),
//								error_response.begin(),
//								error_response.end());

//		req.state = RequestBody::STATE_SENDING_RESPONSE;
//		server.modEpoll(epoll_fd, req.client_fd, EPOLLOUT);
//		return false;
//	}

//	return true;
//}

//void Server::parseChunkedBody(Context& ctx)
//{
//    Logger::file("parseChunkedBody");
//    while (!ctx.input_buffer.empty())
//    {
//        if (ctx.req.current_body_length > ctx.client_max_body_size) {
//            ctx.error_code = 413;
//            ctx.error_message = "Payload too large";
//            return;
//        }

//        if (!ctx.req.chunked_state.processing) {
//            size_t pos = ctx.input_buffer.find("\r\n");
//            if (pos == std::string::npos) {
//                return;
//            }

//            std::string chunk_size_str = ctx.input_buffer.substr(0, pos);
//            size_t chunk_size;
//            std::stringstream ss;
//            ss << std::hex << chunk_size_str;
//            ss >> chunk_size;

//            if (chunk_size + ctx.req.current_body_length > ctx.client_max_body_size) {
//                ctx.error_code = 413;
//                ctx.error_message = "Payload too large";
//                return;
//            }

//            if (chunk_size == 0) {
//                ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
//                ctx.req.is_upload_complete = true;
//                ctx.input_buffer.clear();
//                return;
//            }

//            ctx.input_buffer.erase(0, pos + 2);
//            ctx.req.chunked_state.buffer = "";
//            ctx.req.chunked_state.processing = true;
//            ctx.req.expected_body_length += chunk_size;
//        }

//        size_t remaining = ctx.req.expected_body_length - ctx.req.current_body_length;
//        size_t available = std::min(remaining, ctx.input_buffer.size());

//        if (available > 0) {
//            ctx.req.received_body += ctx.input_buffer.substr(0, available);
//            ctx.req.current_body_length += available;
//            ctx.input_buffer.erase(0, available);
//        }

//        if (ctx.req.current_body_length == ctx.req.expected_body_length) {
//            if (ctx.input_buffer.size() >= 2 && ctx.input_buffer.substr(0, 2) == "\r\n") {
//                ctx.input_buffer.erase(0, 2);
//                ctx.req.chunked_state.processing = false;
//            } else {
//                return;
//            }
//        }
//    }
//}

//bool Server::parseHeaders(Context& ctx)
//{
//    size_t header_end = ctx.input_buffer.find("\r\n\r\n");
//    if (header_end == std::string::npos) {
//        return true;
//    }

//    std::string headers = ctx.input_buffer.substr(0, header_end);
//    std::istringstream stream(headers);
//    std::string line;

//    if (!std::getline(stream, line)) {
//        updateErrorStatus(ctx, 400, "Bad Request - Empty Request");
//        return false;
//    }

//    if (line.back() == '\r') line.pop_back();
//    std::istringstream request_line(line);
//    request_line >> ctx.method >> ctx.path >> ctx.version;

//    if (ctx.method.empty() || ctx.path.empty() || ctx.version.empty()) {
//        updateErrorStatus(ctx, 400, "Bad Request - Invalid Request Line");
//        return false;
//    }

//    while (std::getline(stream, line)) {
//        if (line.empty() || line == "\r") break;
//        if (line.back() == '\r') line.pop_back();

//        size_t colon = line.find(':');
//        if (colon != std::string::npos) {
//            std::string key = line.substr(0, colon);
//            std::string value = line.substr(colon + 1);
//            value = value.substr(value.find_first_not_of(" "));
//            ctx.headers[key] = value;

//            if (key == "Content-Length") {
//                try {
//                    ctx.content_length = std::stoull(value);
//                } catch (const std::exception& e) {
//                    updateErrorStatus(ctx, 400, "Bad Request - Invalid Content-Length");
//                    return false;
//                }
//            }
//        }
//    }

//    ctx.input_buffer.erase(0, header_end + 4);
//    ctx.headers_complete = true;

//    if (ctx.method == "POST" && ctx.headers.find("Content-Length") == ctx.headers.end() &&
//        ctx.headers.find("Transfer-Encoding") == ctx.headers.end()) {
//        updateErrorStatus(ctx, 411, "Length Required");
//        return false;
//    }

//    return true;
//}

//bool Server::handleRead(Context& ctx, std::vector<ServerBlock> &configs)
//{
//    char buffer[DEFAULT_REQUESTBUFFER_SIZE];
//    ssize_t bytes = read(ctx.client_fd, buffer, sizeof(buffer));

//    if (bytes < 0) {
//        Logger::errorLog("read fail, wait for next event");
//        return true;
//    }
//    else if (bytes == 0) {
//        return true;
//    }

//    Logger::file("handleRead() - type: " + requestTypeToString(ctx.type));
//    ctx.last_activity = std::chrono::steady_clock::now();

//    if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE) {
//        ctx.input_buffer.clear();
//        ctx.headers.clear();
//        ctx.method.clear();
//        ctx.path.clear();
//        ctx.version.clear();
//        ctx.headers_complete = false;
//        ctx.content_length = 0;
//        ctx.error_code = 0;
//        ctx.req.parsing_phase = RequestBody::PARSING_HEADER;
//        ctx.req.current_body_length = 0;
//        ctx.req.expected_body_length = 0;
//        ctx.req.received_body.clear();
//        ctx.req.chunked_state.processing = false;
//        ctx.req.is_upload_complete = false;
//        ctx.type = RequestType::INITIAL;
//    }

//    ctx.input_buffer.append(buffer, bytes);

//    switch(ctx.req.parsing_phase) {
//        case RequestBody::PARSING_HEADER:
//            if (!ctx.headers_complete) {
//                if (!parseHeaders(ctx)) {
//					Logger::file("method line 338: ");
//					Logger::file("method line 338: " + ctx.method);

//                    return true;
//                }
//				Logger::file("method line 340: " + ctx.method);
//				Logger::file("method line 340: "d);
//                if (ctx.headers_complete) {
//                    auto headersit = ctx.headers.find("Content-Length");
//                    if (headersit != ctx.headers.end()) {
//                        try {
//                            long long content_length = std::stoull(headersit->second);
//                            getMaxBodySizeFromConfig(ctx, configs);

//                            if (content_length > ctx.client_max_body_size) {
//                                Logger::errorLog("Max payload: " + std::to_string(ctx.client_max_body_size) +
//                                               " Content length: " + std::to_string(content_length));
//                                return updateErrorStatus(ctx, 413, "Payload too large");
//                            }
//                        } catch (const std::exception& e) {
//                            return updateErrorStatus(ctx, 400, "Invalid Content-Length");
//                        }
//                    }

//                    auto it = ctx.headers.find("Transfer-Encoding");
//                    if (it != ctx.headers.end() && it->second == "chunked") {
//                        ctx.req.parsing_phase = RequestBody::PARSING_BODY;
//                        ctx.req.chunked_state.processing = true;
//                        if (!ctx.input_buffer.empty()) {
//                            parseChunkedBody(ctx);
//                            if (ctx.error_code != 0) {
//                                return false;
//                            }
//                        }
//                    }
//                    else if (ctx.content_length > 0) {
//                        ctx.req.parsing_phase = RequestBody::PARSING_BODY;
//                        ctx.req.expected_body_length = ctx.content_length;

//                        if (!ctx.input_buffer.empty()) {
//                            size_t to_process = std::min(ctx.input_buffer.length(),
//                                ctx.content_length - ctx.req.current_body_length);

//                            ctx.req.received_body += ctx.input_buffer.substr(0, to_process);
//                            ctx.req.current_body_length += to_process;
//                            ctx.input_buffer.erase(0, to_process);

//                            if (ctx.req.current_body_length >= ctx.content_length) {
//                                ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
//                                ctx.req.is_upload_complete = true;
//                            }
//                        }
//                    } else {
//                        ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
//                    }
//                }
//            }
//            break;

//        case RequestBody::PARSING_BODY:
//            if (ctx.req.chunked_state.processing) {
//                parseChunkedBody(ctx);
//                if (ctx.error_code != 0) {
//                    return false;
//                }
//            } else {
//                size_t remaining = ctx.content_length - ctx.req.current_body_length;
//                size_t to_process = std::min(ctx.input_buffer.length(), remaining);

//                if (ctx.req.current_body_length + to_process > ctx.client_max_body_size) {
//                    return updateErrorStatus(ctx, 413, "Payload too large");
//                }

//                ctx.req.received_body += ctx.input_buffer.substr(0, to_process);
//                ctx.req.current_body_length += to_process;
//                ctx.input_buffer.erase(0, to_process);

//                if (ctx.req.current_body_length >= ctx.content_length) {
//                    ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
//                    ctx.req.is_upload_complete = true;
//                }
//            }
//            break;

//        case RequestBody::PARSING_COMPLETE:
//            break;
//    }

//    if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE) {
//        determineType(ctx, configs);
//        modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
//    }

//    return true;
//}