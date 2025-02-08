#include "Server.hpp"

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
			Logger::file("in PARSING_BODY");
			return processParsingBody(ctx);

		case RequestBody::PARSING_COMPLETE:
			Logger::file("in PARSING_COMPLETE");
			break;
	}
	return true;
}

void Server::parseRequest(Context& ctx)
{
	char buffer[8192];
	ssize_t bytes = read(ctx.client_fd, buffer, sizeof(buffer));

	if (bytes <= 0)
		return;

	std::string request(buffer, bytes);
	std::istringstream stream(request);
	std::string line;

	// Parse request line
	if (std::getline(stream, line))
	{
		std::istringstream request_line(line);
		request_line >> ctx.method >> ctx.path >> ctx.version;
	}

	// Parse headers
	while (std::getline(stream, line) && !line.empty() && line != "\r")
	{
		size_t colon = line.find(':');
		if (colon != std::string::npos)
		{
			std::string key = line.substr(0, colon);
			std::string value = line.substr(colon + 2); // Skip ": "
			ctx.headers[key] = value;
		}
	}

}

bool Server::parseHeaders(Context& ctx, const std::vector<ServerBlock>& configs)
{
    size_t header_end = ctx.input_buffer.find("\r\n\r\n");
    if (header_end == std::string::npos)
        return true;

    std::string headers = ctx.input_buffer.substr(0, header_end);
    std::istringstream stream(headers);
    std::string line;

    if (!std::getline(stream, line))
	{
        updateErrorStatus(ctx, 400, "Bad Request - Empty Request");
        return false;
    }

    if (line.back() == '\r')
		line.pop_back();
    std::istringstream request_line(line);
    request_line >> ctx.method >> ctx.path >> ctx.version;
    if (ctx.method.empty() || ctx.path.empty() || ctx.version.empty())
	{
        updateErrorStatus(ctx, 400, "Bad Request - Invalid Request Line");
        return false;
    }

    while (std::getline(stream, line))
	{
        if (line.empty() || line == "\r") break;
        if (line.back() == '\r') line.pop_back();

        size_t colon = line.find(':');
        if (colon != std::string::npos) {
            std::string key = line.substr(0, colon);
            std::string value = line.substr(colon + 1);
            value = value.substr(value.find_first_not_of(" "));
            ctx.headers[key] = value;

            if (key == "Content-Length")
			{
                try
				{
                    ctx.content_length = std::stoull(value);
                }
				catch (const std::exception& e)
				{
                    updateErrorStatus(ctx, 400, "Bad Request - Invalid Content-Length");
                    return false;
                }
            }
            ctx.setCookie = "";
            ctx.cookies.clear();
            if (key == "Set-Cookie")
                parseNewCookie(ctx, value);
            if (key == "Cookie")
                parseCookies(ctx, value);
        }
    }

    std::string requested_host = extractHostname(ctx.input_buffer);
    int requested_port = extractPort(ctx.input_buffer);
    bool server_match_found = false;
    for (const auto &config : configs)
    {
		if (((config.name == requested_host && config.port == requested_port) ||
			(config.name == "localhost" && (requested_host == "localhost" || requested_host == "127.0.0.1")) ||
			(requested_host.empty() && config.port == requested_port)))
		{
			server_match_found = true;
			break;
		}
    }
    if (!server_match_found)
    {
		Logger::errorLog("No matching server block found - Closing connection");
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
        return false;
    }

    ctx.input_buffer.erase(0, header_end + 4);
    ctx.headers_complete = true;

    if (ctx.method == "POST" && ctx.headers.find("Content-Length") == ctx.headers.end() &&
        ctx.headers.find("Transfer-Encoding") == ctx.headers.end())
        return updateErrorStatus(ctx, 411, "Length Required");
    return true;
}

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

void Server::parseAccessRights(Context& ctx)
{
	std::string requestedPath = concatenatePath(ctx.root, ctx.path);
	if (ctx.index.empty())
		ctx.index = "index.html";
	if (ctx.location.default_file.empty())
		ctx.location.default_file = ctx.index;
	if (!requestedPath.empty() && requestedPath.back() == '/')
		requestedPath = concatenatePath(requestedPath, ctx.location.default_file);
	if (ctx.location_inited && requestedPath == ctx.location.upload_store && dirWritable(requestedPath))
		return;
	requestedPath = approveExtention(ctx, requestedPath);
	if (ctx.type == ERROR)
		return;
	checkAccessRights(ctx, requestedPath);
	if (ctx.type == ERROR)
		return;
	ctx.approved_req_path = requestedPath;
}

