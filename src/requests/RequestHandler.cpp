/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:17 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/11 16:19:54 by jeberle          ###   ########.fr       */
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
    std::map<int, const ServerBlock*>& _serverBlockConfigs
) : client_fd(_client_fd), config(_config), activeFds(_activeFds), serverBlockConfigs(_serverBlockConfigs){}

std::string getFileExtension(const std::string& filePath)
{
	size_t pos = filePath.find_last_of(".");
	if (pos == std::string::npos) {
		Logger::red("No file extension found, check to executable cgi target");
		return "";
	}
	std::string ext = filePath.substr(pos);
	return ext;
}

void RequestHandler::sendErrorResponse(int statusCode, const std::string& message, const std::map<int, std::string>& errorPages) {
	auto it = errorPages.find(statusCode);
	if (it != errorPages.end()) {
		std::ifstream errorFile(it->second);
		if (errorFile.is_open()) {
			std::stringstream fileContent;
			fileContent << errorFile.rdbuf();
			std::string response = "HTTP/1.1 " + std::to_string(statusCode) + " " + message + "\r\n"
								"Content-Type: text/html\r\n\r\n" + fileContent.str();
			send(client_fd, response.c_str(), response.size(), 0);
			return;
		} else {
			Logger::red("Error loading custom error page: " + it->second);
		}
	}

	std::ostringstream response;
	response << "HTTP/1.1 " << statusCode << " " << message << "\r\n"
			<< "Content-Type: text/html\r\n\r\n"
			<< "<html><body><h1>" << statusCode << " " << message << "</h1></body></html>";
	send(client_fd, response.str().c_str(), response.str().size(), 0);
}

void RequestHandler::closePipe(int fds[2]) {
	close(fds[0]);
	close(fds[1]);
}

bool RequestHandler::createPipes(int pipefd_out[2], int pipefd_in[2]) {
	if (pipe(pipefd_out) == -1) {
		Logger::red("Pipe creation failed (out): " + std::string(strerror(errno)));
		return false;
	}
	if (pipe(pipefd_in) == -1) {
		Logger::red("Pipe creation failed (in): " + std::string(strerror(errno)));
		close(pipefd_out[0]);
		close(pipefd_out[1]);
		return false;
	}
	return true;
}

void RequestHandler::setupChildEnv(const std::map<std::string, std::string>& cgiParams, std::vector<char*>& env) {
	for (const auto& param : cgiParams) {
		std::string envVar = param.first + "=" + param.second;
		env.push_back(strdup(envVar.c_str()));
	}
	env.push_back(nullptr);
}

void RequestHandler::runCgiChild(const std::string& cgiPath, const std::string& scriptPath,
				int pipefd_out[2], int pipefd_in[2],
				const std::map<std::string, std::string>& cgiParams)
{
	dup2(pipefd_out[1], STDOUT_FILENO);
	close(pipefd_out[0]);
	close(pipefd_out[1]);

	dup2(pipefd_in[0], STDIN_FILENO);
	close(pipefd_in[0]);
	close(pipefd_in[1]);

	std::vector<char*> env;
	setupChildEnv(cgiParams, env);

	char* args[] = {
		const_cast<char*>(cgiPath.c_str()),
		const_cast<char*>(scriptPath.c_str()),
		nullptr
	};
	execve(cgiPath.c_str(), args, env.data());
	Logger::red("execve failed: " + std::string(strerror(errno)));
	exit(EXIT_FAILURE);
}

void RequestHandler::writeRequestBodyIfNeeded(int pipe_in, const std::string& method, const std::string& requestBody) {
	if (method == "POST" && !requestBody.empty()) {
		ssize_t written = write(pipe_in, requestBody.c_str(), requestBody.size());
		if (written < 0)
			Logger::red("Error writing to CGI stdin: " + std::string(strerror(errno)));
	}
	close(pipe_in);
}

std::string RequestHandler::readCgiOutput(int pipe_out) {
	std::string cgi_output;
	char buffer[4096];
	int bytes;
	while ((bytes = read(pipe_out, buffer, sizeof(buffer))) > 0) {
		cgi_output.append(buffer, bytes);
	}
	close(pipe_out);
	return cgi_output;
}

