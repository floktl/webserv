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

	// Extract the relative path from req.requested_path (e.g., "/team/florian")
	std::string relative_path = req.requested_path.substr(req.requested_path.find_first_of('/', 7));

	// Remove the leading slash from relative_path
	if (!relative_path.empty() && relative_path.front() == '/')
		relative_path.erase(0, 1);

	// If relative_path is empty or "/", default to the index file
	if (relative_path.empty())
		relative_path = conf->index;

	// Final check: If file_path does not end with ".html", append it
	//if (relative_path.size() < 5 || relative_path.substr(relative_path.size() - 5) != ".html")
	//    relative_path += ".html";

	// Combine root path with the relative path
	std::string file_path = root_path + relative_path;

	// Try to open the file at the constructed path
	std::string file_content;
	std::ifstream file(file_path.c_str());

	std::stringstream buffer;

	std::cout << "File: " << file_path << std::endl;
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
	req.requested_path = "http://localhost:" + std::to_string(req.associated_conf->port) + path;
	req.cgi_output_buffer.clear();
	if (server.getCgiHandler()->needsCGI(req.associated_conf, path))
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