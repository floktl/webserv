#include "RequestHandler.hpp"

RequestHandler::RequestHandler(Server& _server)
	: server(_server) {}

void RequestHandler::buildErrorResponse(int statusCode, const std::string& message, std::stringstream *response, RequestState &req)
{
	// Access the ErrorHandler instance via the Server class
	ErrorHandler* errorHandler = server.getErrorHandler();
	if (errorHandler)
	{
		*response << errorHandler->generateErrorResponse(statusCode, message, req);
	}
	else
	{
		// Fallback if ErrorHandler is not available
		*response << "HTTP/1.1 " << statusCode << " " << message << "\r\n"
			<< "Content-Type: text/html\r\n\r\n"
			<< "<html><body><h1>" << statusCode << " " << message << "</h1></body></html>";
	}
}

#include <fstream>
#include <sstream>
#include <iostream>
#include <string>

#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <algorithm>
#include <iomanip>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <algorithm>
#include <iomanip>
#include <vector>

#include <dirent.h>
#include <sys/stat.h>
#include <time.h>
#include <algorithm>
#include <iomanip>
#include <vector>

void RequestHandler::buildAutoindexResponse(std::stringstream* response, RequestState& req) {
	Logger::file("Attempting autoindex for path: " + req.requested_path);

	// Check directory permissions first
	if (access(req.requested_path.c_str(), R_OK) != 0) {
		Logger::file("Directory not readable: " + req.requested_path + " - " + std::string(strerror(errno)));
		buildErrorResponse(403, "Permission denied", response, req);
		return;
	}

	DIR* dir = opendir(req.requested_path.c_str());
	if (!dir) {
		Logger::file("Failed to open directory: " + std::string(strerror(errno)));
		buildErrorResponse(404, "Directory not found", response, req);
		return;
	}
	Logger::file("Successfully opened directory: " + req.requested_path);

	struct DirEntry {
		std::string name;
		bool isDir;
		time_t mtime;
		off_t size;
	};
	std::vector<DirEntry> entries;

	struct dirent* dir_entry;
	while ((dir_entry = readdir(dir)) != NULL) {
		std::string name = dir_entry->d_name;
		if (name == "." || name == "..") {
			if (req.location_path != "/") {  // Only add parent directory if not at root
				if (name == "..") {
					DirEntry entry;
					entry.name = name;
					entry.isDir = true;
					entry.mtime = 0;
					entry.size = 0;
					entries.push_back(entry);
				}
			}
			continue;
		}

		std::string fullPath = req.requested_path;
		if (fullPath.back() != '/') fullPath += '/';
		fullPath += name;

		struct stat statbuf;
		if (stat(fullPath.c_str(), &statbuf) != 0) {
			Logger::file("Failed to stat file: " + fullPath + " - " + std::string(strerror(errno)));
			continue;
		}

		DirEntry entry;
		entry.name = name;
		entry.isDir = S_ISDIR(statbuf.st_mode);
		entry.mtime = statbuf.st_mtime;
		entry.size = statbuf.st_size;
		entries.push_back(entry);
	}
	closedir(dir);

	// Sort entries: directories first, then alphabetically
	std::sort(entries.begin(), entries.end(),
		[](const DirEntry& a, const DirEntry& b) -> bool {
			if (a.name == "..") return true;  // Parent directory always first
			if (b.name == "..") return false;
			if (a.isDir != b.isDir) return a.isDir > b.isDir;
			return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;  // Case-insensitive sort
		});

	// Build HTTP response
	*response << "HTTP/1.1 200 OK\r\n";
	*response << "Content-Type: text/html; charset=UTF-8\r\n\r\n";

	// Start HTML document
	*response << "<!DOCTYPE html>\n"
			<< "<html>\n"
			<< "<head>\n"
			<< "    <meta charset=\"UTF-8\">\n"
			<< "    <title>Index of " << req.location_path << "</title>\n"
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
			<< "        .name {\n"
			<< "            width: 50%;\n"
			<< "        }\n"
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
			<< "        a:hover {\n"
			<< "            text-decoration: underline;\n"
			<< "        }\n"
			<< "        .directory {\n"
			<< "            color: #6f42c1;\n"
			<< "        }\n"
			<< "    </style>\n"
			<< "</head>\n"
			<< "<body>\n"
			<< "    <h1>Index of " << req.location_path << "</h1>\n"
			<< "    <div class=\"listing\">\n";

	// Directory entries
	for (const auto& entry : entries) {
		std::string displayName = entry.name;
		std::string className = entry.isDir ? "directory" : "";

		// Format the modification time
		char timeStr[50];
		if (entry.name == "..") {
			strcpy(timeStr, "-");
		} else {
			struct tm* tm = localtime(&entry.mtime);
			strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", tm);
		}

		// Format the size
		std::string sizeStr;
		if (entry.isDir || entry.name == "..") {
			sizeStr = "-";
		} else {
			if (entry.size < 1024) {
				sizeStr = std::to_string(entry.size) + "B";
			} else if (entry.size < 1024 * 1024) {
				sizeStr = std::to_string(entry.size / 1024) + "K";
			} else if (entry.size < 1024 * 1024 * 1024) {
				sizeStr = std::to_string(entry.size / (1024 * 1024)) + "M";
			} else {
				sizeStr = std::to_string(entry.size / (1024 * 1024 * 1024)) + "G";
			}
		}

		// Build the entry HTML
		*response << "        <div class=\"entry\">\n"
				<< "            <div class=\"name\">"
				<< "<a href=\"" << (entry.name == ".." ? "../" : entry.name + (entry.isDir ? "/" : "")) << "\" "
				<< "class=\"" << className << "\">"
				<< displayName << (entry.isDir ? "/" : "") << "</a></div>\n"
				<< "            <div class=\"modified\">" << timeStr << "</div>\n"
				<< "            <div class=\"size\">" << sizeStr << "</div>\n"
				<< "        </div>\n";
	}

	// Close HTML document
	*response << "    </div>\n"
			<< "</body>\n"
			<< "</html>\n";

	Logger::file("Generated directory listing with " + std::to_string(entries.size()) + " entries");
}

