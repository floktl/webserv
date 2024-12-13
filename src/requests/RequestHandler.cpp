/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:17 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/13 08:14:28 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "RequestHandler.hpp"
#include "./../utils/Logger.hpp"

#include <sys/epoll.h>   // For epoll functions
#include <iostream>      // For cerr
#include <fstream>       // For file handling
#include <sstream>       // For stringstream
#include <sys/wait.h>    // for waitpid
#include <dirent.h>      // for opendir/closedir
#include <sys/select.h> // For select
#include <cstddef>

// contructor
RequestHandler::RequestHandler(
	int _client_fd,
	const ServerBlock& _config,
	std::set<int>& _activeFds,
	std::map<int,const ServerBlock*>& _serverBlockConfigs
) : client_fd(_client_fd), config(_config), activeFds(_activeFds), serverBlockConfigs(_serverBlockConfigs), errorHandler(_client_fd, _config.errorPages) {}


bool RequestHandler::parseRequestLine(const std::string& request, std::string& method,
							std::string& requestedPath, std::string& version)
{
	std::istringstream requestStream(request);
	std::string requestLine;
	if (!std::getline(requestStream, requestLine))
		return false;
	if (!requestLine.empty() && requestLine.back() == '\r')
		requestLine.pop_back();

	std::istringstream lineStream(requestLine);
	lineStream >> method >> requestedPath >> version;
	if (method.empty() || requestedPath.empty() || version.empty())
		return false;

	return true;
}

void RequestHandler::parseHeaders(std::istringstream& requestStream, std::map<std::string, std::string>& headersMap)
{
	std::string headerLine;
	while (std::getline(requestStream, headerLine) && headerLine != "\r") {
		if (!headerLine.empty() && headerLine.back() == '\r')
			headerLine.pop_back();
		size_t colonPos = headerLine.find(":");
		if (colonPos != std::string::npos) {
			std::string key = headerLine.substr(0, colonPos);
			std::string value = headerLine.substr(colonPos + 1);
			while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
				value.erase(value.begin());
			headersMap[key] = value;
		}
	}
}

std::string RequestHandler::extractRequestBody(std::istringstream& requestStream)
{
	std::string requestBody;
	std::string remaining;
	while (std::getline(requestStream, remaining))
		requestBody += remaining + "\n";
	if (!requestBody.empty() && requestBody.back() == '\n')
		requestBody.pop_back();
	return requestBody;
}

const Location* RequestHandler::findLocation(const std::string& requestedPath)
{
	for (const auto& loc : config.locations) {
		if (requestedPath.find(loc.path) == 0) {
			return &loc;
		}
	}
	return nullptr;
}

bool RequestHandler::handleDeleteMethod(const std::string& filePath)
{
	if (remove(filePath.c_str()) == 0) {
		std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
							"<html><body><h1>File Deleted</h1></body></html>";
		send(client_fd, response.c_str(), response.size(), 0);
	} else {
		errorHandler.sendErrorResponse(404, "File Not Found");
	}
	closeConnection();
	return true;
}

bool RequestHandler::handleDirectoryIndex(std::string& filePath)
{
	DIR* dir = opendir(filePath.c_str());
	if (dir != nullptr) {
		closedir(dir);

		std::string indexFiles = config.index;
		if (indexFiles.empty()) {
			errorHandler.sendErrorResponse(403, "Forbidden");
			closeConnection();
			return true;
		}

		std::istringstream iss(indexFiles);
		std::string singleIndex;
		bool foundIndex = false;
		while (iss >> singleIndex) {
			std::string tryPath = filePath;
			if (tryPath.back() != '/')
				tryPath += "/";
			tryPath += singleIndex;

			std::ifstream testFile(tryPath);
			if (testFile.is_open()) {
				filePath = tryPath;
				testFile.close();
				foundIndex = true;
				break;
			}
		}

		if (!foundIndex) {
			Logger::red("No index file found in directory: " + filePath);
			errorHandler.sendErrorResponse(404, "File Not Found");
			closeConnection();
			return true;
		}
	}
	return false;
}

void RequestHandler::handleStaticFile(const std::string& filePath)
{
	std::ifstream file(filePath);
	if (file.is_open()) {
		std::stringstream fileContent;
		fileContent << file.rdbuf();
		std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
		response += fileContent.str();
		int sent = send(client_fd, response.c_str(), response.size(), 0);
		if (sent < 0) {
			Logger::red("Error sending file: " + std::string(strerror(errno)));
		}
	} else {
		Logger::red("Failed to open file: " + filePath);
		errorHandler.sendErrorResponse(404, "File Not Found");
	}
	closeConnection();
}

void RequestHandler::handle_request()
{
	char buffer[4096];
	std::memset(buffer, 0, sizeof(buffer));

	int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes_read < 0) {
		Logger::red("Error reading request: " + std::string(strerror(errno)));
		closeConnection();
		return;
	}

	std::string request(buffer, bytes_read);

	std::string method, requestedPath, version;
	if (!parseRequestLine(request, method, requestedPath, version)) {
		errorHandler.sendErrorResponse(400, "Bad Request");
		closeConnection();
		return;
	}

	// Extract headers
	std::istringstream requestStream(request);
	std::string dummyLine;
	std::getline(requestStream, dummyLine);
	std::map<std::string, std::string> headersMap;
	parseHeaders(requestStream, headersMap);

	// Extract body if POST
	std::string requestBody;
	if (method == "POST") {
		requestBody = extractRequestBody(requestStream);
	}

	// Find corresponding location
	const Location* location = findLocation(requestedPath);
	if (!location) {
		errorHandler.sendErrorResponse(404, "Not Found");
		closeConnection();
		return;
	}

	std::string root = location->root.empty() ? config.root : location->root;
	std::string filePath = root + requestedPath;

	// Handle DELETE
	if (method == "DELETE") {
		if (handleDeleteMethod(filePath))
			return;
	}

	// Handle directories & index
	if (handleDirectoryIndex(filePath)) {
		return;
	}

	// Handle CGI
	CgiHandler cgiHandler(
		client_fd, location, filePath, method, requestedPath, requestBody, headersMap, activeFds, config, serverBlockConfigs
	);
	if (cgiHandler.handleCGIIfNeeded()) {
		return;
	}


	// Handle files (GET / POST)
	if (method == "GET" || method == "POST") {
		handleStaticFile(filePath);
		return;
	}

	errorHandler.sendErrorResponse(405, "Method Not Allowed");
	closeConnection();
}

void RequestHandler::closeConnection()
{
	activeFds.erase(client_fd);
	serverBlockConfigs.erase(client_fd);
	close(client_fd);
}