int extractPort(const std::string& header)
{
	size_t host_pos = header.find("Host: ");
	if (host_pos != std::string::npos)
	{
		size_t port_pos = header.find(":", host_pos + 6);
		if (port_pos != std::string::npos)
		{
			size_t end_pos = header.find("\r\n", port_pos);
			if (end_pos != std::string::npos)
			{
				std::string port_str = header.substr(port_pos + 1, end_pos - port_pos - 1);
				port_str = trim(port_str);
				try
				{
					int port = std::stoi(port_str);
					if (port <= 0 || port > 65535)
					{
						Logger::red("Invalid port number: '" + port_str + "'");
						return -1;
					}
					return port;
				}
				catch (const std::exception& e)
				{
					return -1;
				}
			}
		}
		else
			return 80;
	}
	return -1;
}

std::string extractHostname(const std::string& header)
{
	std::string hostname;
	size_t host_pos = header.find("Host: ");
	if (host_pos != std::string::npos)
	{
		size_t start = host_pos + 6;
		size_t end_pos = header.find("\r\n", start);
		if (end_pos != std::string::npos)
		{
			size_t colon_pos = header.find(":", start);
			if (colon_pos != std::string::npos && colon_pos < end_pos)
				hostname = header.substr(start, colon_pos - start);
			else
				hostname = header.substr(start, end_pos - start);
		}
	}
	return hostname; // Returns empty string if no Host header found
}

void Server::parseNewCookie(Context& ctx, std::string value)
{
	Logger::errorLog("parseNewCookie: " + value);
    ctx.setCookie = value;
}

void Server::parseCookies(Context& ctx, std::string value)
{
	Logger::errorLog("parseCookies: " + value);
	ctx.cookies = value;
}

void Server::parseChunkedBody(Context& ctx)
{
	while (!ctx.input_buffer.empty())
	{
		if (!ctx.req.chunked_state.processing)
		{
			size_t pos = ctx.input_buffer.find("\r\n");
			if (pos == std::string::npos)
				return;

			// Parse chunk size
			std::string chunk_size_str = ctx.input_buffer.substr(0, pos);
			size_t chunk_size;
			std::stringstream ss;
			ss << std::hex << chunk_size_str;
			ss >> chunk_size;

			if (chunk_size == 0)
			{
				// Final chunk
				ctx.req.parsing_phase = RequestBody::PARSING_COMPLETE;
				ctx.req.is_upload_complete = true;
				ctx.input_buffer.clear();
				return;
			}
			ctx.input_buffer.erase(0, pos + 2);
			ctx.req.chunked_state.buffer = "";
			ctx.req.chunked_state.processing = true;
			ctx.req.expected_body_length += chunk_size;
		}

		// Process chunk data
		size_t remaining = ctx.req.expected_body_length - ctx.req.current_body_length;
		size_t available = std::min(remaining, ctx.input_buffer.size());

		if (available > 0) {
			ctx.req.received_body += ctx.input_buffer.substr(0, available);
			ctx.req.current_body_length += available;
			ctx.input_buffer.erase(0, available);
		}
		// Check for chunk end
		if (ctx.req.current_body_length == ctx.req.expected_body_length)
		{
			if (ctx.input_buffer.size() >= 2 && ctx.input_buffer.substr(0, 2) == "\r\n")
			{
				ctx.input_buffer.erase(0, 2);
				ctx.req.chunked_state.processing = false;
			}
			else
				return; // Need more data
		}
	}
}

bool Server::handleContentLength(Context& ctx, const std::vector<ServerBlock>& configs)
{
	auto headersit = ctx.headers.find("Content-Length");
	long long content_length;

	if (headersit != ctx.headers.end())
	{
		content_length = std::stoull(headersit->second);
		getMaxBodySizeFromConfig(ctx, configs);
		if (content_length > ctx.client_max_body_size)
			return updateErrorStatus(ctx, 413, "Payload too large");
	}
	return true;
}

bool Server::handleTransferEncoding(Context& ctx)
{
	Logger::file("handleTransferEncoding");
	auto it = ctx.headers.find("Transfer-Encoding");
	if (it != ctx.headers.end() && it->second == "chunked")
	{
		ctx.req.parsing_phase = RequestBody::PARSING_BODY;
		ctx.req.chunked_state.processing = true;
		if (!ctx.input_buffer.empty())
			parseChunkedBody(ctx);
		return true;
	}
	return false;
}

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

bool Server::isRequestComplete(Context& ctx)
{
	if (!ctx.headers_complete)
		return false;

	auto it = ctx.headers.find("Content-Length");
	if (it == ctx.headers.end())
		return true;

	return (ctx.body_received >= ctx.content_length);
}
