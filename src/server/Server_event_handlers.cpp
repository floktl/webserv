#include "Server.hpp"
bool Server::handleRead(Context& ctx, std::vector<ServerBlock> &configs) {
	char buffer[8192];
	ssize_t bytes;

	while ((bytes = read(ctx.client_fd, buffer, sizeof(buffer))) > 0) {
		ctx.input_buffer.append(buffer, bytes);

		if (!ctx.headers_complete) {
			parseHeaders(ctx);
		}

		if (ctx.headers_complete && ctx.content_length > 0) {
			size_t new_body_bytes = ctx.input_buffer.length();
			ctx.body_received += new_body_bytes;

			ctx.body += ctx.input_buffer;
			ctx.input_buffer.clear();
		}

		if (isRequestComplete(ctx)) {
			return processRequest(ctx, configs);
		}
	}

	if (bytes < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			Logger::file("Read error on fd " + std::to_string(ctx.client_fd) +
						": " + std::string(strerror(errno)));
			return false;
		}
	} else if (bytes == 0) {
		return false;
	}

	return true;
}

bool Server::parseHeaders(Context& ctx) {
	size_t header_end = ctx.input_buffer.find("\r\n\r\n");
	if (header_end == std::string::npos) {
		return false;
	}

	std::string headers = ctx.input_buffer.substr(0, header_end);
	std::istringstream stream(headers);
	std::string line;

	if (std::getline(stream, line)) {
		if (line.back() == '\r') line.pop_back();
		std::istringstream request_line(line);
		request_line >> ctx.method >> ctx.path >> ctx.version;
	}

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
			}
		}
	}

	ctx.input_buffer.erase(0, header_end + 4);
	ctx.headers_complete = true;

	return true;
}

bool Server::handleWrite(Context& ctx) {
	while (!ctx.output_buffer.empty()) {
		ssize_t sent = send(ctx.client_fd,
						ctx.output_buffer.c_str(),
						ctx.output_buffer.size(),
						MSG_NOSIGNAL);

		if (sent < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return true;
			}
			Logger::file("Write error on fd " + std::to_string(ctx.client_fd));
			return false;
		}

		ctx.output_buffer.erase(0, sent);
	}

	modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
	return true;
}

bool Server::queueResponse(Context& ctx, const std::string& response) {
	ctx.output_buffer += response;
	modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
	return true;
}

bool Server::processRequest(Context& ctx, std::vector<ServerBlock> &configs) {
	switch (ctx.type) {
		case INITIAL:
			determineType(ctx, configs);
			switch (ctx.type) {
				case STATIC:
					return staticHandler(ctx);
				case CGI:
					return true;
				case ERROR:
					return errorsHandler(ctx, 0);
				default:
					break;
			}
			break;
		case STATIC:
			return staticHandler(ctx);
		case CGI:
			return true;
		case ERROR:
			return errorsHandler(ctx, 0);
	}

	ctx.input_buffer.clear();
	ctx.body.clear();
	ctx.headers_complete = false;
	ctx.content_length = 0;
	ctx.body_received = 0;
	ctx.headers.clear();

	return true;
}

bool Server::handleNewConnection(int epoll_fd, int server_fd)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);

	int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
	if (client_fd < 0)
	{
		Logger::file("Accept failed: " + std::string(strerror(errno)));
		return true;
	}

	setNonBlocking(client_fd);
	Logger::file("epoll_fd before modEpoll: " + std::to_string(epoll_fd));
	modEpoll(epoll_fd, client_fd, EPOLLIN | EPOLLET);
	Logger::file("epoll_fd after modEpoll: " + std::to_string(epoll_fd) + " to: EPOLLIN | EPOLLET");
	Context ctx;
	ctx.server_fd = server_fd;
	ctx.client_fd = client_fd;
	ctx.epoll_fd = epoll_fd;
	ctx.type = RequestType::INITIAL;
	ctx.last_activity = std::chrono::steady_clock::now();
	ctx.doAutoIndex = "";

	globalFDS.request_state_map[client_fd] = ctx;
	logRequestBodyMapFDs();
	logContext(ctx, "New Connection");
	Logger::file("Leaving handle new connection sucessfully...\n");
	return true;
}

