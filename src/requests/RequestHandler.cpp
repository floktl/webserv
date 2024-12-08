/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:17 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/08 15:07:25 by jeberle          ###   ########.fr       */
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


std::string getFileExtension(const std::string& filePath)
{
	size_t pos = filePath.find_last_of(".");
	if (pos == std::string::npos)
	{
		Logger::red("No file extension found, check to executable cgi target");
		return "";
	}
	std::string ext = filePath.substr(pos);
	return ext;
}

// std::string handleCGIResponse(const std::string& output) {
//     if (output.find("Content-Type:") == std::string::npos) {
//         return "Content-Type: text/html\r\n\r\n" + output;
//     }
//     return output;
// }

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

void executeCGI(const std::string& cgiPath, const std::string& scriptPath,
				const std::map<std::string, std::string>& cgiParams, int client_fd)
{
	int pipefd[2];
	if (pipe(pipefd) == -1)
	{
		Logger::red("Pipe creation failed: " + std::string(strerror(errno)));
		return;
	}

	pid_t pid = fork();
	if (pid < 0)
	{
		Logger::red("Fork failed: " + std::string(strerror(errno)));
		return;
	}

	if (pid == 0)
	{
		dup2(pipefd[1], STDOUT_FILENO);
		close(pipefd[0]);
		close(pipefd[1]);

		std::vector<char*> env;
		for (const auto& param : cgiParams)
		{
			std::string envVar = param.first + "=" + param.second;
			env.push_back(strdup(envVar.c_str()));
		}
		env.push_back(nullptr);

		char* args[] = {const_cast<char*>(cgiPath.c_str()), const_cast<char*>(scriptPath.c_str()), nullptr};
		execve(cgiPath.c_str(), args, env.data());
		Logger::red("execve failed: " + std::string(strerror(errno)));
		exit(EXIT_FAILURE);
	}
	else
	{
		close(pipefd[1]);

		// Read CGI output
		std::string cgi_output;
		char buffer[4096];
		int bytes;
		while ((bytes = read(pipefd[0], buffer, sizeof(buffer))) > 0)
		{
			cgi_output.append(buffer, bytes);
		}

		// Separate headers from body
		std::string headers;
		std::string body;
		size_t header_end = cgi_output.find("\r\n\r\n");

		if (header_end != std::string::npos)
		{
			headers = cgi_output.substr(0, header_end);
			body = cgi_output.substr(header_end + 4);
		}
		else
		{
			// No headers found, treat entire output as body
			body = cgi_output;
			headers = "Content-Type: text/html; charset=UTF-8";
		}

		// Construct and send HTTP response
		std::string response = "HTTP/1.1 200 OK\r\n" + headers + "\r\n\r\n" + body;
		int sent = send(client_fd, response.c_str(), response.length(), 0);

		if (sent < 0)
		{
			Logger::red("Failed to send CGI response: " + std::string(strerror(errno)));
		}

		close(pipefd[0]);
		int status;
		waitpid(pid, &status, 0);
	}
}

void RequestHandler::handle_request(int client_fd, const ServerBlock& config,
									std::set<int>& activeFds,
									std::map<int, const ServerBlock*>& serverBlockConfigs)
{
	char buffer[4096];
	std::memset(buffer, 0, sizeof(buffer));

	int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes_read < 0)
	{
		Logger::red("Error reading request: " + std::string(strerror(errno)));
		close(client_fd);
		activeFds.erase(client_fd);
		serverBlockConfigs.erase(client_fd);
		return;
	}

	std::string request(buffer);
	std::string requestedPath = "/";
	size_t start = request.find("GET ") + 4;
	size_t end = request.find(" HTTP/1.1");
	if (start != std::string::npos && end != std::string::npos)
	{
		requestedPath = request.substr(start, end - start);
	}

	const Location* location = nullptr;
	for (const auto& loc : config.locations)
	{
		if (requestedPath.find(loc.path) == 0)
		{
			location = &loc;
			break;
		}
	}

	if (!location)
	{
		Logger::red("No matching location found for path: " + requestedPath);
		sendErrorResponse(client_fd, 404, "Not Found");
		return;
	}

	std::string root = location->root.empty() ? config.root : location->root;
	std::string filePath = root + requestedPath;

	DIR* dir = opendir(filePath.c_str());
	if (dir != nullptr) {
		closedir(dir);

		std::string indexFiles = config.index;
		if (indexFiles.empty()) {
			sendErrorResponse(client_fd, 403, "Forbidden");
			activeFds.erase(client_fd);
			serverBlockConfigs.erase(client_fd);
			close(client_fd);
			return;
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
			activeFds.erase(client_fd);
			serverBlockConfigs.erase(client_fd);
			close(client_fd);
			return;
		}
	}

	if (!location->cgi.empty())
	{
		std::string ext = getFileExtension(filePath);
		if (filePath.size() >= ext.size() &&
			filePath.compare(filePath.size() - ext.size(), ext.size(), ext) == 0)
		{
			std::map<std::string, std::string> cgiParams;
			cgiParams["SCRIPT_FILENAME"] = filePath;
			cgiParams["DOCUMENT_ROOT"] = root;
			cgiParams["QUERY_STRING"] = "";
			cgiParams["REQUEST_METHOD"] = "GET";
			cgiParams["SERVER_PROTOCOL"] = "HTTP/1.1";
			cgiParams["GATEWAY_INTERFACE"] = "CGI/1.1";
			cgiParams["SERVER_SOFTWARE"] = "my-cpp-server/1.0";
			cgiParams["REDIRECT_STATUS"] = "200";
			cgiParams["SERVER_NAME"] = "localhost";
			cgiParams["SERVER_PORT"] = "8001";
			cgiParams["REMOTE_ADDR"] = "127.0.0.1";
			cgiParams["REQUEST_URI"] = requestedPath;
			cgiParams["SCRIPT_NAME"] = requestedPath;
			executeCGI(location->cgi, filePath, cgiParams, client_fd);

			activeFds.erase(client_fd);
			serverBlockConfigs.erase(client_fd);
			close(client_fd);
			return;
		}
	}

	std::ifstream file(filePath);
	if (file.is_open())
	{
		std::stringstream fileContent;
		fileContent << file.rdbuf();
		std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
		response += fileContent.str();
		int sent = send(client_fd, response.c_str(), response.size(), 0);
		if (sent < 0)
		{
			Logger::red("Error sending static file: " + std::string(strerror(errno)));
		}
	}
	else
	{
		Logger::red("Failed to open static file: " + filePath);
		sendErrorResponse(client_fd, 404, "File Not Found");
	}

	activeFds.erase(client_fd);
	serverBlockConfigs.erase(client_fd);
	close(client_fd);
}