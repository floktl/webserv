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
			"font-family: Arial, Helvetica, sans-serif;"
			"color: white;"
			"text-align: center;"
			"margin: 0;"
			"padding: 0;"
			"height: 100vh;"
			"display: flex;"
			"user-select: none;"
			"justify-content: center;"
			"align-items: center;"
			"position: relative;"
			"overflow: hidden;"
			"}"
			"body::before {"
			"content: '';"
			"position: absolute;"
			"top: 0;"
			"left: 0;"
			"width: 100%;"
			"height: 100%;"
			"background-color: rgb(168, 0, 0);"
			"z-index: -2;"
			"}"
			"body::after {"
			"content: '';"
			"position: absolute;"
			"top: 0;"
			"left: 0;"
			"width: 100%;"
			"height: 100%;"
			"background-image: radial-gradient(circle at 30% 40%, rgb(255, 0, 0) 0%, transparent 30%),"
			"radial-gradient(circle at 70% 60%, rgb(255, 0, 0) 0%, transparent 30%),"
			"radial-gradient(circle at 50% 70%, rgb(255, 0, 0) 0%, transparent 20%),"
			"radial-gradient(circle at 20% 20%, rgb(255, 0, 0) 0%, transparent 25%),"
			"radial-gradient(circle at 80% 30%, rgb(255, 0, 0) 0%, transparent 15%);"
			"z-index: -1;"
			"animation: strobo 0.5s infinite alternate, randomPattern 2s infinite linear;"
			"}"
			"@keyframes strobo {"
			"0% { opacity: 0.4; }"
			"20% { opacity: 0.8; }"
			"40% { opacity: 0.2; }"
			"60% { opacity: 0.9; }"
			"80% { opacity: 0.3; }"
			"100% { opacity: 1; }"
			"}"
			"@keyframes randomPattern {"
			"0% { background-position: 0% 0%; }"
			"20% { background-position: 20% 10%; }"
			"40% { background-position: -15% 20%; }"
			"60% { background-position: 5% -10%; }"
			"80% { background-position: -5% 15%; }"
			"100% { background-position: 10% 5%; }"
			"}"
			"h1 {"
			"font-size: 2rem;"
			"text-shadow: 0 0 10px rgba(0, 0, 0, 0.7);"
			"}"
			"main {"
			"background-color: rgba(0, 0, 0, 0.5);"
			"padding: 2rem;"
			"border-radius: 10px;"
			"box-shadow: 0 0 20px rgba(0, 0, 0, 0.5);"
			"animation: pulse 2s infinite;"
			"}"
			"@keyframes pulse {"
			"0% { transform: scale(1); }"
			"50% { transform: scale(1.03); }"
			"100% { transform: scale(1); }"
			"}"
			"svg {"
			"filter: drop-shadow(0 0 5px rgba(255, 255, 255, 0.7));"
			"}"
			"</style>"
			"</head>"
			"<body>"
			"<main>"
			"<svg width=\"200\" xmlns=\"http://www.w3.org/2000/svg\" shape-rendering=\"geometricPrecision\" text-rendering=\"geometricPrecision\" image-rendering=\"optimizeQuality\" fill-rule=\"evenodd\" clip-rule=\"evenodd\" viewBox=\"0 0 512 431.15\"><g fill-rule=\"nonzero\"><path fill=\"#D92D27\" d=\"M492.56 431.15H19.42v-.05c-14.72.03-24.33-16.08-16.78-29.12L234.51 9.64c7.35-12.57 25.75-13.05 33.42-.23l240.41 390.96c9.12 12.64.2 30.78-15.78 30.78z\"/><path fill=\"#fff\" d=\"M41.15 399.63h431.88L252.71 47.08c-.63-.99-1.38-.98-1.97 0L40.02 397.9c-.63 1.12-.31 1.74 1.13 1.73z\"/><path d=\"M243.18 246.58h25.46v20.32h-25.46v-20.32zm-78.9 102.8h-17.43v7.87h21.35v12.7h-38.78v-54.47h38.35l-2.18 12.69h-18.74v8.58h17.43v12.63zm58.16 20.57h-19.16l-7.15-16.21h-5v16.21h-16.18v-54.47h27.45c12.49 0 18.74 6.36 18.74 19.08 0 8.72-2.7 14.47-8.11 17.26l9.41 18.13zm-31.31-41.78v12.15h5.26c2.09 0 3.62-.23 4.57-.66.95-.43 1.44-1.44 1.44-3v-4.82c0-1.57-.49-2.58-1.44-3.01-.95-.43-2.49-.66-4.57-.66h-5.26zm84.25 41.78h-19.17l-7.15-16.21h-4.99v16.21h-16.18v-54.47h27.45c12.49 0 18.74 6.36 18.74 19.08 0 8.72-2.7 14.47-8.11 17.26l9.41 18.13zm-31.31-41.78v12.15h5.25c2.1 0 3.63-.23 4.58-.66.95-.43 1.44-1.44 1.44-3v-4.82c0-1.57-.49-2.58-1.44-3.01-.95-.43-2.5-.66-4.58-.66h-5.25zm34.14 14.59c0-9.94 1.86-17.18 5.58-21.74 3.71-4.57 10.43-6.85 20.13-6.85s16.41 2.28 20.13 6.85c3.72 4.56 5.58 11.8 5.58 21.74 0 4.94-.4 9.09-1.18 12.46-.78 3.37-2.14 6.3-4.05 8.8-1.92 2.5-4.57 4.33-7.93 5.49-3.37 1.16-7.55 1.74-12.55 1.74-4.99 0-9.18-.58-12.55-1.74-3.37-1.16-6.01-2.99-7.93-5.49s-3.27-5.43-4.05-8.8c-.79-3.37-1.18-7.52-1.18-12.46zm18.74-9.07v22.66h7.23c2.38 0 4.11-.27 5.19-.83 1.07-.55 1.61-1.81 1.61-3.79v-22.66h-7.33c-2.32 0-4.02.28-5.09.83-1.07.55-1.61 1.81-1.61 3.79zm85.62 36.26H363.4l-7.15-16.21h-4.99v16.21h-16.18v-54.47h27.44c12.5 0 18.75 6.36 18.75 19.08 0 8.72-2.71 14.47-8.11 17.26l9.41 18.13zm-31.31-41.78v12.15h5.25c2.1 0 3.63-.23 4.58-.66.95-.43 1.43-1.44 1.43-3v-4.82c0-1.57-.48-2.58-1.43-3.01-.95-.43-2.5-.66-4.58-.66h-5.25zm-82.62-93h-25.46c-1.82-22.29-4.73-26.26-5.95-40.78-1.25-14.61-2.12-35.58 18.73-35.58 21.86 0 20.1 23.36 18.45 38.47-1.37 12.45-4.05 17.16-5.77 37.89z\"/></g></svg>"
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