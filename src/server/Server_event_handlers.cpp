#include "Server.hpp"

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

    // Set non-blocking and add to epoll
    setNonBlocking(client_fd);
	Logger::file("epoll_fd before modEpoll: " + std::to_string(epoll_fd));
    modEpoll(epoll_fd, client_fd, EPOLLIN | EPOLLET);
	Logger::file("epoll_fd after modEpoll: " + std::to_string(epoll_fd) + " to: EPOLLIN | EPOLLET");
    // Create new context
    Context ctx;
    ctx.server_fd = server_fd;  // Store the server fd that accepted this connection
    ctx.client_fd = client_fd;
	ctx.epoll_fd = epoll_fd;
    ctx.type = RequestType::INITIAL;
    ctx.last_activity = std::chrono::steady_clock::now();
	ctx.doAutoIndex = "";

    // Store in global state
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

bool Server::staticHandler(Context& ctx, uint32_t ev) {
    Logger::file("Entering staticHandler for client_fd: " + std::to_string(ctx.client_fd) + ", epoll_fd: " + std::to_string(ctx.epoll_fd));

    auto now = std::chrono::steady_clock::now();
    logContext(ctx);

    Logger::file("Fetching client_fd: " + std::to_string(ctx.client_fd) + " from request_state_map");

    Logger::file("Checking timeout for client_fd: " + std::to_string(ctx.client_fd));

    if (now - ctx.last_activity > RequestBody::TIMEOUT_DURATION) {
        //Logger::file("[handleClientWrite] Timeout reached. Removing fd: " + std::to_string(ctx.client_fd));
        delFromEpoll(ctx.epoll_fd, ctx.client_fd);
        return false;
    }

    Logger::file("Checking socket errors for client_fd: " + std::to_string(ctx.client_fd));
    int error = 0;
    socklen_t len = sizeof(error);
    if (getsockopt(ctx.client_fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
        Logger::file("[handleClientWrite] getsockopt error on client_fd: " + std::to_string(ctx.client_fd) + ", error: " + std::to_string(error) + " (" + std::string(strerror(errno)) + ")");
        delFromEpoll(ctx.epoll_fd, ctx.client_fd);
        return false;
    }

    if (!ctx.doAutoIndex.empty()) {
        std::stringstream response;
        buildAutoIndexResponse(ctx, &response);
        ctx.doAutoIndex = "";
        return sendWrapper(ctx, response.str());
    }

    buildStaticResponse(ctx);

    Logger::file("Exiting staticHandler for client_fd: " + std::to_string(ctx.client_fd));

    (void)ctx;
    (void)ctx.epoll_fd;

	(void)ev;
    return true;
}


void Server::buildStaticResponse(Context &ctx) {
    // Check if client requested keep-alive
    bool keepAlive = false;
    auto it = ctx.headers.find("Connection");
    if (it != ctx.headers.end()) {
        keepAlive = (it->second.find("keep-alive") != std::string::npos);
    }

    // Build content string with all Context struct fields
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

    // Add headers
    content << "\nHeaders:\n";
    for (const auto& header : ctx.headers) {
        content << header.first << ": " << header.second << "\n";
    }

    // Add error pages
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
        if (name == "." || (name == ".." && ctx.location_path == "/"))
            continue;

        if (name == ".." && ctx.location_path != "/") {
            entries.push_back({"..", true, 0, 0});
            continue;
        }

        std::string fullPath = ctx.requested_path;
        if (fullPath.back() != '/')
            fullPath += '/';
        fullPath += name;

        struct stat statbuf;
        if (stat(fullPath.c_str(), &statbuf) != 0)
            continue;

        entries.push_back({
            name,
            S_ISDIR(statbuf.st_mode),
            statbuf.st_mtime,
            statbuf.st_size
        });
    }
    closedir(dir);

    std::sort(entries.begin(), entries.end(),
        [](const DirEntry& a, const DirEntry& b) -> bool {
            if (a.name == "..") return true;
            if (b.name == "..") return false;
            if (a.isDir != b.isDir) return a.isDir > b.isDir;
            return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
        });

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