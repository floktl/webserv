/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ErrorHandler.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 07:09:22 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/13 07:53:48 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./ErrorHandler.hpp"
#include <sstream>
#include <fstream>
#include <iostream>
#include <sys/socket.h>
#include <unistd.h>
#include "./../utils/Logger.hpp"

ErrorHandler::ErrorHandler(int _client_fd, const std::map<int, std::string>& _errorPages)
	: client_fd(_client_fd), errorPages(_errorPages) {}

void ErrorHandler::sendErrorResponse(int statusCode, const std::string& message) {
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
