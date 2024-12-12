/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:17 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/12 12:19:37 by jeberle          ###   ########.fr       */
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
) : client_fd(_client_fd), config(_config), activeFds(_activeFds), serverBlockConfigs(_serverBlockConfigs) {}

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

bool RequestHandler::parseRequestLine(const std::string& request, std::string& method, std::string& requestedPath, std::string& version) {
    std::istringstream requestStream(request);
    std::string requestLine;
    if (!std::getline(requestStream, requestLine)) return false;
    if (!requestLine.empty() && requestLine.back() == '\r') requestLine.pop_back();

    std::istringstream lineStream(requestLine);
    lineStream >> method >> requestedPath >> version;
    return !(method.empty() || requestedPath.empty() || version.empty());
}

void RequestHandler::parseHeaders(std::istringstream& requestStream, std::map<std::string, std::string>& headersMap) {
    std::string headerLine;
    while (std::getline(requestStream, headerLine) && headerLine != "\r") {
        if (!headerLine.empty() && headerLine.back() == '\r') headerLine.pop_back();
        size_t colonPos = headerLine.find(":");
        if (colonPos != std::string::npos) {
            std::string key = headerLine.substr(0, colonPos);
            std::string value = headerLine.substr(colonPos + 1);
            while (!value.empty() && (value.front() == ' ' || value.front() == '\t')) value.erase(value.begin());
            headersMap[key] = value;
        }
    }
}

std::string RequestHandler::extractRequestBody(std::istringstream& requestStream) {
    std::string requestBody;
    std::string remaining;
    while (std::getline(requestStream, remaining)) requestBody += remaining + "\n";
    if (!requestBody.empty() && requestBody.back() == '\n') requestBody.pop_back();
    return requestBody;
}

const Location* RequestHandler::findLocation(const std::string& requestedPath) {
    for (std::vector<Location>::const_iterator it = config.locations.begin(); it != config.locations.end(); ++it) {
        if (requestedPath.find(it->path) == 0) {
            return &(*it);
        }
    }
    return nullptr;
}

void RequestHandler::handleStaticFile(const std::string& filePath) {
    std::ifstream file(filePath, std::ios::binary);
    if (file.is_open()) {
        std::ostringstream fileContent;
        fileContent << file.rdbuf();
        std::string response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n" + fileContent.str();
        send(client_fd, response.c_str(), response.size(), 0);
    } else {
        sendErrorResponse(404, "File Not Found", config.error_pages);
    }

    closeConnection();
}

void RequestHandler::handle_request() {
    char buffer[4096];
    std::memset(buffer, 0, sizeof(buffer));

    int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, MSG_DONTWAIT);
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

    std::istringstream requestStream(request);
    std::string dummyLine;
    std::getline(requestStream, dummyLine);
    std::map<std::string, std::string> headersMap;
    parseHeaders(requestStream, headersMap);

    std::string requestBody;
    if (method == "POST") requestBody = extractRequestBody(requestStream);

    const Location* location = findLocation(requestedPath);
    if (!location) {
        sendErrorResponse(404, "Not Found", config.error_pages);
        closeConnection();
        return;
    }

    std::string root = location->root.empty() ? config.root : location->root;
    std::string filePath = root + requestedPath;

    handleStaticFile(filePath);
}

void RequestHandler::closeConnection() {
    activeFds.erase(client_fd);
    serverBlockConfigs.erase(client_fd);
    close(client_fd);
}