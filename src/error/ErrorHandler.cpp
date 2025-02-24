#include "ErrorHandler.hpp"

#include "ErrorHandler.hpp"
#include <fstream>
#include <sstream>
#include <iostream>

// Constructor with Inline Initialization of Default Error Pages
ErrorHandler::ErrorHandler(Server& _server)
	: server(_server)
{}

// Destructor
ErrorHandler::~ErrorHandler() {}

// Reads and Returns the Content of An Error Page File IF Found, OtherWise Returns to Empty String
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

// Generates to http error response based on the error code and sends it via the server
bool ErrorHandler::generateErrorResponse(Context& ctx) const
{
	std::ostringstream response;
	std::string content;
	Logger::blue("generateErrorResponse");
	Logger::errorLog("ErrorHandler: Errorcode: " + std::to_string(ctx.error_code) + " " + ctx.error_message);

	std::map<int, std::string>::iterator it = ctx.errorPages.find(ctx.error_code);
	if (it != ctx.errorPages.end())
	{
		Logger::blue("loadErrorPage");
		content = loadErrorPage(it->second);
	}
	else
	{
		content = "<!DOCTYPE html>"
			"<html lang=\"en\">"
			"<head>"
			"<meta charset=\"UTF-8\">"
			"<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
			"<title>Webserv: Internal Server Error</title>"
			"<style>"
			"body {"
			"background-color: rgb(163, 163, 163);"
			"background-image: radial-gradient(circle at center, rgb(85, 0, 0) 0%, rgb(168, 0, 0));"
			"color: white;"
			"font-family: Arial, Helvetica, sans-serif;"
			"margin: 0;"
			"padding: 0;"
			"height: 100vh;"
			"display: flex;"
			"justify-content: center;"
			"align-items: center;"
			"}"
			"h1 {"
			"font-size: 2rem;"
			"}"
			"</style>"
			"</head>"
			"<body>"
			"<main>"
			"<h1>" + std::to_string(ctx.error_code) + ": " + ctx.error_message + "</h1>"
			"<p>Some problem on running the app occurred!</p>"
			"</main>"
			"</body>"
			"</html>";
	}

	response << "HTTP/1.1 " << ctx.error_code << " " << ctx.error_message << "\r\n"
			<< "Content-Type: text/html\r\n"
			<< "Content-Length: " << content.size() << "\r\n\r\n"
			<< content;

	return (server.sendHandler(ctx, response.str()));
}

// Updates the Context with an Error Type, Error Code, and Error Message, Returning False
bool	updateErrorStatus(Context &ctx, int error_code, std::string error_string)
{
	ctx.type = ERROR;
	ctx.error_code = error_code;
	ctx.error_message = error_string;
	return false;
}