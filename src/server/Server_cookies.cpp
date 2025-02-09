#include "Server.hpp"

// Parses a new "Set-Cookie" header value and updates the request context
void Server::parseNewCookie(Context& ctx, std::string value)
{
	Logger::errorLog("parseNewCookie: " + value);
    ctx.setCookie = value;
}

// Parses existing "Cookie" header values and stores them in the request context
void Server::parseCookies(Context& ctx, std::string value)
{
	Logger::errorLog("parseCookies: " + value);
	ctx.cookies = value;
}

