#include "ErrorHandler.hpp"

#include "ErrorHandler.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

// Constructor with inline initialization of default error pages
ErrorHandler::ErrorHandler(Server& _server)
	: server(_server)
{}

// Destructor
ErrorHandler::~ErrorHandler() {}

// Reads and returns the content of an error page file if found, otherwise returns an empty string
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

// Generates an HTTP error response based on the error code and sends it via the server
bool ErrorHandler::generateErrorResponse(Context& ctx) const
{
	std::ostringstream response;
	std::string content;

	std::map<int, std::string>::iterator it = ctx.errorPages.find(ctx.error_code);
	if (it != ctx.errorPages.end())
		content = loadErrorPage(it->second);
	else
		content = "Default error page content for status " + std::to_string(ctx.error_code);
	if (content.empty())
		content = "<!DOCTYPE html><html><head><style>body { background-color: #940000; color: white; font-family: Arial, Helvetica, sans-serif; margin: 0; padding: 0; height: 100vh; display: flex; justify-content: center; align-items: center;} h1 {font-size: 2rem;text-align: center;}</style></head><body><h1>"
					+ std::to_string(ctx.error_code) + " " + ctx.error_message + "</h1></body></html>";
	response << "HTTP/1.1 " << ctx.error_code << " " << ctx.error_message << "\r\n"
			<< "Content-Type: text/html\r\n"
			<< "Content-Length: " << content.size() << "\r\n\r\n"
			<< content;

	Logger::errorLog("ErrorHandler: Errorcode: " + std::to_string(ctx.error_code) + " " + ctx.error_message);
	return (server.sendHandler(ctx, response.str()));
}

// Updates the context with an error type, error code, and error message, returning false
bool	updateErrorStatus(Context &ctx, int error_code, std::string error_string)
{
	ctx.type = ERROR;
	ctx.error_code = error_code;
	ctx.error_message = error_string;
	return false;
}