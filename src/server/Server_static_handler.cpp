#include "Server.hpp"

//Die wichtigsten Änderungen:

//Bei autoindex KEINE Cookies setzen, da es sich um eine technische Verzeichnisauflistung handelt
//Bei normalen GET Requests für Dateien Cookies in buildStaticResponse setzen
//Bei POST/DELETE nach erfolgreicher Aktion Cookie im Redirect setzen

//Das ist konsistenter, weil:

//Autoindex ist eine technische Funktion und braucht keine Cookies
//Normale Seiten können Cookies bekommen
//Nach Aktionen (POST/DELETE) wird das Cookie im Redirect gesetzt
bool Server::staticHandler(Context& ctx)
{
	Logger::errorLog("static " + ctx.method);
	if (ctx.method == "POST")
	{
		// if (!handleStaticUpload(ctx))
		// 	return (false);
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
				//modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
			return (true);
		}
	}
	else if (ctx.method == "DELETE")
		return deleteHandler(ctx);
	return (false);
}

// Sends an HTTP response to the client and handles connection cleanup if necessary
bool Server::sendHandler(Context& ctx, std::string http_response)
{
	ssize_t bytes_sent = send(ctx.client_fd, http_response.c_str(), http_response.size(), MSG_NOSIGNAL);
	if (bytes_sent < 0)
	{
		Logger::errorLog("Error sending response to client_fd: " + std::to_string(ctx.client_fd) + " - " + std::string(strerror(errno)));
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
		return false;
	}
	if (ctx.type == ERROR || !ctx.keepAlive)
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);

	return true;
}
