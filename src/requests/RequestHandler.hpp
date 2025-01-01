#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include "../main.hpp"

struct RequestState;

class CgiHandler;
class Server;

class RequestHandler {
	public:
		RequestHandler(Server& server);

		void buildResponse(RequestState &req);
		void parseRequest(RequestState &req);
		const Location* findMatchingLocation(const ServerBlock* conf, const std::string& path);
		void buildErrorResponse(int statusCode, const std::string& message, std::stringstream *response, RequestState &req);
	private:
		Server& server;
};

#endif