void RequestHandler::parseCgiOutput(std::string& headers, std::string& body, int pipefd_out[2], [[maybe_unused]] int pipefd_in[2], pid_t pid) {
	char buffer[4096];
	int bytes;
	struct timeval timeout;
	timeout.tv_sec = 5;
	timeout.tv_usec = 0;

	fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(pipefd_out[0], &readfds);

	std::string output;
	while (true) {
		fd_set tempfds = readfds;
		int activity = select(pipefd_out[0] + 1, &tempfds, NULL, NULL, &timeout);
		if (activity == -1) {
			break;
		} else if (activity == 0) {
			Logger::red("CGI script timed out");
			kill(pid, SIGKILL);
			close(pipefd_out[0]);
			return;
		}

		if (FD_ISSET(pipefd_out[0], &tempfds)) {
			bytes = read(pipefd_out[0], buffer, sizeof(buffer));
			if (bytes > 0) {
				output.append(buffer, static_cast<size_t>(bytes));
			} else {
				break;
			}
		}
	}

	size_t header_end = output.find("\r\n\r\n");
	if (header_end != std::string::npos) {
		headers = output.substr(0, header_end);
		body = output.substr(header_end + 4);
	} else {
		// No headers found
		body = output;
		headers = "Content-Type: text/html; charset=UTF-8";
	}
}



bool RequestHandler::checkForRedirect(const std::string& headers) {
	if (headers.find("Status: 302") != std::string::npos) {
		size_t location_pos = headers.find("Location:");
		if (location_pos != std::string::npos) {
			size_t end_pos = headers.find("\r\n", location_pos);
			std::string location = headers.substr(location_pos + 9, end_pos - location_pos - 9);
			location.erase(0, location.find_first_not_of(" \t"));
			location.erase(location.find_last_not_of(" \t") + 1);

			// Send Redirect Response
			std::string redirect_response = "HTTP/1.1 302 Found\r\n";
			redirect_response += "Location: " + location + "\r\n\r\n";
			int sent = send(client_fd, redirect_response.c_str(), redirect_response.size(), 0);
			if (sent < 0) {
				Logger::red("Failed to send Redirect response: " + std::string(strerror(errno)));
			}
			return true;
		}
	}
	return false;
}

void RequestHandler::sendCgiResponse(const std::string& headers, const std::string& body) {
	std::string response = "HTTP/1.1 200 OK\r\n" + headers + "\r\n\r\n" + body;
	int sent = send(client_fd, response.c_str(), response.length(), 0);
	if (sent < 0) {
		Logger::red("Failed to send CGI response: " + std::string(strerror(errno)));
	}
}

void RequestHandler::waitForChild(pid_t pid) {
	int status;
	waitpid(pid, &status, 0);
}

void RequestHandler::executeCGI(const std::string& cgiPath, const std::string& scriptPath,
				const std::map<std::string, std::string>& cgiParams,
				const std::string& requestBody, const std::string& method) {
	int pipefd_out[2]; // For CGI stdout
	int pipefd_in[2];  // For CGI stdin
	if (!createPipes(pipefd_out, pipefd_in)) {
		return;
	}

	pid_t pid = fork();
	if (pid < 0) {
		Logger::red("Fork failed: " + std::string(strerror(errno)));
		closePipe(pipefd_out);
		closePipe(pipefd_in);
		return;
	}

	if (pid == 0) {
		// Child
		runCgiChild(cgiPath, scriptPath, pipefd_out, pipefd_in, cgiParams);
	} else {
		// Parent
		close(pipefd_out[1]);
		close(pipefd_in[0]);

		writeRequestBodyIfNeeded(pipefd_in[1], method, requestBody);

		std::string headers;
		std::string body;
		parseCgiOutput(headers, body, pipefd_out, pipefd_in, pid);

		if (checkForRedirect(headers)) {
			close(client_fd);
			waitForChild(pid);
			return;
		}

		sendCgiResponse(headers, body);

		close(client_fd);
		waitForChild(pid);
	}
}

void RequestHandler::closeConnection()
{
	activeFds.erase(client_fd);
	serverBlockConfigs.erase(client_fd);
	close(client_fd);
}

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

