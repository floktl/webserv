#include "Server.hpp"

size_t Server::findBodyStart(const std::string& buffer, Context& ctx) {
	Logger::green("findBodyStart");
    const std::string boundaryMarker = "\r\n\r\n";
    size_t pos = buffer.find(boundaryMarker);

    if (pos != std::string::npos) {
        if (isMultipartUpload(ctx)) {
            std::string contentType = ctx.headers["Content-Type"];
			Logger::yellow(contentType);
            size_t boundaryPos = contentType.find("boundary=");
			Logger::yellow(std::to_string(boundaryPos));
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
		Logger::yellow(std::to_string(pos + boundaryMarker.length()));
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

            // Jetzt Boundary und Header entfernen
            const std::string boundaryMarker = "\r\n\r\n";
            size_t boundaryPos = cleanBody.find(boundaryMarker);
            if (boundaryPos != std::string::npos) {
                cleanBody = cleanBody.substr(boundaryPos + boundaryMarker.length());
            }

            // Entfernen des Content-Disposition-Headers (falls vorhanden)
            size_t contentDispositionPos = cleanBody.find("Content-Disposition: form-data;");
            if (contentDispositionPos != std::string::npos) {
                size_t endPos = cleanBody.find("\r\n", contentDispositionPos);
                if (endPos != std::string::npos) {
                    cleanBody.erase(contentDispositionPos, endPos - contentDispositionPos + 2);
                }
            }

            // Entfernen des Content-Type-Headers (falls vorhanden)
            size_t contentTypePos = cleanBody.find("Content-Type:");
            if (contentTypePos != std::string::npos) {
                size_t endPos = cleanBody.find("\r\n", contentTypePos);
                if (endPos != std::string::npos) {
                    cleanBody.erase(contentTypePos, endPos - contentTypePos + 2);
                }
            }

            // Entfernen des Boundary am Ende
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

bool Server::prepareMultipartUpload(Context& ctx, std::vector<ServerBlock> configs) {
	if (!determineType(ctx, configs)) {
		Logger::errorLog("Failed to determine request type");
		return false;
	}
	std::string req_root = retreiveReqRoot(ctx);

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
    std::string bodyContent = extractBodyContent(buffer, bytes, ctx);

    if (ctx.upload_fd > 0) {
		ctx.write_buffer.insert(ctx.write_buffer.end(), bodyContent.begin(), bodyContent.end());
		ctx.tmp_buffer.clear();

		ctx.header_offset = 0;
        ctx.write_pos = 0;
        ctx.write_len = bodyContent.size();

        Logger::progressBar(ctx.req.current_body_length,ctx.req.expected_body_length, "Upload: 8");
        modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
    }
    return true;
}

// Runs the main event loop, handling incoming connections and processing events via epoll
int Server::runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs) {
	const int max_events = 64;
	struct epoll_event events[max_events];
	const int timeout_ms = 1000;
	int incoming_fd = -1;
	int server_fd = -1;
	int client_fd = -1;
	uint32_t ev;
	int eventNum;

	while (true) {
		eventNum = epoll_wait(epoll_fd, events, max_events, timeout_ms);

		if (eventNum < 0) {
			Logger::errorLog("Epoll error: " + std::string(strerror(errno)));
			if (errno == EINTR)
				continue;
			break;
		}
		else if (eventNum == 0) {
			checkAndCleanupTimeouts();
			continue;
		}

		for (int eventIter = 0; eventIter < eventNum; eventIter++) {
			incoming_fd = events[eventIter].data.fd;
			if (incoming_fd <= 0) {
				Logger::errorLog("WARNING: Invalid fd " + std::to_string(incoming_fd) + " in epoll event");
				continue;
			}
			ev = events[eventIter].events;

			if (findServerBlock(configs, incoming_fd)) {
				server_fd = incoming_fd;
				acceptNewConnection(epoll_fd, server_fd, configs);
			}
			else {
				client_fd = incoming_fd;
				handleAcceptedConnection(epoll_fd, client_fd, ev, configs);
			}
		}
	}
	close(epoll_fd);
	epoll_fd = -1;
	return EXIT_SUCCESS;
}

// Runs the main event loop, handling incoming connections and processing events via epoll
bool Server::acceptNewConnection(int epoll_fd, int server_fd, std::vector<ServerBlock> &configs)
{

	Logger::green("acceptNewConnection");
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

// Handles an existing client connection by processing read/write events and error handling
bool Server::handleAcceptedConnection(int epoll_fd, int client_fd, uint32_t ev, std::vector<ServerBlock> &configs)
{
	Logger::green("handleAcceptedConnection");
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

bool Server::extractFileContent(const std::string& boundary, const std::string& buffer, std::vector<char>& output) {
	// Find the boundary
	size_t pos = buffer.find(boundary);
	if (pos == std::string::npos) {
		// If no boundary found, this is pure file content
		output.insert(output.end(), buffer.begin(), buffer.end());
		return true;
	}

	// Find the end of headers (double CRLF)
	size_t headers_end = buffer.find("\r\n\r\n", pos);
	if (headers_end == std::string::npos) {
		return false;
	}

	// Skip the headers and the double CRLF
	size_t content_start = headers_end + 4;

	// Find the next boundary if it exists
	size_t next_boundary = buffer.find(boundary, content_start);

	// Copy only the file content
	if (next_boundary == std::string::npos) {
		output.insert(output.end(),
					buffer.begin() + content_start,
					buffer.end());
	} else {
		// Don't include the final CRLF before the boundary
		output.insert(output.end(),
					buffer.begin() + content_start,
					buffer.begin() + next_boundary - 2);
	}

	return true;
}

// Handles reading request data from the client and processing it accordingly
bool Server::handleRead(Context& ctx, std::vector<ServerBlock>& configs)
{
	Logger::green("handleRead " + std::to_string(ctx.client_fd));

	ctx.read_buffer.clear();
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
	//Logger::red(std::string(buffer));

	ctx.last_activity = std::chrono::steady_clock::now();
	if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE && ctx.upload_fd < 0)
		resetContext(ctx);

	ctx.read_buffer.append(buffer, bytes);
	Logger::cyan(ctx.read_buffer);
	if (!ctx.headers_complete) {
		if (!handleParsingPhase(ctx, configs)) {
			Logger::errorLog("Header parsing failed");
			return false;
		}

		if (ctx.headers_complete) {
			handleSessionCookies(ctx);

			if (ctx.method == "GET" || ctx.method == "DELETE") {
				if (!determineType(ctx, configs)) {
					Logger::errorLog("Failed to determine request type");
					return false;
				}
				modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
				return true;
			}

			if (isMultipartUpload(ctx)) {
				Logger::yellow("Do inital body stuff");
				if (ctx.uploaded_file_path.empty()) {
					parseMultipartHeaders(ctx);
				}
				if (!ctx.uploaded_file_path.empty() && ctx.upload_fd < 0) {
					if (!prepareMultipartUpload(ctx, configs)) {
						return false;
					}
				}
				readingTheBody(ctx, buffer, bytes);
			}
		}
	} else if (isMultipartUpload(ctx)) {
		Logger::yellow("Do seq body stuff");
		extractFileContent(ctx.boundary, ctx.read_buffer, ctx.write_buffer);
		ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
	}
	Logger::magenta(std::string(ctx.write_buffer.begin(), ctx.write_buffer.end()));
	if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE && ctx.had_seq_parse) {
		Logger::progressBar(ctx.req.expected_body_length, ctx.req.expected_body_length, "Upload: 8");
		std::cout << std::endl;
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
    Logger::green("doMultipartWriting");

    // Da wir O_APPEND nutzen, können wir einfach direkt den ganzen Buffer schreiben
    int tmp_fd = open(ctx.uploaded_file_path.c_str(), O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
    ssize_t written = write(tmp_fd,
                           ctx.write_buffer.data(),
                           ctx.write_buffer.size());

    if (written < 0) {
        Logger::errorLog("Write error: " + std::string(strerror(errno)));
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            return true;
        }
        close(ctx.upload_fd);
        ctx.upload_fd = -1;
        return updateErrorStatus(ctx, 500, "Failed to write to upload file");
    }

    ctx.req.current_body_length += written;
    Logger::progressBar(ctx.req.current_body_length, ctx.req.expected_body_length, "Upload: 8");

    if (ctx.req.current_body_length >= (ctx.req.expected_body_length - (2 * ctx.boundary.size()))) {
        Logger::progressBar(ctx.req.expected_body_length, ctx.req.expected_body_length, "Upload: 8");
        std::cout << std::endl;

        close(ctx.upload_fd);
        ctx.upload_fd = -1;
        ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
        ctx.req.is_upload_complete = true;
        ctx.uploaded_file_path.clear();

        return redirectAction(ctx);
    }

    // Ready for next read
    modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
    return true;
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
	Logger::blue("handleWrite");
//std::string tmp(ctx.write_buffer.begin(), ctx.write_buffer.end());
//Logger::blue(tmp);

	bool result = false;
	initializeWritingActions(ctx);
	Logger::yellow("initializeWritingActions");
	Logger::yellow(std::to_string(ctx.upload_fd));
	Logger::yellow(std::to_string(ctx.write_buffer.size()));
	if (ctx.upload_fd > 0 && !ctx.write_buffer.empty()) {
		Logger::yellow("doMultipartWriting");
		result = doMultipartWriting(ctx);
	}
	else
	{
		Logger::errorLog("in normal");
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
				result = true;
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