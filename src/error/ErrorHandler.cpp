/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ErrorHandler.cpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 07:09:22 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/18 10:42:26 by fkeitel          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "ErrorHandler.hpp"

#include "ErrorHandler.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

// Constructor with inline initialization of default error pages
ErrorHandler::ErrorHandler(Server& _server)
    : server(_server) // Initialize the Server reference
{}

ErrorHandler::~ErrorHandler() {}

std::string ErrorHandler::loadErrorPage(const std::string& filePath) const
{
    std::ifstream errorFile(filePath);
    if (errorFile.is_open())
    {
        std::stringstream buffer;
        buffer << errorFile.rdbuf();
        return buffer.str();
    }
    return "";
}

std::string ErrorHandler::generateErrorResponse(int statusCode, const std::string& message, RequestState &req) const
{
    auto it = req.associated_conf->errorPages.find(statusCode);
    std::string content;

    if (it != req.associated_conf->errorPages.end())
    {
        content = loadErrorPage(it->second);
        if (content.empty())
        {
            std::cerr << "Failed to load custom error page: " << it->second << std::endl;
        }
    }

    if (content.empty())
    {
        content = "<html><body><h1>" + std::to_string(statusCode) + " " + message + "</h1></body></html>";
    }

    std::ostringstream response;
    response << "HTTP/1.1 " << statusCode << " " << message << "\r\n"
		<< "Content-Type: text/html\r\n"
		<< "Content-Length: " << content.size() << "\r\n\r\n"
		<< content;

    return response.str();
}