bool Server::sendWrapper(Context& ctx, std::string http_response)
{
	bool keepAlive = false;
	auto it = ctx.headers.find("Connection");

	if (it != ctx.headers.end())
		keepAlive = (it->second.find("keep-alive") != std::string::npos);


	ssize_t bytes_sent = send(ctx.client_fd, http_response.c_str(), http_response.size(), MSG_NOSIGNAL);
	if (bytes_sent < 0)
	{
		Logger::file("Error sending response to client_fd: " + std::to_string(ctx.client_fd) + " - " + std::string(strerror(errno)));
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
	}
	else if (bytes_sent == static_cast<ssize_t>(http_response.size()))
	{
		Logger::file("Successfully sent full response to client_fd: " + std::to_string(ctx.client_fd));
		if (!keepAlive)
			delFromEpoll(ctx.epoll_fd, ctx.client_fd);
	}
	else
	{
		Logger::file("Partial response sent to client_fd: " + std::to_string(ctx.client_fd) + " - Sent: " + std::to_string(bytes_sent) + " of " + std::to_string(http_response.size()) + " bytes");
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
	}
	return true;
}

bool Server::errorsHandler(Context& ctx, uint32_t ev)
{
	(void)ev;
	std::string errorResponse = getErrorHandler()->generateErrorResponse(ctx);
	return (sendWrapper(ctx, errorResponse));
}

bool Server::staticHandler(Context& ctx) {
	std::string response;

	if (!ctx.doAutoIndex.empty()) {
		std::stringstream ss;
		buildAutoIndexResponse(ctx, &ss);
		ctx.doAutoIndex = "";
		response = ss.str();
	} else {
		std::stringstream ss;
		buildStaticResponse(ctx);
		response = ss.str();
	}

	return queueResponse(ctx, response);
}


void Server::buildStaticResponse(Context &ctx) {
	bool keepAlive = false;
	auto it = ctx.headers.find("Connection");
	if (it != ctx.headers.end()) {
		keepAlive = (it->second.find("keep-alive") != std::string::npos);
	}

	std::stringstream content;
	content << "epoll_fd: " << ctx.epoll_fd << "\n"
			<< "client_fd: " << ctx.client_fd << "\n"
			<< "server_fd: " << ctx.server_fd << "\n"
			<< "type: " << static_cast<int>(ctx.type) << "\n"
			<< "method: " << ctx.method << "\n"
			<< "path: " << ctx.path << "\n"
			<< "version: " << ctx.version << "\n"
			<< "body: " << ctx.body << "\n"
			<< "location_path: " << ctx.location_path << "\n"
			<< "requested_path: " << ctx.requested_path << "\n"
			<< "error_code: " << ctx.error_code << "\n"
			<< "port: " << ctx.port << "\n"
			<< "name: " << ctx.name << "\n"
			<< "root: " << ctx.root << "\n"
			<< "index: " << ctx.index << "\n"
			<< "client_max_body_size: " << ctx.client_max_body_size << "\n"
			<< "timeout: " << ctx.timeout << "\n";

	content << "\nHeaders:\n";
	for (const auto& header : ctx.headers) {
		content << header.first << ": " << header.second << "\n";
	}

	content << "\nError Pages:\n";
	for (const auto& error : ctx.errorPages) {
		content << error.first << ": " << error.second << "\n";
	}

	std::string content_str = content.str();

	std::stringstream http_response;
	http_response << "HTTP/1.1 200 OK\r\n"
			<< "Content-Type: text/plain\r\n"
			<< "Content-Length: " << content_str.length() << "\r\n"
			<< "Connection: " << (keepAlive ? "keep-alive" : "close") << "\r\n"
			<< "\r\n"
			<< content_str;

	sendWrapper(ctx,  http_response.str());
}


