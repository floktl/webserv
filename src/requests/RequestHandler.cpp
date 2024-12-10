/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:17 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/10 14:05:11 by fkeitel          ###   ########.fr       */
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

void sendErrorResponse(int client_fd, int statusCode, const std::string& message)
{
	std::ostringstream response;
	response << "HTTP/1.1 " << statusCode << " " << message << "\r\n"
			<< "Content-Type: text/html\r\n\r\n"
			<< "<html><body><h1>" << statusCode << " " << message << "</h1></body></html>";
	int sent = send(client_fd, response.str().c_str(), response.str().size(), 0);
	if (sent < 0)
		Logger::red("Error sending response: " + std::string(strerror(errno)));
}

void closePipe(int fds[2]) {
	close(fds[0]);
	close(fds[1]);
}

bool createPipes(int pipefd_out[2], int pipefd_in[2]) {
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

void setupChildEnv(const std::map<std::string, std::string>& cgiParams, std::vector<char*>& env) {
	for (const auto& param : cgiParams) {
		std::string envVar = param.first + "=" + param.second;
		env.push_back(strdup(envVar.c_str()));
	}
	env.push_back(nullptr);
}

void runCgiChild(const std::string& cgiPath, const std::string& scriptPath,
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

void writeRequestBodyIfNeeded(int pipe_in, const std::string& method, const std::string& requestBody) {
	if (method == "POST" && !requestBody.empty()) {
		ssize_t written = write(pipe_in, requestBody.c_str(), requestBody.size());
		if (written < 0)
			Logger::red("Error writing to CGI stdin: " + std::string(strerror(errno)));
	}
	close(pipe_in);
}

std::string readCgiOutput(int pipe_out) {
	std::string cgi_output;
	char buffer[4096];
	int bytes;
	while ((bytes = read(pipe_out, buffer, sizeof(buffer))) > 0) {
		cgi_output.append(buffer, bytes);
	}
	close(pipe_out);
	return cgi_output;
}

void parseCgiOutput(const std::string& cgi_output, std::string& headers, std::string& body, int pipefd_out[2], int pipefd_in[2]){

	        // Read CGI output with timeout
        char buffer[4096];
        int bytes;
        struct timeval timeout;
        timeout.tv_sec = 5;  // Timeout: 5 seconds
        timeout.tv_usec = 0; // No microseconds

        fd_set readfds;
        FD_ZERO(&readfds);
        FD_SET(pipefd_out[0], &readfds);

        while (true)
        {
            fd_set tempfds = readfds; // Preserve original set
            int activity = select(pipefd_out[0] + 1, &tempfds, NULL, NULL, &timeout);
            if (activity == -1)
            {
                break;
            }
            else if (activity == 0)
            {
				//Logger::red("Request timed out for client_fd: " + std::to_string(client_fd));
				//sendErrorResponse(client_fd, 408, "Request Timeout"); // HTTP 408 Request Timeout
				//close(client_fd);
				//activeFds.erase(client_fd);
				//serverBlockConfigs.erase(client_fd);
                Logger::red("CGI script timed out");
                kill(pid, SIGKILL); // Terminate the child process
                close(pipefd_out[0]);
                return;
            }

            if (FD_ISSET(pipefd_out[0], &tempfds))
            {
                bytes = read(pipefd_out[0], buffer, sizeof(buffer));
                if (bytes > 0)
                {
                    cgi_output.append(buffer, bytes);
                }
                else
                {
                    break;
                }
            }
    }
	size_t header_end = cgi_output.find("\r\n\r\n");
	if (header_end != std::string::npos) {
		headers = cgi_output.substr(0, header_end);
		body = cgi_output.substr(header_end + 4);
	} else {
		// No headers found
		body = cgi_output;
		headers = "Content-Type: text/html; charset=UTF-8";
	}
}

bool checkForRedirect(const std::string& headers, int client_fd) {
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

void sendCgiResponse(int client_fd, const std::string& headers, const std::string& body) {
	std::string response = "HTTP/1.1 200 OK\r\n" + headers + "\r\n\r\n" + body;
	int sent = send(client_fd, response.c_str(), response.length(), 0);
	if (sent < 0) {
		Logger::red("Failed to send CGI response: " + std::string(strerror(errno)));
	}
}

void waitForChild(pid_t pid) {
	int status;
	waitpid(pid, &status, 0);
}

void executeCGI(const std::string& cgiPath, const std::string& scriptPath,
                const std::map<std::string, std::string>& cgiParams, int client_fd,
                const std::string& requestBody, const std::string& method)
{
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

		std::string cgi_output = readCgiOutput(pipefd_out[0]);

		std::string headers;
		std::string body;
		parseCgiOutput(cgi_output, headers, body, pipefd_out, pipefd_in);

		if (checkForRedirect(headers, client_fd)) {
			close(client_fd);
			waitForChild(pid);
			return;
		}

		sendCgiResponse(client_fd, headers, body);

		close(client_fd);
		waitForChild(pid);
	}
}

static void closeConnection(int client_fd,
							std::set<int>& activeFds,
							std::map<int, const ServerBlock*>& serverBlockConfigs)
{
	activeFds.erase(client_fd);
	serverBlockConfigs.erase(client_fd);
	close(client_fd);
}

static bool parseRequestLine(const std::string& request, std::string& method,
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

static void parseHeaders(std::istringstream& requestStream, std::map<std::string, std::string>& headers)
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

static std::string extractRequestBody(std::istringstream& requestStream)
{
	std::string requestBody;
	std::string remaining;
	while (std::getline(requestStream, remaining))
		requestBody += remaining + "\n";
	if (!requestBody.empty() && requestBody.back() == '\n')
		requestBody.pop_back();
	return requestBody;
}

static const Location* findLocation(const ServerBlock& config, const std::string& requestedPath)
{
	for (const auto& loc : config.locations) {
		if (requestedPath.find(loc.path) == 0) {
			return &loc;
		}
	}
	return nullptr;
}

static bool handleDeleteMethod(const std::string& filePath, int client_fd,
							std::set<int>& activeFds,
							std::map<int, const ServerBlock*>& serverBlockConfigs)
{
	if (remove(filePath.c_str()) == 0) {
		std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
							"<html><body><h1>File Deleted</h1></body></html>";
		send(client_fd, response.c_str(), response.size(), 0);
	} else {
		sendErrorResponse(client_fd, 404, "File Not Found");
	}
	closeConnection(client_fd, activeFds, serverBlockConfigs);
	return true;
}

static bool handleDirectoryIndex(const ServerBlock& config, std::string& filePath,
								int client_fd, std::set<int>& activeFds,
								std::map<int, const ServerBlock*>& serverBlockConfigs)
{
	DIR* dir = opendir(filePath.c_str());
	if (dir != nullptr) {
		closedir(dir);

		std::string indexFiles = config.index;
		if (indexFiles.empty()) {
			sendErrorResponse(client_fd, 403, "Forbidden");
			closeConnection(client_fd, activeFds, serverBlockConfigs);
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
			sendErrorResponse(client_fd, 404, "File Not Found");
			closeConnection(client_fd, activeFds, serverBlockConfigs);
			return true;
		}
	}
	return false;
}

static bool handleCGIIfNeeded(const Location* location, const std::string& filePath,
							const std::string& method, const std::string& requestedPath,
							const std::string& requestBody,
							const std::map<std::string, std::string>& headers,
							int client_fd, std::set<int>& activeFds,
							std::map<int, const ServerBlock*>& serverBlockConfigs)
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

			executeCGI(location->cgi, filePath, cgiParams, client_fd, requestBody, method);
			closeConnection(client_fd, activeFds, serverBlockConfigs);
			return true;
		}
	}
	return false;
}

