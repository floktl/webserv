/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ClientHandler.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:17 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/02 14:34:37 by fkeitel          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ClientHandler.hpp"

#include <sys/epoll.h>   // For epoll functions
#include <cstring>       // For memset
#include <iostream>      // For cerr
#include <fstream>       // For file handling
#include <sstream>       // For stringstream

void ClientHandler::handle_client(int client_fd, const FileConfData& config,
        int epoll_fd, std::set<int>& activeFds,
        std::map<int, const FileConfData*>& clientConfigMap)
{
	char buffer[4096];
	memset(buffer, 0, sizeof(buffer));

	// Read the request from the client
	int bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0);
	if (bytes_read < 0)
	{
		perror("recv");
		close(client_fd);
		activeFds.erase(client_fd);
		clientConfigMap.erase(client_fd);
		return;
	}

	std::string request(buffer); // Convert the received data into a string
	std::string response;

	// Simple HTTP request parsing
	std::string requestedPath = "/";
	size_t start = request.find("GET ") + 4;
	size_t end = request.find(" HTTP/1.1");
	if (start != std::string::npos && end != std::string::npos)
	{
		requestedPath = request.substr(start, end - start);
	}

	std::string root = config.root;
	std::string indexFile = config.index;

	// Map the request to a file
	std::string absoluteRoot = getAbsolutePath(config.root);
	std::string filePath = absoluteRoot + (requestedPath == "/" ? "/" + indexFile : requestedPath);

	// Try to open the file
	std::ifstream file(filePath);

	if (file.is_open())
	{
		std::stringstream fileContent;
		fileContent << file.rdbuf(); // Read the file content
		response = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n";
		response += fileContent.str(); // Append file content to the response
	}
	else
	{
		// File not found, return 404
		std::cerr << "Failed to open file: " << filePath << std::endl;
		response = "HTTP/1.1 404 Not Found\r\nContent-Type: text/html\r\n\r\n";
		response += "<html><body><h1>404 Not Found</h1></body></html>";
	}

	// Send the response to the client
	send(client_fd, response.c_str(), response.size(), 0);

    // Remove the client FD from epoll
    if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client_fd, nullptr) < 0)
    {
        perror("epoll_ctl (EPOLL_CTL_DEL)");
    }

    // Clean up FD
    activeFds.erase(client_fd);
    clientConfigMap.erase(client_fd);
    close(client_fd);
}
