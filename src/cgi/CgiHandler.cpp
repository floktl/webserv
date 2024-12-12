/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiHandler.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/11 14:42:00 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/12 09:00:09 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "CgiHandler.hpp"

CgiHandler::CgiHandler(
	int _client_fd,
	const Location* _location,
	const std::string& _filePath,
	const std::string& _method,
	const std::string& _requestedPath,
	const std::string& _requestBody,
	const std::map<std::string, std::string>& _headersMap,
	std::set<int> _activeFds,
	const std::map<int, const ServerBlock*>& _serverBlockConfigs
)
	: client_fd(_client_fd),
	location(_location),
	filePath(_filePath),
	method(_method),
	requestedPath(_requestedPath),
	requestBody(_requestBody),
	headersMap(_headersMap),
	activeFds(std::move(_activeFds))
{
	// Konvertiere _serverBlockConfigs (Pointer) in serverBlockConfigs (Werte)
	for (const auto& pair : _serverBlockConfigs) {
		if (pair.second) { // Prüfe, ob der Zeiger gültig ist
			serverBlockConfigs.emplace(pair.first, *(pair.second));
		}
	}
}

std::string CgiHandler::getFileExtension()
{
	size_t pos = filePath.find_last_of(".");
	if (pos == std::string::npos) {
		Logger::red("No file extension found, check to executable cgi target");
		return "";
	}
	std::string ext = filePath.substr(pos);
	return ext;
}

void CgiHandler::closePipe(int fds[2]) {
	close(fds[0]);
	close(fds[1]);
}

bool CgiHandler::createPipes(int pipefd_out[2], int pipefd_in[2]) {
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


void CgiHandler::setupChildEnv(const std::map<std::string, std::string>& cgiParams, std::vector<char*>& env) {
	for (const auto& param : cgiParams) {
		std::string envVar = param.first + "=" + param.second;
		env.push_back(strdup(envVar.c_str()));
	}
	env.push_back(nullptr);
}


void CgiHandler::runCgiChild(const std::string& cgiPath, const std::string& scriptPath,
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

void CgiHandler::writeRequestBodyIfNeeded(int pipe_in) {
	if (method == "POST" && !requestBody.empty()) {
		ssize_t written = write(pipe_in, requestBody.c_str(), requestBody.size());
		if (written < 0)
			Logger::red("Error writing to CGI stdin: " + std::string(strerror(errno)));
	}
	close(pipe_in);
}

std::string CgiHandler::readCgiOutput(int pipe_out) {
	std::string cgi_output;
	char buffer[4096];
	int bytes;
	while ((bytes = read(pipe_out, buffer, sizeof(buffer))) > 0) {
		cgi_output.append(buffer, bytes);
	}
	close(pipe_out);
	return cgi_output;
}

void CgiHandler::parseCgiOutput(std::string& headers, std::string& body, int pipefd_out[2], [[maybe_unused]] int pipefd_in[2], pid_t pid) {
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



bool CgiHandler::checkForRedirect(const std::string& headers) {
	if (headers.find("Status: 302") != std::string::npos) {
		size_t location_pos = headers.find("Location:");
		if (location_pos != std::string::npos) {
			size_t end_pos = headers.find("\r\n", location_pos);
			std::string redirectLocation = headers.substr(location_pos + 9, end_pos - location_pos - 9);
			redirectLocation.erase(0, redirectLocation.find_first_not_of(" \t"));
			redirectLocation.erase(redirectLocation.find_last_not_of(" \t") + 1);

			// Send Redirect Response
			std::string redirect_response = "HTTP/1.1 302 Found\r\n";
			redirect_response += "Location: " + redirectLocation + "\r\n\r\n";
			int sent = send(client_fd, redirect_response.c_str(), redirect_response.size(), 0);
			if (sent < 0) {
				Logger::red("Failed to send Redirect response: " + std::string(strerror(errno)));
			}
			return true;
		}
	}
	return false;
}

void CgiHandler::sendCgiResponse(const std::string& headers, const std::string& body) {
	std::string response = "HTTP/1.1 200 OK\r\n" + headers + "\r\n\r\n" + body;
	int sent = send(client_fd, response.c_str(), response.length(), 0);
	if (sent < 0) {
		Logger::red("Failed to send CGI response: " + std::string(strerror(errno)));
	}
}

void CgiHandler::waitForChild(pid_t pid) {
	int status;
	waitpid(pid, &status, 0);
}

void CgiHandler::executeCGI(const std::string& cgiPath, const std::string& scriptPath,
				const std::map<std::string, std::string>& cgiParams) {
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

		writeRequestBodyIfNeeded(pipefd_in[1]);

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

void CgiHandler::closeConnection()
{
	activeFds.erase(client_fd);
	serverBlockConfigs.erase(client_fd);
	close(client_fd);
}



bool CgiHandler::handleCGIIfNeeded()
{
	if (!location->cgi.empty()) {
		std::string ext = getFileExtension();
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
				std::map<std::string, std::string>::const_iterator it = headersMap.find("Content-Length");
				if (it != headersMap.end()) {
					cgiParams["CONTENT_LENGTH"] = it->second;
				}
				it = headersMap.find("Content-Type");
				if (it != headersMap.end()) {
					cgiParams["CONTENT_TYPE"] = it->second;
				}
			}
			Logger::green("test 4");

			executeCGI(location->cgi, filePath, cgiParams);
			closeConnection();
			return true;
		}
	}
	return false;
}