#include "Server.hpp"

void Server::buildStaticResponse(Context &ctx)
{
	std::string fullPath = ctx.approved_req_path;

	if (ctx.is_download) {
		ctx.multipart_fd_up_down = -1;
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		return;
	}

	std::ifstream file(fullPath, std::ios::binary);
	if (!file) {
		updateErrorStatus(ctx, 404, "Not found");
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

	for (const auto& cookiePair : ctx.setCookies) {
		Cookie cookie;
		cookie.name = cookiePair.first;
		cookie.value = cookiePair.second;
		cookie.path = "/";
		http_response << generateSetCookieHeader(cookie) << "\r\n";
	}

	http_response << "\r\n" << content_str;
	sendHandler(ctx, http_response.str());
	resetContext(ctx);
}

// Builds on Auto-Index (Directory listing) Response When no index file is found
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

// Quees to http response in the output buffer and updates epoll events for writing
bool Server::queueResponse(Context& ctx, const std::string& response)
{
	ctx.output_buffer += response;
	modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
	return true;
}

// Retrieves Directory Entries, Filtering and Sorting Them for Directory Listing
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
			Logger::errorLog("stat failed for path: " + fullPath + " with error");
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

// Generates The HTML Header and Styling for the Auto-Index Response
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
			<< "body {\n"
			<< "	margin: 0;\n"
			<< "	padding: 40px;\n"
			<< "	height: 100vh;\n"
			<< "	overflow: hidden;\n"
			<< "	background: linear-gradient(\n"
			<< "		to bottom,\n"
			<< "		#ff0000,\n"
			<< "		#ff7f00,\n"
			<< "		#ffff00,\n"
			<< "		#00ff00,\n"
			<< "		#0000ff,\n"
			<< "		#4b0082,\n"
			<< "		#9400d3,\n"
			<< "		#ff0000\n"
			<< "	);\n"
			<< "	background-size: 100% 8000%;\n"
			<< "	animation: rainbow-animation 10s linear infinite;\n"
			<< "}\n"
			<< "@keyframes rainbow-animation {\n"
			<< "	0% {\n"
			<< "		background-position: 0% 0%;\n"
			<< "	}\n"
			<< "	100% {\n"
			<< "		background-position: 0% 100%;\n"
			<< "	}\n"
			<< "}\n"
			<< "        h1 {\n"
			<< "            border-bottom: 1px solid #eee;\n"
			<< "            padding-bottom: 10px;\n"
			<< "            color: white;\n"
			<< "            mix-blend-mode: difference;\n"
			<< "        }\n"
			<< "        .listing {\n"
			<< "            display: table;\n"
			<< "            width: 100%;\n"
			<< "            border-collapse: collapse;\n"
			<< "        }\n"
			<< "        .entry {\n"
			<< "            display: table-row;\n"
			<< "            background-color: white;\n"
			<< "            mix-blend-mode: difference;\n"
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

// Generates The HTML Body Containing Directory Entries for the Auto-Index Response
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