void RequestHandler::parseHeaders(std::istringstream& requestStream, std::map<std::string, std::string>& headers)
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
			headers[key] = value;
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
		sendErrorResponse(404, "File Not Found", config.error_pages);
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
			sendErrorResponse(403, "Forbidden", config.error_pages);
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
			sendErrorResponse(404, "File Not Found", config.error_pages);
			closeConnection();
			return true;
		}
	}
	return false;
}

bool RequestHandler::handleCGIIfNeeded(const Location* location, const std::string& filePath,
							const std::string& method, const std::string& requestedPath,
							const std::string& requestBody,
							const std::map<std::string, std::string>& headers)
{
	if (!location->cgi.empty()) {
		std::string ext = getFileExtension(filePath);
		if (filePath.size() >= ext.size() &&
			filePath.compare(filePath.size() - ext.size(), ext.size(), ext) == 0) {
			std::map<std::string, std::string> cgiParams;
			cgiParams["SCRIPT_FILENAME"] = filePath;
			cgiParams["DOCUMENT_ROOT"] = location->root.empty() ? "" : location->root;
			cgiParams["QUERY_STRING"] = "";
			cgiParams["REQUEST_METHOD"] = method;
			cgiParams["SERVER_PROTOCOL"] = "HTTP/1.1";
			cgiParams["GATEWAY_INTERFACE"] = "CGI/1.1";
			cgiParams["SERVER_SOFTWARE"] = "my-cpp-server/1.0";
			cgiParams["REDIRECT_STATUS"] = "200";
			cgiParams["SERVER_NAME"] = "localhost";
			cgiParams["SERVER_PORT"] = "8001";
			cgiParams["REMOTE_ADDR"] = "127.0.0.1";
			cgiParams["REQUEST_URI"] = requestedPath;
			cgiParams["SCRIPT_NAME"] = requestedPath;

			if (method == "POST") {
				std::map<std::string, std::string>::const_iterator it = headers.find("Content-Length");
				if (it != headers.end()) {
					cgiParams["CONTENT_LENGTH"] = it->second;
				}
				it = headers.find("Content-Type");
				if (it != headers.end()) {
					cgiParams["CONTENT_TYPE"] = it->second;
				}
			}
			Logger::green("test 4");

			executeCGI(location->cgi, filePath, cgiParams, requestBody, method);
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
		sendErrorResponse(404, "File Not Found", config.error_pages);
	}
	closeConnection();
}

void RequestHandler::handle_request()
{
	char buffer[4096];
	std::memset(buffer, 0, sizeof(buffer));
	Logger::yellow("ERROR DEFINE: " + config.index);
for (const auto& ep : config.error_pages) {
	Logger::yellow("ERROR DEFINE: " + std::to_string(ep.first) + " -> " + ep.second);
}
	int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes_read < 0) {
		Logger::red("Error reading request: " + std::string(strerror(errno)));
		closeConnection();
		return;
	}

	std::string request(buffer, bytes_read);

	std::string method, requestedPath, version;
	if (!parseRequestLine(request, method, requestedPath, version)) {
		sendErrorResponse(400, "Bad Request", config.error_pages);
		closeConnection();
		return;
	}

	// Extract headers
	std::istringstream requestStream(request);
	std::string dummyLine;
	std::getline(requestStream, dummyLine);
	std::map<std::string, std::string> headers;
	parseHeaders(requestStream, headers);

	// Extract body if POST
	std::string requestBody;
	if (method == "POST") {
		requestBody = extractRequestBody(requestStream);
	}

	// Find corresponding location
	const Location* location = findLocation(requestedPath);
	if (!location) {
		Logger::red("No matching location found for path: " + requestedPath);
		sendErrorResponse(404, "Not Found", config.error_pages);
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
	if (handleCGIIfNeeded(location, filePath, method, requestedPath, requestBody, headers)) {
		return;
	}

	// Handle files (GET / POST)
	if (method == "GET" || method == "POST") {
		handleStaticFile(filePath);
		return;
	}

	sendErrorResponse(405, "Method Not Allowed", config.error_pages);
	closeConnection();
}
