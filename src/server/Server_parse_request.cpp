#include "Server.hpp"

// Handles the parsing phase of an HTTP request, switching between header and body parsing
bool Server::handleParsingPhase(Context& ctx, const std::vector<ServerBlock>& configs)
{
	switch(ctx.req.parsing_phase)
	{
		case RequestBody::PARSING_HEADER:
			if (!ctx.headers_complete)
			{
				if (!parseHeaders(ctx, configs))
					return true;
				if (!handleContentLength(ctx, configs))
					return false;
				if (!handleTransferEncoding(ctx))
					return handleStandardBody(ctx);
			}
			break;

		case RequestBody::PARSING_BODY:
			return processParsingBody(ctx);

		case RequestBody::PARSING_COMPLETE:
			break;
	}
	return true;
}

// Reads and parses the incoming HTTP request into method, path, version, and headers
void Server::parseRequest(Context& ctx)
{
	char buffer[8192];
	ssize_t bytes = read(ctx.client_fd, buffer, sizeof(buffer));

	if (bytes <= 0)
		return;

	std::string request(buffer, bytes);
	std::istringstream stream(request);
	std::string line;

	if (std::getline(stream, line))
	{
		std::istringstream request_line(line);
		request_line >> ctx.method >> ctx.path >> ctx.version;
	}

	while (std::getline(stream, line) && !line.empty() && line != "\r")
	{
		size_t colon = line.find(':');
		if (colon != std::string::npos)
			return;
	}

}


// Finalizes request parsing by determining the request type and updating epoll events
bool Server::finalizeRequest(Context& ctx, const std::vector<ServerBlock>& configs)
{
	if (ctx.req.parsing_phase == RequestBody::PARSING_COMPLETE)
	{
		determineType(ctx, configs);
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLOUT | EPOLLET);
	}
	else if (ctx.error_code != 0)
		return false;
	return true;
}

// Processes standard (non-chunked) request bodies based on Content-Length
bool Server::handleStandardBody(Context& ctx)
{
	if (ctx.content_length > 0 && ctx.headers_complete)
	{
		if (ctx.content_length > ctx.client_max_body_size)
			return updateErrorStatus(ctx, 413, "Payload too large");

		ctx.req.parsing_phase = RequestBody::PARSING_BODY;
		ctx.req.expected_body_length = ctx.content_length;
		ctx.req.current_body_length = 0;

		if (!ctx.input_buffer.empty())
		{
			ctx.req.received_body += ctx.input_buffer;
			ctx.req.current_body_length += ctx.input_buffer.length();
			ctx.input_buffer.clear();

			if (ctx.req.current_body_length >= ctx.req.expected_body_length)
			{
				ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
				ctx.req.is_upload_complete = true;
			}
		}
		return true;
	}
	ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
	return true;
}

// Processes request body parsing, handling chunked and standard bodies
bool Server::processParsingBody(Context& ctx)
{
	if (ctx.req.chunked_state.processing)
	{
		parseChunkedBody(ctx);
	}
	else
	{
		ctx.req.received_body += ctx.input_buffer;
		ctx.req.current_body_length += ctx.input_buffer.length();
		ctx.input_buffer.clear();
		if (ctx.req.current_body_length >= ctx.req.expected_body_length)
		{
			ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
			ctx.req.is_upload_complete = true;
			if (ctx.req.current_body_length > ctx.req.expected_body_length)
			{
				ctx.req.received_body = ctx.req.received_body.substr(0, ctx.req.expected_body_length);
				ctx.req.current_body_length = ctx.req.expected_body_length;
			}
		}
	}
	return true;
}

// Checks if the request has been fully received based on headers and body content
bool Server::isRequestComplete(Context& ctx)
{
	if (!ctx.headers_complete)
		return false;

	auto it = ctx.headers.find("Content-Length");
	if (it == ctx.headers.end())
		return true;

	return (ctx.body_received >= ctx.content_length);
}