// bool Server::handleCGIEvent(int epoll_fd, int fd, uint32_t ev) {
//     int client_fd = globalFDS.svFD_to_clFD_map[fd];
//     RequestBody &req = globalFDS.request_state_map[client_fd];
// 	// Only process method check once at start
//     if (!req.cgiMethodChecked) {
// 		req.cgiMethodChecked = true;
//         if (!clientHandler->processMethod(req, epoll_fd))
// 		{
//             return true;
// 		}
//     }
//     if (ev & EPOLLIN)
//         cgiHandler->handleCGIRead(epoll_fd, fd);
//     if (fd == req.cgi_out_fd && (ev & EPOLLHUP)) {
//         if (!req.cgi_output_buffer.empty())
//             cgiHandler->finalizeCgiResponse(req, epoll_fd, client_fd);
//         cgiHandler->cleanupCGI(req);
//     }
//     if (fd == req.cgi_in_fd) {
//         if (ev & EPOLLOUT)
//             cgiHandler->handleCGIWrite(epoll_fd, fd, ev);
//         if (ev & (EPOLLHUP | EPOLLERR)) {
//             epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
//             close(fd);
//             req.cgi_in_fd = -1;
//             globalFDS.svFD_to_clFD_map.erase(fd);
//         }
//     }
//     return true;
// }




//bool Server::staticHandler(Context& ctx, int epoll_fd, int fd, uint32_t ev)
//{
//    RequestBody &req = globalFDS.request_state_map[fd];

//    if (ev & EPOLLIN)
//    {
//        // clientHandler->handleClientRead(epoll_fd, fd);
//        // Logger::file("handleClient");
//    }

//    if (req.state != RequestBody::STATE_CGI_RUNNING &&
//        req.state != RequestBody::STATE_PREPARE_CGI &&
//        (ev & EPOLLOUT))
//    {
//        // clientHandler->handleClientWrite(epoll_fd, fd);
//    }

//    // Handle request body
//    if ((req.state == RequestBody::STATE_CGI_RUNNING ||
//        req.state == RequestBody::STATE_PREPARE_CGI) &&
//        (ev & EPOLLOUT) && !req.response_buffer.empty())
//    {
//        char send_buffer[8192];
//        size_t chunk_size = std::min(req.response_buffer.size(), sizeof(send_buffer));

//        std::copy(req.response_buffer.begin(),
//				req.response_buffer.begin() + chunk_size,
//				send_buffer);

//        ssize_t written = write(fd, send_buffer, chunk_size);

//        if (written > 0)
//        {
//            req.response_buffer.erase(
//                req.response_buffer.begin(),
//                req.response_buffer.begin() + written
//            );

//            if (req.response_buffer.empty())
//            {
//                delFromEpoll(epoll_fd, fd);
//            }
//        }
//        else if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
//        {
//            delFromEpoll(epoll_fd, fd);
//        }
//    }

//    return true;
//}


bool Server::buildAutoIndexResponse(Context& ctx, std::stringstream* response) {
	Logger::file("buildAutoIndexResponse: " + ctx.doAutoIndex);
	DIR* dir = opendir(ctx.doAutoIndex.c_str());
	if (!dir) {
		ctx.type = ERROR;
		ctx.error_code = 500;
		ctx.error_message = "Internal Server Error";
	}

	std::vector<DirEntry> entries;
	struct dirent* dir_entry;
	while ((dir_entry = readdir(dir)) != NULL) {
		std::string name = dir_entry->d_name;
		Logger::file("Found directory entry: " + name);
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
			Logger::file("stat failed for path: " + fullPath + " with error: " + std::string(strerror(errno)));
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
Logger::file("Before sorting, number of entries: " + std::to_string(entries.size()));

	std::sort(entries.begin(), entries.end(),
		[](const DirEntry& a, const DirEntry& b) -> bool {
			if (a.name == "..") return true;
			if (b.name == "..") return false;
			if (a.isDir != b.isDir) return a.isDir > b.isDir;
			return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
		});
Logger::file("After sorting, number of entries: " + std::to_string(entries.size()));

	*response << "HTTP/1.1 200 OK\r\n";
	*response << "Content-Type: text/html; charset=UTF-8\r\n\r\n";

	*response << "<!DOCTYPE html>\n"
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

		*response << "        <div class=\"entry\">\n"
				<< "            <div class=\"name\">"
				<< "<a href=\"" << (entry.name == ".." ? "../" : entry.name + (entry.isDir ? "/" : "")) << "\" "
				<< "class=\"" << className << "\">"
				<< displayName << (entry.isDir ? "/" : "") << "</a></div>\n"
				<< "            <div class=\"modified\">" << timeStr << "</div>\n"
				<< "            <div class=\"size\">" << sizeStr << "</div>\n"
				<< "        </div>\n";
	}

	*response << "    </div>\n"
			<< "</body>\n"
			<< "</html>\n";

	return true;
}