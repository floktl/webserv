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

std::string ErrorHandler::generateErrorResponse(Context& ctx) const
{
	std::ostringstream response;
	std::string content;

	//Logger::file("Generating error response for error code: " + std::to_string(ctx.error_code));

	// Find the custom error page if it exists
	//Logger::file("Searching for custom error page in errorPages map...");
	std::map<int, std::string>::iterator it = ctx.errorPages.find(ctx.error_code);

	if (it != ctx.errorPages.end())
	{
		//Logger::file("Custom error page found for error code " + std::to_string(ctx.error_code) + ": " + it->second);
		content = loadErrorPage(it->second);
		if (content.empty())
		{
			//Logger::file("Failed to load custom error page file: " + it->second);
		}
		else
		{
			//Logger::file("Successfully loaded custom error page.");
		}
	}
	else
	{
		//Logger::file("No custom error page found for status code " + std::to_string(ctx.error_code));
		content = "Default error page content for status " + std::to_string(ctx.error_code);
		//Logger::file("Using default error page content.");
	}

	// Fallback to default error page if content is still empty
	if (content.empty())
	{
		//Logger::file("Content is still empty after attempting to load custom page. Falling back to generic HTML error response.");
		content = "<!DOCTYPE html><html><head><style>body { background-color: #940000; color: white; font-family: Arial, Helvetica, sans-serif; margin: 0; padding: 0; height: 100vh; display: flex; justify-content: center; align-items: center;} h1 {font-size: 2rem;text-align: center;}</style></head><body><h1>" + std::to_string(ctx.error_code) + " " + ctx.error_message + "</h1></body></html>";
	}

	//Logger::file("Preparing HTTP response headers...");
	response << "HTTP/1.1 " << ctx.error_code << " " << ctx.error_message << "\r\n"
			<< "Content-Type: text/html\r\n"
			<< "Content-Length: " << content.size() << "\r\n\r\n"
			<< content;

	//Logger::file("Response successfully generated with length: " + std::to_string(content.size()));
	//Logger::file("Final response string: \n" + response.str());

	return response.str();
}

