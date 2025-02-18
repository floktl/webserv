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

    // Only proceed if boundary is set
    if (ctx.boundary.empty()) {
        // Find and set boundary from Content-Type header
        auto content_type_it = ctx.headers.find("Content-Type");
        if (content_type_it != ctx.headers.end()) {
            size_t boundary_pos = content_type_it->second.find("boundary=");
            if (boundary_pos != std::string::npos) {
                ctx.boundary = "--" + content_type_it->second.substr(boundary_pos + 9);
                // Remove any trailing parameters
                size_t end_pos = ctx.boundary.find(';');
                if (end_pos != std::string::npos) {
                    ctx.boundary = ctx.boundary.substr(0, end_pos);
                }
                Logger::yellow("Set boundary to: " + ctx.boundary);
            }
        }

        if (ctx.boundary.empty()) {
            Logger::errorLog("Could not determine boundary from headers");
            return false;
        }
    }

    // Now extract content and place it in write buffer
    ctx.write_buffer.clear();
    bool extraction_result = extractFileContent(ctx.boundary, std::string(buffer, bytes), ctx.write_buffer, ctx);

    // Check if this chunk contains the end boundary marker
    if (std::string(buffer, bytes).find("--" + ctx.boundary + "--") != std::string::npos) {
        Logger::yellow("Final boundary found - Upload will complete after processing");
    }

    // Let your existing flow handle the write buffer
    if (ctx.upload_fd > 0 && !ctx.write_buffer.empty()) {
        ctx.write_pos = 0;
        ctx.write_len = ctx.write_buffer.size();
        modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
    } else if (ctx.upload_fd > 0) {
        // Keep waiting for more data
        modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
    }

    return extraction_result;
}

