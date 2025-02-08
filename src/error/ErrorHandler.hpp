#ifndef ERRORHANDLER_HPP
#define ERRORHANDLER_HPP

#include "../server/server.hpp"

class Server;

class ErrorHandler
{
public:
	ErrorHandler(Server& server);
	~ErrorHandler();

	bool generateErrorResponse(Context& ctx) const;

private:
	Server& server;
	std::string loadErrorPage(const std::string& filePath) const;
};

#endif // ERRORHANDLER_HPP
