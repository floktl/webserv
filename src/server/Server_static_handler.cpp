#include "Server.hpp"

// The most important changes:

// Do not set cookies in auto index because it is a technical directory list
// Set cookies in BuildSTATICRESPONSE at normal gets for files
// Set Cookie in Redirect at Post/Delete after a successful campaign

// This is more consistent because:

// Auto index is a technical function and does not need cookies
// Normal pages can get cookies
// After actions (Post/Delete), the cookie is set in the redirect
bool Server::staticHandler(Context& ctx)
{
	if (ctx.method == "POST")
	{
		return (true);
	}
	else if (ctx.method == "GET")
	{
		std::string response;
		if (!ctx.doAutoIndex.empty())
		{
			std::stringstream ss;
			buildAutoIndexResponse(ctx, &ss);
			if (ctx.type == ERROR)
				return (false);
			response = ss.str();
			resetContext(ctx);
			return sendHandler(ctx, response);
		}
		else
		{
			buildStaticResponse(ctx);
			if (ctx.type == ERROR)
				return (false);
			if (!ctx.keepAlive)
// Modepoll (ctx.epoll_fd, ctx.client_fd, epollin | epollet);
			return (true);
		}
	}
	else if (ctx.method == "DELETE")
		return deleteHandler(ctx);
	return (false);
}

// Send to http response to the client and handles Connection Cleanup if necessary
bool Server::sendHandler(Context& ctx, std::string http_response)
{
	ssize_t bytes_sent = send(ctx.client_fd, http_response.c_str(), http_response.size(), MSG_NOSIGNAL);
	if (bytes_sent < 0)
	{
		Logger::errorLog("Error sending response to client_fd: " + std::to_string(ctx.client_fd));
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
		return false;
	}
	if (ctx.type == ERROR || !ctx.keepAlive)
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);

	return true;
}