// Runs the main event loop, handling incoming connections and processing events via epoll
int Server::runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs) {
	const int max_events = 64;
	struct epoll_event events[max_events];
	const int timeout_ms = 1000;
	int incoming_fd = -1;
	int server_fd = -1;
	int client_fd = -1;
	//uint32_t ev;
	int eventNum;

	while (true) {
		eventNum = epoll_wait(epoll_fd, events, max_events, timeout_ms);
		Logger::red("loop...");
		log_global_fds(globalFDS);
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
		// for (int eventIter = 0; eventIter < eventNum; eventIter++) {
		// 	incoming_fd = events[eventIter].data.fd;
		// 	if (incoming_fd <= 0) {
		// 		Logger::errorLog("WARNING: Invalid fd " + std::to_string(incoming_fd) + " in epoll event");
		// 		continue;
		// 	}
		// 	ev = events[eventIter].events;

		// 	if (findServerBlock(configs, incoming_fd)) {
		// 		server_fd = incoming_fd;
		// 		acceptNewConnection(epoll_fd, server_fd, configs);
		// 	}
		// 	else {
		// 		client_fd = incoming_fd;
		// 		handleAcceptedConnection(epoll_fd, client_fd, ev, configs);
		// 	}
		// }
		for (int eventIter = 0; eventIter < eventNum; eventIter++) {
			incoming_fd = events[eventIter].data.fd;

			auto ctx_iter = globalFDS.context_map.find(incoming_fd);
			bool is_active_upload = false;

			Logger::blue("event...");
			if (ctx_iter != globalFDS.context_map.end()) {
				Context& ctx = ctx_iter->second;
				if (isMultipartUpload(ctx) &&
					ctx.req.parsing_phase == RequestBody::PARSING_BODY &&
					!ctx.req.is_upload_complete) {
					Logger::magenta("is loader...");
					is_active_upload = true;
				}
			}

			if (is_active_upload) {
				Logger::yellow("Continuing active upload for fd: " + std::to_string(incoming_fd));
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

bool Server::extractFileContent(const std::string& boundary, const std::string& buffer, std::vector<char>& output, Context& ctx) {
    Logger::yellow("extractFileContent - buffer size: " + std::to_string(buffer.size()));
    Logger::yellow("boundary: '" + boundary + "'");
    Logger::yellow("initial boundary found for ctx: " + std::to_string(ctx.found_first_boundary) );

    if (boundary.empty()) {
        return false;
    }

    // Look for boundary in this chunk
    size_t pos = buffer.find(boundary);
    Logger::yellow("boundary position: " + (pos == std::string::npos ? "not found" : std::to_string(pos)));

    // Check for final boundary marker
    bool final_boundary = (buffer.find("--" + boundary + "--") != std::string::npos);

    // Case 1: First boundary hasn't been found yet
    if (!ctx.found_first_boundary) {
        if (pos != std::string::npos) {
            // Found the first boundary!
            ctx.found_first_boundary = true;

            // Skip headers after the boundary
            size_t headers_end = buffer.find("\r\n\r\n", pos);
            if (headers_end != std::string::npos) {
                size_t content_start = headers_end + 4;

                // If this is also the final boundary, don't add anything
                if (final_boundary && buffer.find("--" + boundary + "--") < content_start) {
                    return true;
                }

                // Add content that follows the headers
                if (content_start < buffer.size()) {
                    output.insert(output.end(),
                                buffer.begin() + content_start,
                                buffer.end());
                }
            }
        }
        // If no boundary found yet, just return - we're still waiting for first boundary
        return true;
    }

    // Case 2: We've already found the first boundary
    else {
        // Check if this chunk contains the end boundary
        if (final_boundary) {
            size_t end_pos = buffer.find("--" + boundary + "--");
            // Only include content up to the end boundary
            if (end_pos > 0) {
                output.insert(output.end(),
                            buffer.begin(),
                            buffer.begin() + end_pos - 2); // -2 to remove CRLF before boundary
            }
            return true;
        }

        // Check if this chunk contains an intermediate boundary
        if (pos != std::string::npos) {
            // Add content up to the boundary
            if (pos > 0) {
                output.insert(output.end(),
                            buffer.begin(),
                            buffer.begin() + pos - 2); // -2 to remove CRLF before boundary
            }

            // Skip headers after the boundary
            size_t headers_end = buffer.find("\r\n\r\n", pos);
            if (headers_end != std::string::npos) {
                size_t content_start = headers_end + 4;
                // Add any content after the headers
                if (content_start < buffer.size()) {
                    output.insert(output.end(),
                                buffer.begin() + content_start,
                                buffer.end());
                }
            }
        }
        // No boundary in this chunk - it's all content
        else {
            output.insert(output.end(), buffer.begin(), buffer.end());
        }
    }

    return true;
}

// Handles reading request data from the client and processing it accordingly
bool Server::handleRead(Context& ctx, std::vector<ServerBlock>& configs)
{
    Logger::green("handleRead " + std::to_string(ctx.client_fd));
    Logger::yellow("Read buffer size before: " + std::to_string(ctx.read_buffer.size()));
    Logger::yellow("Write buffer size before: " + std::to_string(ctx.write_buffer.size()));

    // For multipart uploads, we need to preserve the buffer between reads
    // to handle boundaries that might span across multiple reads
    if (!isMultipartUpload(ctx) || ctx.req.parsing_phase != RequestBody::PARSING_BODY) {
        ctx.read_buffer.clear();
    }

    // Always clear write buffer before new reads
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

    Logger::yellow("Bytes read: " + std::to_string(bytes));
    Logger::yellow("Read buffer size after: " + std::to_string(ctx.read_buffer.size()));
    Logger::yellow("Upload status - current: " + std::to_string(ctx.req.current_body_length) +
                  ", expected: " + std::to_string(ctx.req.expected_body_length));

    ctx.last_activity = std::chrono::steady_clock::now();

    // Reset context if previous request was complete and we're not in an upload
    if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE && ctx.upload_fd < 0)
        resetContext(ctx);

    // For multipart uploads in progress, append to the buffer to preserve boundary data
	if (isMultipartUpload(ctx) && ctx.req.parsing_phase == RequestBody::PARSING_BODY) {
		ctx.read_buffer = std::string(buffer, bytes);  // Process only this chunk, not accumulated history
	} else {
		ctx.read_buffer = std::string(buffer, bytes);
	}

    Logger::cyan("READ_BUFFER:\n" + Logger::logReadBuffer(ctx.read_buffer));

    if (!ctx.headers_complete) {
        // Process headers first
        if (!handleParsingPhase(ctx, configs)) {
            Logger::errorLog("Header parsing failed");
            return false;
        }

        if (ctx.headers_complete) {
            // Headers are now complete
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
                ctx.req.parsing_phase = RequestBody::PARSING_BODY;

                if (ctx.boundary.empty()) {
                    // Extract boundary from Content-Type header
                    auto content_type_it = ctx.headers.find("Content-Type");
                    if (content_type_it != ctx.headers.end()) {
                        size_t boundary_pos = content_type_it->second.find("boundary=");
                        if (boundary_pos != std::string::npos) {
                            ctx.boundary = "--" + content_type_it->second.substr(boundary_pos + 9);
                            // Remove any trailing parameters
                            size_t end_pos = ctx.boundary.find(';');
                            if (end_pos != std::string::npos) {
                                ctx.boundary = ctx.boundary.substr(0, end_pos);
                            }
                            Logger::yellow("Set boundary to: " + ctx.boundary);
                        }
                    }
                }

                if (ctx.uploaded_file_path.empty()) {
                    parseMultipartHeaders(ctx);
                }

                if (!ctx.uploaded_file_path.empty() && ctx.upload_fd < 0) {
                    if (!prepareMultipartUpload(ctx, configs)) {
                        return false;
                    }
                }

                // For initial body chunk, process it immediately
                readingTheBody(ctx, buffer, bytes);
            }
        }
    } else if (isMultipartUpload(ctx) && ctx.req.parsing_phase == RequestBody::PARSING_BODY) {
        Logger::yellow("Do seq body stuff");

        // Check for boundaries spanning across multiple chunks
        if (ctx.read_buffer.size() > ctx.boundary.size() * 2) {
            // Look at each possible position where a boundary might start
            for (size_t i = 0; i <= ctx.read_buffer.size() - ctx.boundary.size(); i++) {
                if (ctx.read_buffer.compare(i, ctx.boundary.size(), ctx.boundary) == 0) {
                    Logger::yellow("Found boundary spanning chunks!");
                    break;
                }
            }
        }

        // Look for final boundary
        if (ctx.read_buffer.find("--" + ctx.boundary + "--") != std::string::npos) {
            Logger::yellow("Final boundary found in sequential parse");
        }

        // Extract file content from the current buffer
	extractFileContent(ctx.boundary, ctx.read_buffer, ctx.write_buffer, ctx);
    }

    Logger::magenta("WRITE_BUFFER:\n" + Logger::logWriteBuffer(ctx.write_buffer));

    // If upload is complete, log progress
    if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE && ctx.had_seq_parse) {
        std::cout << std::endl;
    }

    // For uploads, if we have content to write, switch to write mode
    if (isMultipartUpload(ctx) && ctx.req.parsing_phase == RequestBody::PARSING_BODY
        && !ctx.write_buffer.empty() && ctx.upload_fd >= 0) {
        modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
        return true;
    }

    // If we have a final boundary, mark upload as complete
    if (isMultipartUpload(ctx) && ctx.read_buffer.find("--" + ctx.boundary + "--") != std::string::npos) {
        if (ctx.upload_fd >= 0) {
            // First write any remaining data
            if (!ctx.write_buffer.empty()) {
                // Write the buffer before closing
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
    Logger::green("doMultipartWriting");
    Logger::yellow("current_body_length: " + std::to_string(ctx.req.current_body_length));
    Logger::yellow("expected_body_length: " + std::to_string(ctx.req.expected_body_length));
    Logger::yellow("boundary size: " + std::to_string(ctx.boundary.size()));
    Logger::yellow("buffer size: " + std::to_string(ctx.write_buffer.size()));

    // Don't proceed if buffer is empty but upload is still ongoing
    if (ctx.write_buffer.empty()) {
        Logger::yellow("Write buffer empty but upload still in progress");
        modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
        return true;
    }

    // Open file in append mode if not already open
    if (ctx.upload_fd < 0) {
        ctx.upload_fd = open(ctx.uploaded_file_path.c_str(), O_WRONLY | O_APPEND | O_CREAT, S_IRUSR | S_IWUSR);
        if (ctx.upload_fd < 0) {
            Logger::errorLog("Failed to open file: " + std::string(strerror(errno)));
            return updateErrorStatus(ctx, 500, "Failed to open upload file");
        }
    }

    // Write data from buffer to file
    ssize_t written = write(ctx.upload_fd,
                         ctx.write_buffer.data(),
                         ctx.write_buffer.size());

    if (written < 0) {
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Try again later
            modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
            return true;
        }
        Logger::errorLog("Write error: " + std::string(strerror(errno)));
        close(ctx.upload_fd);
        ctx.upload_fd = -1;
        return updateErrorStatus(ctx, 500, "Failed to write to upload file");
    }

    // Update progress and clear the write buffer after successful write
    ctx.req.current_body_length += written;
    Logger::progressBar(ctx.req.current_body_length, ctx.req.expected_body_length, "Upload: 8");
    Logger::yellow("After write - current_body_length: " + std::to_string(ctx.req.current_body_length));

    // Clear buffer after successful write
    ctx.write_buffer.clear();

    // Check for completion - either by final boundary or by size
    bool final_boundary_found = false;
    // Convert any combined buffers to a string for searching
    std::string combined_buffer;
    if (!ctx.read_buffer.empty()) {
        combined_buffer = ctx.read_buffer;
    }

    if (combined_buffer.find("--" + ctx.boundary + "--") != std::string::npos) {
        final_boundary_found = true;
        Logger::yellow("Final boundary found!");
    }

    // Also check if we've reached or exceeded the expected body length
    bool size_complete = ctx.req.current_body_length >=
                       (ctx.req.expected_body_length - (ctx.boundary.size() * 4)); // Account for boundary overhead

    if (final_boundary_found || size_complete ||
        (ctx.req.current_body_length > 0 && ctx.read_buffer.empty() && ctx.write_buffer.empty())) {

        std::string completion_reason;
        if (final_boundary_found) completion_reason = "final boundary found";
        else if (size_complete) completion_reason = "size threshold reached";
        else completion_reason = "buffer empty and some data written";

        Logger::red("Upload completed! Reason: " + completion_reason);
        Logger::yellow("Final current_body_length: " + std::to_string(ctx.req.current_body_length));

        // Show 100% completion
        Logger::progressBar(ctx.req.expected_body_length, ctx.req.expected_body_length, "Upload: 8");
        std::cout << std::endl;

        // Close file and mark upload as complete
        close(ctx.upload_fd);
        ctx.upload_fd = -1;
        ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
        ctx.req.is_upload_complete = true;

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
	if (ctx.upload_fd > 0) {  // Remove the buffer empty check
		if (!ctx.write_buffer.empty()) {
			Logger::yellow("doMultipartWriting");
			result = doMultipartWriting(ctx);
		} else {
			// Buffer is empty but upload still in progress
			Logger::yellow("Waiting for more data, upload_fd is open");
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
			return true;  // Continue the upload process
		}
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