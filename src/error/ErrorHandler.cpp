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
		Logger::red("code:");
			// Ensure associated_conf is not null
		if (req.associated_conf == nullptr) {
			return "";
		}

		// Look for the error page for the given status code
		auto it = req.associated_conf->errorPages.find(statusCode);
		std::string content;

		if (it != req.associated_conf->errorPages.end())
		{
			content = loadErrorPage(it->second);
			if (content.empty())
			{
				std::cerr << "Failed to load custom error page: " << it->second << std::endl;
			}
		} else {
			std::cerr << "Error: Status code " << statusCode << " not found in errorPages" << std::endl;
			content = "Default error page content for status " + std::to_string(statusCode);
		}

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
