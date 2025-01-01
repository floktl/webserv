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


#include <fstream>    // For file operations
#include <sstream>    // For std::stringstream
#include <iostream>   // For std::cout
#include <string>     // For std::string


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

void RequestHandler::buildResponse(RequestState &req)
{
	const ServerBlock* conf = req.associated_conf;

	if (!conf) return;

	// Ensure the root path ends with a '/'
	std::string root_path = conf->root;
	if (root_path.empty())
		root_path = conf->index;
	if (root_path.back() != '/')
		root_path += '/';

	std::string file_content;
	std::ifstream file(req.requested_path.c_str());

	std::stringstream buffer;

	std::stringstream response;
	if (file.is_open())
	{
		// Read the file's content into file_content
		buffer << file.rdbuf();
		file_content = buffer.str();
		file.close();
		// Construct HTTP response
		response << "HTTP/1.1 " << (file.is_open() ? "200 OK" : "404 Not Found") << "\r\n";
		response << "Content-Length: " << file_content.size() << "\r\n";
		response << "\r\n";
		response << file_content;
	}
	else
	{
		RequestHandler::buildErrorResponse(404, "error 404 file not found", &response, req);
	}
	std::string response_str = response.str();
	// Assign response to req.response_buffer
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

	if (relativePath.empty())
	{
		std::string usedIndex = (loc && !loc->default_file.empty())
								? loc->default_file
								: req.associated_conf->index;
		relativePath = !usedIndex.empty() ? usedIndex : "index.html";
	}
	Logger::file(">>>>> " + usedRoot + relativePath);
	return usedRoot + relativePath;
}

void RequestHandler::parseRequest(RequestState &req)
{
	std::string request(req.request_buffer.begin(), req.request_buffer.end());

	size_t pos = request.find("\r\n");
	if (pos == std::string::npos)
		return;
	std::string requestLine = request.substr(0, pos);

	std::string method, path, version;
	{
		size_t firstSpace = requestLine.find(' ');
		if (firstSpace == std::string::npos) return;
		method = requestLine.substr(0, firstSpace);

		size_t secondSpace = requestLine.find(' ', firstSpace + 1);
		if (secondSpace == std::string::npos) return;
		path = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);

		version = requestLine.substr(secondSpace + 1);
	}

	std::string query;
	size_t qpos = path.find('?');
	if (qpos != std::string::npos)
	{
		query = path.substr(qpos + 1);
		path = path.substr(0, qpos);
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
		printRequestState(req);
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