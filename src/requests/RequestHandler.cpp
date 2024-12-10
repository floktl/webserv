/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:17 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/10 13:26:41 by fkeitel          ###   ########.fr       */
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

#include <sys/select.h> // For select

void executeCGI(const std::string& cgiPath, const std::string& scriptPath,
                const std::map<std::string, std::string>& cgiParams, int client_fd,
                const std::string& requestBody, const std::string& method)
{
    int pipefd_out[2]; // For CGI stdout
    int pipefd_in[2];  // For CGI stdin
    if (pipe(pipefd_out) == -1)
    {
        Logger::red("Pipe creation failed (out): " + std::string(strerror(errno)));
        return;
    }
    if (pipe(pipefd_in) == -1)
    {
        Logger::red("Pipe creation failed (in): " + std::string(strerror(errno)));
        close(pipefd_out[0]);
        close(pipefd_out[1]);
        return;
    }

    pid_t pid = fork();
    if (pid < 0)
    {
        Logger::red("Fork failed: " + std::string(strerror(errno)));
        close(pipefd_out[0]);
        close(pipefd_out[1]);
        close(pipefd_in[0]);
        close(pipefd_in[1]);
        return;
    }

    if (pid == 0)
    {
        // Child
        dup2(pipefd_out[1], STDOUT_FILENO);
        close(pipefd_out[0]);
        close(pipefd_out[1]);

        dup2(pipefd_in[0], STDIN_FILENO);
        close(pipefd_in[0]);
        close(pipefd_in[1]);

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
        // Parent
        close(pipefd_out[1]);
        close(pipefd_in[0]);

        // Write request body to CGI if POST
        if (method == "POST" && !requestBody.empty())
        {
            ssize_t written = write(pipefd_in[1], requestBody.c_str(), requestBody.size());
            if (written < 0)
                Logger::red("Error writing to CGI stdin: " + std::string(strerror(errno)));
        }
        close(pipefd_in[1]);

        // Read CGI output with timeout
        std::string cgi_output;
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
				close(client_fd);
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

        // Process CGI output (same as before)
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
            body = cgi_output;
            headers = "Content-Type: text/html; charset=UTF-8";
        }

        // Check for Redirect (302 Found)
        if (headers.find("Status: 302") != std::string::npos)
        {
            size_t location_pos = headers.find("Location:");
            if (location_pos != std::string::npos)
            {
                size_t end_pos = headers.find("\r\n", location_pos);
                std::string location = headers.substr(location_pos + 9, end_pos - location_pos - 9);
                location.erase(0, location.find_first_not_of(" \t"));
                location.erase(location.find_last_not_of(" \t") + 1);

                std::string redirect_response = "HTTP/1.1 302 Found\r\n";
                redirect_response += "Location: " + location + "\r\n\r\n";
                send(client_fd, redirect_response.c_str(), redirect_response.size(), 0);
                close(pipefd_out[0]);
                close(client_fd);
                return;
            }
        }

        std::string response = "HTTP/1.1 200 OK\r\n" + headers + "\r\n\r\n" + body;
        int sent = send(client_fd, response.c_str(), response.length(), 0);
        if (sent < 0)
        {
            Logger::red("Failed to send CGI response: " + std::string(strerror(errno)));
        }

        close(pipefd_out[0]);
        int status;
        waitpid(pid, &status, 0);
    }
}

void sendRedirectResponse(int client_fd, const std::string& redirectHost, int port)
{
    std::ostringstream response;
    response << "HTTP/1.1 301 Moved Permanently\r\n"
        << "Location: http://" << redirectHost << ":" << port << "/\r\n\r\n";

    int sent = send(client_fd, response.str().c_str(), response.str().size(), 0);
    if (sent < 0)
    {
        const char* error_message = strerror(errno);
        write(STDERR_FILENO, "Error sending redirect response: ", 33);
        write(STDERR_FILENO, error_message, strlen(error_message));
        write(STDERR_FILENO, "\n", 1);
    }
}

