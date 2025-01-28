#include "Server.hpp"

bool Server::handleRead(Context& ctx, std::vector<ServerBlock> &configs)
{
	//Logger::file("||||||||  READ READ READ READ READ READ READ READ READ  ||||||||");
	char buffer[8192];
	ssize_t bytes;
	if (ctx.client_fd <= 0) {
        //Logger::file("Invalid client_fd handeRead: " + std::to_string(ctx.client_fd));
        return false;
    }

	while ((bytes = read(ctx.client_fd, buffer, sizeof(buffer))) > 0)
	{
		ctx.input_buffer.append(buffer, bytes);

		if (!ctx.headers_complete)
		{
			logContext(ctx, "before parsing headers");
			parseHeaders(ctx);
			logContext(ctx, "after parsing headers");
		}

		if (ctx.headers_complete && ctx.content_length > 0)
		{
			size_t new_body_bytes = ctx.input_buffer.length();
			ctx.body_received += new_body_bytes;

			ctx.body += ctx.input_buffer;
			ctx.input_buffer.clear();
		}

		if (isRequestComplete(ctx))
		{
			logContext(ctx, "before processrequest");
			determineType(ctx, configs);
			//bool requestgg = processRequest(ctx, configs);
			//if (requestgg)
			//	logContext(ctx, "processrequest success");
			//else
			logContext(ctx, "after process Request");
			//Logger::file("after process Request KEEP ALIVE!!!!: " + std::to_string(ctx.keepAlive));
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);

            return true;
		}
		return false;
	}

	if (bytes < 0)
	{
		if (errno != EAGAIN && errno != EWOULDBLOCK)
		{
			//Logger::file("Read error on fd " + std::to_string(ctx.client_fd) + ": " + std::string(strerror(errno)));
			logContext(ctx, "read error");
			return false;
		}
		return false;
	}
	else if (bytes == 0)
	{
		//Logger::file("bytes == 0 in handleread end");
		return true;
	}

	return true;
}

bool Server::parseHeaders(Context& ctx)
{
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
	// Determine keepAlive status
	ctx.keepAlive = (ctx.version == "HTTP/1.1") ?
    (ctx.headers["Connection"] != "close") :
    (ctx.headers["Connection"] == "keep-alive");
	//Logger::file("checked headers keep alive: " + std::to_string(ctx.keepAlive));
	ctx.input_buffer.erase(0, header_end + 4);
	ctx.headers_complete = true;

	return true;
}

bool Server::handleWrite(Context& ctx)
{
    //Logger::file("||||||||  WRITE WRITE WRITE WRITE WRITE WRITE WRITE WRITE WRITE  ||||||||");

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
    if (result) {
        ctx.input_buffer.clear();
        ctx.body.clear();
        ctx.headers_complete = false;
        ctx.content_length = 0;
        ctx.body_received = 0;
        ctx.headers.clear();

        // Reset back to EPOLLIN only if keepAlive is true
		//Logger::file("handleWrite in result clean: "+ std::to_string(ctx.keepAlive));
        if (ctx.keepAlive) {
		 	//Logger::file("modEpoll because keep alive ");
            modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
        } else {
			//Logger::file("delFromEpoll because not keep alive ");
            delFromEpoll(ctx.epoll_fd, ctx.client_fd);
		}
    }
	//Logger::file("handleWrite after result");

    return result;
}

bool Server::queueResponse(Context& ctx, const std::string& response)
{
	ctx.output_buffer += response;
	modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
	return true;
}

