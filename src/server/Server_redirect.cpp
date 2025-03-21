#include "Server.hpp"

// Handles HTTP redirection by constructing a redirect response with a given status code and location
bool Server::redirectAction(Context& ctx)
{
	int return_code = 301;
	std::string return_url = "/";

	// Retrieve redirection details from context
	if (!ctx.location.return_code.empty())
		return_code = std::stoi(ctx.location.return_code);
	if (!ctx.location.return_url.empty())
		return_url = ctx.location.return_url;

	// Construct redirect response
	std::stringstream response;
	response << "HTTP/1.1 " << return_code << " Moved Permanently\r\n"
			 << "Location: " << return_url << "\r\n"
			 << "Connection: " << (ctx.keepAlive ? "keep-alive" : "close") << "\r\n";

	// Set cookies if necessary
	for (const auto& cookiePair : ctx.setCookies)
	{
		Cookie cookie;
		cookie.name = cookiePair.first;
		cookie.value = cookiePair.second;
		cookie.path = "/";
		response << generateSetCookieHeader(cookie) << "\r\n";
	}

	response << "Content-Length: 0\r\n\r\n";

	// Send response
	if (!sendHandler(ctx, response.str()))
	{
		return Logger::error("Failed to send redirect response");
	}

	// Keep connection alive if needed
	if (ctx.keepAlive)
	{
		resetContext(ctx);
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
	}

	return true;
}

