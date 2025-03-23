#include "./server.hpp"

bool Server::isMultipart(Context& ctx)
{
	bool isMultipartUpload = ctx.method == "POST" &&
		ctx.headers.find("Content-Type") != ctx.headers.end() &&
		ctx.headers["Content-Type"].find("multipart/form-data") != std::string::npos;

	return isMultipartUpload;
}

bool Server::parseCGIBody(Context& ctx)
{
	ctx.body += ctx.read_buffer;

	bool finished = false;

	if (ctx.headers.count("Content-Length"))
	{
		unsigned long expected_length = std::stol(ctx.headers["Content-Length"]);
		if (ctx.body.length() >= expected_length)
			finished = true;
	}
	else if (ctx.read_buffer.empty())
		finished = true;

	if (finished)
		return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
	else
		return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN);
}