bool validateHostHeader(int client_fd, const std::string& request, const ServerBlock& config,
                        std::set<int>& activeFds, std::map<int, const ServerBlock*>& serverBlockConfigs)
{
    std::istringstream requestStream(request);
    std::string headers;
    std::string hostHeader;

    // Extract Host header
    while (std::getline(requestStream, headers) && headers != "\r")
    {
        if (headers.substr(0, 5) == "Host:")
        {
            hostHeader = headers.substr(5);
            hostHeader.erase(0, hostHeader.find_first_not_of(" \t")); // Trim leading spaces
            hostHeader.erase(hostHeader.find_last_not_of(" \t") + 1); // Trim trailing spaces
            break;
        }
    }

    if (hostHeader.empty())
    {
        Logger::red("Missing Host header");
        sendErrorResponse(client_fd, 400, "Bad Request - Missing Host Header");
        close(client_fd);
        activeFds.erase(client_fd);
        serverBlockConfigs.erase(client_fd);
        return false;
    }

	auto trim = [](std::string& str) {
		str.erase(0, str.find_first_not_of(" \t\r\n")); // Trim leading whitespace
		str.erase(str.find_last_not_of(" \t\r\n") + 1); // Trim trailing whitespace
	};

	// Create modifiable copies of hostHeader and config.name
	std::string trimmedHostHeader = hostHeader;
	std::string trimmedConfigName = config.name;

	trim(trimmedHostHeader);
	trim(trimmedConfigName);

	if (trimmedHostHeader != trimmedConfigName && trimmedHostHeader != "localhost")
	{
		Logger::red("Host mismatch: Received Host: " + trimmedHostHeader);
		Logger::red("Expected Host: " + trimmedConfigName);

		std::ostringstream redirectResponse;
		redirectResponse << "HTTP/1.1 301 Moved Permanently\r\n"
						<< "Location: http://" << trimmedConfigName << ":8080/\r\n\r\n";

		send(client_fd, redirectResponse.str().c_str(), redirectResponse.str().size(), 0);
		close(client_fd);
		activeFds.erase(client_fd);
		serverBlockConfigs.erase(client_fd);
		return false;
	}

    return true;
}

bool waitForRequest(int client_fd, int timeout_sec,
                    std::set<int>& activeFds,
                    std::map<int, const ServerBlock*>& serverBlockConfigs)
{
    // Define timeout duration
    struct timeval timeout;
    timeout.tv_sec = timeout_sec;  // Timeout in seconds
    timeout.tv_usec = 0;           // 0 microseconds

    fd_set readfds;
    FD_ZERO(&readfds);             // Clear the set
    FD_SET(client_fd, &readfds);   // Add client_fd to the set

    // Wait for the socket to become readable within the timeout
    int activity = select(client_fd + 1, &readfds, NULL, NULL, &timeout);
    if (activity == -1)
    {
        Logger::red("Error in select: " + std::string(strerror(errno)));
        close(client_fd);
        activeFds.erase(client_fd);
        serverBlockConfigs.erase(client_fd);
        return false; // Indicate failure
    }
    else if (activity == 0)
    {
        Logger::red("Request timed out for client_fd: " + std::to_string(client_fd));
        sendErrorResponse(client_fd, 408, "Request Timeout"); // HTTP 408 Request Timeout
        close(client_fd);
        activeFds.erase(client_fd);
        serverBlockConfigs.erase(client_fd);
        return false; // Indicate timeout
    }
    return true; // Socket is readable
}