const Location* RequestHandler::findMatchingLocation(const ServerBlock* conf, const std::string& path) {
	if (!conf) return nullptr;

	const Location* bestMatch = nullptr;
	size_t bestMatchLength = 0;
	bool hasExactMatch = false;

	for (const auto& loc : conf->locations) {
		if (path == loc.path) {
			bestMatch = &loc;
			hasExactMatch = true;
			break;
		}
	}

	if (hasExactMatch) {
		return bestMatch;
	}

	for (const auto& loc : conf->locations) {
		bool isPrefix = loc.path.back() == '/';

		if (isPrefix) {
			if (path.compare(0, loc.path.length(), loc.path) == 0) {
				if (loc.path.length() > bestMatchLength) {
					bestMatch = &loc;
					bestMatchLength = loc.path.length();
				}
			}
		} else {
			if (path.length() >= loc.path.length() &&
				path.compare(0, loc.path.length(), loc.path) == 0 &&
				(path.length() == loc.path.length() || path[loc.path.length()] == '/')) {

				if (loc.path.length() > bestMatchLength) {
					bestMatch = &loc;
					bestMatchLength = loc.path.length();
				}
			}
		}
	}

	return bestMatch;
}

void RequestHandler::buildResponse(RequestState &req) {
const ServerBlock* conf = req.associated_conf;
if (!conf) return;
const Location* loc = findMatchingLocation(req.associated_conf, req.location_path);

std::string method = getMethod(req.request_buffer);

if (method == "POST") {
	std::string response = "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n";
	req.response_buffer.assign(response.begin(), response.end());
	return;
}

if (method == "DELETE") {
	if (access(req.requested_path.c_str(), F_OK) != 0) {
		std::stringstream response;
		buildErrorResponse(404, "Not Found", &response, req);
		std::string resp_str = response.str();
		req.response_buffer.assign(resp_str.begin(), resp_str.end());
		return;
	}

	if (unlink(req.requested_path.c_str()) != 0) {
		std::stringstream response;
		buildErrorResponse(500, "Internal Server Error", &response, req);
		std::string resp_str = response.str();
		req.response_buffer.assign(resp_str.begin(), resp_str.end());
		return;
	}

	std::string response = "HTTP/1.1 204 No Content\r\n\r\n";
	req.response_buffer.assign(response.begin(), response.end());
	return;
}

std::stringstream buffer;
std::stringstream response;

struct stat path_stat;
if (stat(req.requested_path.c_str(), &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
	if (loc && loc->doAutoindex) {
		Logger::file("Directory detected, building autoindex...");
		buildAutoindexResponse(&response, req);
	} else {
		buildErrorResponse(404, "Directory listing not allowed", &response, req);
	}
} else {
	std::ifstream file(req.requested_path.c_str());
	if (file.is_open()) {
		Logger::file("File opened: " + req.requested_path);
		buffer << file.rdbuf();
		std::string file_content = buffer.str();
		file.close();

		response << "HTTP/1.1 200 OK\r\n";
		response << "Content-Length: " << file_content.size() << "\r\n";
		response << "\r\n";
		response << file_content;
	} else {
		buildErrorResponse(404, "error 404 file not found", &response, req);
	}
}

std::string response_str = response.str();
req.response_buffer.assign(response_str.begin(), response_str.end());
}

std::string RequestHandler::buildRequestedPath(RequestState &req, const std::string &rawPath)
{
	const Location* loc = findMatchingLocation(req.associated_conf, rawPath);

	std::string usedRoot = (loc && !loc->root.empty()) ? loc->root : req.associated_conf->root;
	if (!usedRoot.empty() && usedRoot.back() != '/')
		usedRoot += '/';

	std::string relativePath = rawPath;
	if (!relativePath.empty() && relativePath.front() == '/')
		relativePath.erase(0, 1);

	bool isDirectoryRequest = relativePath.empty() || relativePath.back() == '/';

	if (isDirectoryRequest) {
		if (loc && loc->doAutoindex) {
			std::string fullPath = usedRoot + relativePath;
			struct stat path_stat;
			if (stat(fullPath.c_str(), &path_stat) == 0 && S_ISDIR(path_stat.st_mode)) {
				return fullPath;
			}
		}
		std::string usedIndex = (loc && !loc->default_file.empty())
							? loc->default_file
							: req.associated_conf->index;
		if (!usedIndex.empty()) {
			relativePath += usedIndex;
		}
	}
	return usedRoot + relativePath;
}

void RequestHandler::parseRequest(RequestState &req)
{
	std::string request(req.request_buffer.begin(), req.request_buffer.end());

	size_t header_end = request.find("\r\n\r\n");
	if (header_end == std::string::npos)
		return;

	std::string headers = request.substr(0, header_end);
	std::istringstream header_stream(headers);
	std::string requestLine;
	std::getline(header_stream, requestLine);

	std::string method, path, version;
	std::istringstream request_stream(requestLine);
	request_stream >> method >> path >> version;

	std::string query;
	size_t qpos = path.find('?');
	if (qpos != std::string::npos)
	{
		query = path.substr(qpos + 1);
		path = path.substr(0, qpos);
	}

	if (method == "POST") {
		req.request_body = request.substr(header_end + 4);
		std::string content_type_header = "Content-Type: ";
		size_t content_type_pos = headers.find(content_type_header);
		if (content_type_pos != std::string::npos) {
			size_t content_type_end = headers.find("\r\n", content_type_pos);
			req.content_type = headers.substr(
				content_type_pos + content_type_header.length(),
				content_type_end - (content_type_pos + content_type_header.length())
			);
		}
	}

	req.location_path = path;
	req.requested_path = buildRequestedPath(req, path);
	req.cgi_output_buffer.clear();

	if (server.getCgiHandler()->needsCGI(req, path))
	{
		req.state = RequestState::STATE_PREPARE_CGI;
		server.getCgiHandler()->addCgiTunnel(req, method, query);
	}
	else
	{
		buildResponse(req);
		req.state = RequestState::STATE_SENDING_RESPONSE;
		server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
	}
}

std::string RequestHandler::getMethod(const std::vector<char>& request_buffer) {
	if (request_buffer.empty()) return "";

	std::string request(request_buffer.begin(), request_buffer.end());
	std::istringstream iss(request);
	std::string method;
	iss >> method;

	return method;
}