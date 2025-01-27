#ifndef ERRORHANDLER_HPP
#define ERRORHANDLER_HPP

#include "../main.hpp"

class Server;

class ErrorHandler
{
public:
	ErrorHandler(Server& server);
	~ErrorHandler();

	std::string generateErrorResponse(Context& ctx) const;

private:
	Server& server;
	std::string loadErrorPage(const std::string& filePath) const;
};

#endif // ERRORHANDLER_HPP