void RequestHandler::handle_request(int client_fd, const ServerBlock& config,
									std::set<int>& activeFds,
									std::map<int, const ServerBlock*>& serverBlockConfigs)
{
	char buffer[4096];
	std::memset(buffer, 0, sizeof(buffer));

	if (!waitForRequest(client_fd, 5, activeFds, serverBlockConfigs))
	{
		// Timeout or error occurred, request already handled by waitForRequest
		return;
	}
	int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes_read < 0)
	{
		Logger::red("Error reading request: " + std::string(strerror(errno)));
		close(client_fd);
		activeFds.erase(client_fd);
		serverBlockConfigs.erase(client_fd);
		return;
	}
	Logger::red(buffer);
	std::string request(buffer, bytes_read);

	// Validate Host header
    //if (!validateHostHeader(client_fd, request, config, activeFds, serverBlockConfigs))
    //{
    //    // Host validation failed; request already handled
    //    return;
    //}
	// Parse Request line
	std::istringstream requestStream(request);
	std::string requestLine;
	if (!std::getline(requestStream, requestLine))
	{
		sendErrorResponse(client_fd, 400, "Bad Request");
		close(client_fd);
		activeFds.erase(client_fd);
		serverBlockConfigs.erase(client_fd);
		return;
	}
	if (!requestLine.empty() && requestLine.back() == '\r')
		requestLine.pop_back();

	std::string method;
	std::string requestedPath;
	std::string version;

	{
		std::istringstream lineStream(requestLine);
		lineStream >> method >> requestedPath >> version;
	}

	if (method.empty() || requestedPath.empty() || version.empty())
	{
		sendErrorResponse(client_fd, 400, "Bad Request");
		close(client_fd);
		activeFds.erase(client_fd);
		serverBlockConfigs.erase(client_fd);
		return;
	}

	// Extract headers
	std::map<std::string, std::string> headers;
	std::string headerLine;
	while (std::getline(requestStream, headerLine) && headerLine != "\r")
	{
		if (!headerLine.empty() && headerLine.back() == '\r')
			headerLine.pop_back();
		size_t colonPos = headerLine.find(":");
		if (colonPos != std::string::npos)
		{
			std::string key = headerLine.substr(0, colonPos);
			std::string value = headerLine.substr(colonPos + 1);
			// trim
			while (!value.empty() && (value.front() == ' ' || value.front() == '\t'))
				value.erase(value.begin());
			headers[key] = value;
		}
	}

	// Extract body if POST
	std::string requestBody;
	if (method == "POST")
	{
		// Read the rest into body
		std::string remaining;
		while (std::getline(requestStream, remaining))
			requestBody += remaining + "\n";

		// Remove extra trailing newline
		if (!requestBody.empty() && requestBody.back() == '\n')
			requestBody.pop_back();
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
		activeFds.erase(client_fd);
		serverBlockConfigs.erase(client_fd);
		close(client_fd);
		return;
	}

	std::string root = location->root.empty() ? config.root : location->root;
	std::string filePath = root + requestedPath;

	// DELETE Method Handling
	if (method == "DELETE")
	{
		// Versuch die Datei zu löschen
		if (remove(filePath.c_str()) == 0)
		{
			// Erfolgreich gelöscht
			std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h1>File Deleted</h1></body></html>";
			send(client_fd, response.c_str(), response.size(), 0);
		}
		else
		{
			// Nicht gefunden oder Fehler
			sendErrorResponse(client_fd, 404, "File Not Found");
		}
		activeFds.erase(client_fd);
		serverBlockConfigs.erase(client_fd);
		close(client_fd);
		Logger::green("test 1");
		return;
	}
	// Directory handling for GET/POST
	DIR* dir = opendir(filePath.c_str());
	if (dir != nullptr) {
		closedir(dir);

		std::string indexFiles = config.index;
		if (indexFiles.empty()) {
			sendErrorResponse(client_fd, 403, "Forbidden");
			activeFds.erase(client_fd);
			serverBlockConfigs.erase(client_fd);
			close(client_fd);
			Logger::green("test 2");
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
			Logger::green("test 3");
			return;
		}
	}

	// CGI Handling for GET/POST
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

			// Content-Length for POST if available
			if (method == "POST") {
				std::map<std::string, std::string>::const_iterator it = headers.find("Content-Length");
				if (it != headers.end()) {
					cgiParams["CONTENT_LENGTH"] = it->second;
				}
				// Content-Type for POST if available
				it = headers.find("Content-Type");
				if (it != headers.end()) {
					cgiParams["CONTENT_TYPE"] = it->second;
				}
			}
			Logger::green("test 4");

			executeCGI(location->cgi, filePath, cgiParams, client_fd, requestBody, method);

			Logger::green("test 5");
			activeFds.erase(client_fd);
			serverBlockConfigs.erase(client_fd);
			close(client_fd);
			return;
		}
	}

	// Statisches File Handling für GET & POST (POST hier ggf. statische Resources liefern)
	if (method == "GET" || method == "POST")
	{
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
	}
	Logger::green("test 5");
	activeFds.erase(client_fd);
	serverBlockConfigs.erase(client_fd);
	close(client_fd);
}
