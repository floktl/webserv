#include "Server.hpp"

bool Server::redirectAction(Context& ctx)
{
    int return_code = 301;
    std::string return_url = "/";

    if (!ctx.location.return_code.empty())
        return_code = std::stoi(ctx.location.return_code);
    if (!ctx.location.return_url.empty())
        return_url = ctx.location.return_url;

    std::stringstream log_message;
    log_message << "Handling redirect: code=" << return_code << " url=" << return_url;
    Logger::file(log_message.str());

    std::stringstream response;
    response << "HTTP/1.1 " << return_code << " Moved Permanently\r\n"
            << "Location: " << return_url << "\r\n"
            << "Connection: " << (ctx.keepAlive ? "keep-alive" : "close") << "\r\n"
            << "Content-Length: 0\r\n"
            << "\r\n";

    if (!sendHandler(ctx, response.str()))
        return  Logger::error("Failed to send redirect response");
    if (ctx.keepAlive)
	{
        resetContext(ctx);
        modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
    }
    return true;
}

