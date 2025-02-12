#include "Server.hpp"

std::string determineContentType(const std::string& path) {
	std::string contentType = "text/plain";
	size_t dot_pos = path.find_last_of('.');
	if (dot_pos != std::string::npos) {
		std::string ext = path.substr(dot_pos + 1);
		if (ext == "html" || ext == "htm") contentType = "text/html";
		else if (ext == "css") contentType = "text/css";
		else if (ext == "js") contentType = "application/javascript";
		else if (ext == "jpg" || ext == "jpeg") contentType = "image/jpeg";
		else if (ext == "png") contentType = "image/png";
		else if (ext == "gif") contentType = "image/gif";
		else if (ext == "pdf") contentType = "application/pdf";
		else if (ext == "json") contentType = "application/json";
	}
	return contentType;
}

// Builds an HTTP response for serving static files, setting content type and response headers
void Server::buildStaticResponse(Context &ctx) {
	std::string fullPath = ctx.approved_req_path;
	std::ifstream file(fullPath, std::ios::binary);
	if (!file) {
		updateErrorStatus(ctx, 404, "Not found asasas");
		return;
	}

	std::string contentType = determineContentType(fullPath);
	std::stringstream content;
	content << file.rdbuf();
	std::string content_str = content.str();

	std::stringstream http_response;
	http_response << "HTTP/1.1 200 OK\r\n"
				<< "Content-Type: " << contentType << "\r\n"
				<< "Content-Length: " << content_str.length() << "\r\n"
				<< "Connection: " << (ctx.keepAlive ? "keep-alive" : "close") << "\r\n";

	// Add Set-Cookie headers for any cookies that need to be set
	for (const auto& cookiePair : ctx.setCookies) {
		Cookie cookie;
		cookie.name = cookiePair.first;
		cookie.value = cookiePair.second;
		cookie.path = "/";  // Default path
		// You can customize other cookie attributes here if needed
		http_response << generateSetCookieHeader(cookie) << "\r\n";
	}

	http_response << "\r\n" << content_str;
	sendHandler(ctx, http_response.str());
	resetContext(ctx);
}

// Builds an auto-index (directory listing) response when no index file is found
bool Server::buildAutoIndexResponse(Context& ctx, std::stringstream* response)
{
	std::vector<DirEntry> entries = getDirectoryEntries(ctx);
	if (entries.empty())
		return updateErrorStatus(ctx, 500, "Internal Server Error");

	std::stringstream content;
	generateAutoIndexHeader(ctx, content);
	generateAutoIndexBody(entries, content);

	content << "    </div>\n" << "</body>\n" << "</html>\n";

	std::string content_str = content.str();

	*response << "HTTP/1.1 200 OK\r\n"
				<< "Content-Type: text/html; charset=UTF-8\r\n"
				<< "Connection: " << (ctx.keepAlive ? "keep-alive" : "close") << "\r\n"
				<< "Content-Length: " << content_str.length() << "\r\n\r\n"
				<< content_str;
	return true;
}

// Queues an HTTP response in the output buffer and updates epoll events for writing
bool Server::queueResponse(Context& ctx, const std::string& response)
{
	ctx.output_buffer += response;
	modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
	return true;
}

// Retrieves directory entries, filtering and sorting them for directory listing
std::vector<DirEntry> Server::getDirectoryEntries(Context& ctx)
{
	DIR* dir = opendir(ctx.doAutoIndex.c_str());
	if (!dir)
		return {};
	std::vector<DirEntry> entries;
	struct dirent* dir_entry;
	while ((dir_entry = readdir(dir)) != NULL)
	{
		std::string name = dir_entry->d_name;
		if (name == "." || (name == ".." && ctx.location_path == "/"))
			continue;
		if (name == ".." && ctx.location_path != "/")
		{
			entries.push_back({"..", true, 0, 0});
			continue;
		}
		std::string fullPath = ctx.doAutoIndex;
		if (fullPath.back() != '/')
			fullPath += '/';
		fullPath += name;
		struct stat statbuf;
		if (stat(fullPath.c_str(), &statbuf) != 0)
		{
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
	std::sort(entries.begin(), entries.end(),
		[](const DirEntry& a, const DirEntry& b) -> bool {
			if (a.name == "..") return true;
			if (b.name == "..") return false;
			if (a.isDir != b.isDir) return a.isDir > b.isDir;
			return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
		});
	return entries;
}

// Generates the HTML header and styling for the auto-index response
void Server::generateAutoIndexHeader(Context& ctx, std::stringstream& content)
{
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
}

// Generates the HTML body containing directory entries for the auto-index response
void Server::generateAutoIndexBody(const std::vector<DirEntry>& entries, std::stringstream& content)
{
	for (const auto& entry : entries)
	{
		std::string displayName = entry.name;
		std::string className = entry.isDir ? "directory" : "";

		char timeStr[50];
		if (entry.name == "..")
		{
			std::strcpy(timeStr, "-");
		}
		else
		{
			struct tm* tm = localtime(&entry.mtime);
			strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", tm);
		}

		std::string sizeStr;
		if (entry.isDir || entry.name == "..")
		{
			sizeStr = "-";
		}
		else
		{
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
				<< "<a href=\"" << (entry.name == ".." ? "../" : entry.name + (entry.isDir ? "/" : ""))
				<< "\" " << "class=\"" << className << "\">"
				<< displayName << (entry.isDir ? "/" : "") << "</a></div>\n"
				<< "            <div class=\"modified\">" << timeStr << "</div>\n"
				<< "            <div class=\"size\">" << sizeStr << "</div>\n"
				<< "        </div>\n";
	}
}
