/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ClientHandler.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:17 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/02 12:28:21 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ClientHandler.hpp"

void ClientHandler::handle_client(int client_fd, const FileConfData& config,
		int kq, std::set<int>& activeFds,
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

	// Remove the client FD from kqueue
	struct kevent ev_remove;
	EV_SET(&ev_remove, client_fd, EVFILT_READ, EV_DELETE, 0, 0, nullptr);
	if (kevent(kq, &ev_remove, 1, nullptr, 0, nullptr) < 0)
	{
		perror("kevent (EV_DELETE)");
	}
	// Clean up FD
	activeFds.erase(client_fd);
	clientConfigMap.erase(client_fd);
	close(client_fd);
}
