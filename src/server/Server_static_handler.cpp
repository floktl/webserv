#include "Server.hpp"


bool Server::staticHandler(Context& ctx)
{
	if (ctx.method == "POST")
	{
		return (true);
	}
	else if (ctx.method == "GET")
	{
		if (ctx.is_download && ctx.multipart_fd_up_down > 0)
			return buildDownloadResponse(ctx);

		if (!ctx.do_autoindex.empty())
		{
			std::stringstream ss;
			buildAutoIndexResponse(ctx, &ss);
			if (ctx.type == ERROR)
				return (false);
			std::string response = ss.str();
			resetContext(ctx);
			return sendHandler(ctx, response);
		}

		else
		{
			buildStaticResponse(ctx);
			if (ctx.type == ERROR)
				return (false);
			if (!ctx.keepAlive)
				return (false);
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
	//Logger::magenta("send sendHandler");
	ssize_t bytes_sent = send(ctx.client_fd, http_response.c_str(), http_response.size(), MSG_NOSIGNAL);
	if (bytes_sent < 0)
	{
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
		return false;
	}
	if (bytes_sent >= 0 && (ctx.type == ERROR || !ctx.keepAlive))
	{
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
		globalFDS.context_map.erase(ctx.client_fd);
	}

	return true;
}