static void handleStaticFile(const std::string& filePath,
							int client_fd,
							std::set<int>& activeFds,
							std::map<int, const ServerBlock*>& serverBlockConfigs)
{
	std::ifstream file(filePath);
	if (file.is_open()) {
		std::stringstream fileContent;
		fileContent << file.rdbuf();
		std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
		response += fileContent.str();
		int sent = send(client_fd, response.c_str(), response.size(), 0);
		if (sent < 0) {
			Logger::red("Error sending static file: " + std::string(strerror(errno)));
		}
	} else {
		Logger::red("Failed to open static file: " + filePath);
		sendErrorResponse(client_fd, 404, "File Not Found");
	}
	closeConnection(client_fd, activeFds, serverBlockConfigs);
}

void RequestHandler::handle_request(int client_fd, const ServerBlock& config,
									std::set<int>& activeFds,
									std::map<int, const ServerBlock*>& serverBlockConfigs)
{
	char buffer[4096];
	std::memset(buffer, 0, sizeof(buffer));

	int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes_read < 0) {
		Logger::red("Error reading request: " + std::string(strerror(errno)));
		closeConnection(client_fd, activeFds, serverBlockConfigs);
		return;
	}

	std::string request(buffer, bytes_read);

	std::string method, requestedPath, version;
	if (!parseRequestLine(request, method, requestedPath, version)) {
		sendErrorResponse(client_fd, 400, "Bad Request");
		closeConnection(client_fd, activeFds, serverBlockConfigs);
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
	const Location* location = findLocation(config, requestedPath);
	if (!location) {
		Logger::red("No matching location found for path: " + requestedPath);
		sendErrorResponse(client_fd, 404, "Not Found");
		closeConnection(client_fd, activeFds, serverBlockConfigs);
		return;
	}

	std::string root = location->root.empty() ? config.root : location->root;
	std::string filePath = root + requestedPath;

	// Handle DELETE
	if (method == "DELETE") {
		if (handleDeleteMethod(filePath, client_fd, activeFds, serverBlockConfigs))
			return;
	}

	// Handle directories & index
	if (handleDirectoryIndex(config, filePath, client_fd, activeFds, serverBlockConfigs)) {
		return;
	}

	// Handle CGI
	if (handleCGIIfNeeded(location, filePath, method, requestedPath, requestBody, headers,
						client_fd, activeFds, serverBlockConfigs)) {
		return;
	}

	// Handle static files (GET / POST)
	if (method == "GET" || method == "POST") {
		handleStaticFile(filePath, client_fd, activeFds, serverBlockConfigs);
		return;
	}

	sendErrorResponse(client_fd, 405, "Method Not Allowed");
	closeConnection(client_fd, activeFds, serverBlockConfigs);
}