bool Server::handleNewConnection(int epoll_fd, int server_fd)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	if (server_fd <= 0) {
        //Logger::file("Invalid server: " + std::to_string(server_fd));
        if (server_fd == 0)
        return false;
    }
	int client_fd = accept(server_fd, (struct sockaddr*)&client_addr, &client_len);
    if (client_fd <= 0) {
        //Logger::file("Failed to accept connection: " + std::string(strerror(errno)));
        return true;
    }

    //Logger::file("Added to clFD_to_svFD_map[clFD:" + std::to_string(client_fd) + "] = svFD(" + std::to_string(server_fd) + ")");
	globalFDS.clFD_to_svFD_map[client_fd] = server_fd;

	if (setNonBlocking(client_fd) < 0) {
            //Logger::file("Failed to set non-blocking mode: " + std::string(strerror(errno)));
            close(client_fd);
            return true;
    }
	//Logger::file("epoll_fd before modEpoll: " + std::to_string(epoll_fd));
	modEpoll(epoll_fd, client_fd, EPOLLIN | EPOLLET);
	//Logger::file("epoll_fd after modEpoll: " + std::to_string(epoll_fd) + " to: EPOLLIN | EPOLLET");
	if (client_fd < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            //Logger::file("Accept error: " + std::string(strerror(errno)));
        }
    }
	Context ctx;
	ctx.server_fd = server_fd;
	ctx.client_fd = client_fd;
	ctx.epoll_fd = epoll_fd;
	ctx.type = RequestType::INITIAL;
	ctx.last_activity = std::chrono::steady_clock::now();
	ctx.doAutoIndex = "";
	ctx.keepAlive = true;
	globalFDS.request_state_map[client_fd] = ctx;
	logRequestBodyMapFDs();
	logContext(ctx, "New Connection");
	//Logger::file("Leaving handle new connection sucessfully...\n");
	return true;
}

bool Server::sendWrapper(Context& ctx, std::string http_response)
{
    // Attempt to send the response
    ssize_t bytes_sent = send(ctx.client_fd, http_response.c_str(), http_response.size(), MSG_NOSIGNAL);
    if (bytes_sent < 0) {
        //Logger::file("Error sending response to client_fd: " + std::to_string(ctx.client_fd) + " - " + std::string(strerror(errno)));
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

    // Sende die Response
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

// void Server::buildStaticResponse(Context &ctx) {
// 	bool keepAlive = false;
// 	auto it = ctx.headers.find("Connection");
// 	if (it != ctx.headers.end()) {
// 		keepAlive = (it->second.find("keep-alive") != std::string::npos);
// 	}

// 	std::stringstream content;
// 	content << "epoll_fd: " << ctx.epoll_fd << "\n"
// 			<< "client_fd: " << ctx.client_fd << "\n"
// 			<< "server_fd: " << ctx.server_fd << "\n"
// 			<< "type: " << static_cast<int>(ctx.type) << "\n"
// 			<< "method: " << ctx.method << "\n"
// 			<< "path: " << ctx.path << "\n"
// 			<< "version: " << ctx.version << "\n"
// 			<< "body: " << ctx.body << "\n"
// 			<< "location_path: " << ctx.location_path << "\n"
// 			<< "requested_path: " << ctx.requested_path << "\n"
// 			<< "error_code: " << ctx.error_code << "\n"
// 			<< "port: " << ctx.port << "\n"
// 			<< "name: " << ctx.name << "\n"
// 			<< "root: " << ctx.root << "\n"
// 			<< "index: " << ctx.index << "\n"
// 			<< "client_max_body_size: " << ctx.client_max_body_size << "\n"
// 			<< "timeout: " << ctx.timeout << "\n";

// 	content << "\nHeaders:\n";
// 	for (const auto& header : ctx.headers) {
// 		content << header.first << ": " << header.second << "\n";
// 	}

// 	content << "\nError Pages:\n";
// 	for (const auto& error : ctx.errorPages) {
// 		content << error.first << ": " << error.second << "\n";
// 	}

// 	std::string content_str = content.str();

// 	std::stringstream http_response;
// 	http_response << "HTTP/1.1 200 OK\r\n"
// 			<< "Content-Type: text/plain\r\n"
// 			<< "Content-Length: " << content_str.length() << "\r\n"
// 			<< "Connection: " << (keepAlive ? "keep-alive" : "close") << "\r\n"
// 			<< "\r\n"
// 			<< content_str;

// 	sendWrapper(ctx,  http_response.str());
// }


// bool Server::handleCGIEvent(int epoll_fd, int fd, uint32_t ev) {
//     int client_fd = globalFDS.clFD_to_svFD_map[fd];
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
//             globalFDS.clFD_to_svFD_map.erase(fd);
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
//        // //Logger::file("handleClient");
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
			//Logger::file("stat failed for path: " + fullPath + " with error: " + std::string(strerror(errno)));
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