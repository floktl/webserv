#include "Server.hpp"

void Server::fillCgiEnvironmentVariables(const Context& ctx, std::vector<std::string> &env)
{
	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("SERVER_PROTOCOL=" + ctx.version);
	env.push_back("SERVER_SOFTWARE=Webserv/1.0");
	env.push_back("SERVER_NAME=" + ctx.name);
	env.push_back("SERVER_PORT=" + std::to_string(ctx.port));
	env.push_back("REQUEST_METHOD=" + ctx.method);

	std::string pathWithoutQuery = ctx.path;
	size_t queryPos = pathWithoutQuery.find('?');
	if (queryPos != std::string::npos)
		pathWithoutQuery = pathWithoutQuery.substr(0, queryPos);

	std::string docRoot = ctx.location.root.empty() ? ctx.root : ctx.location.root;
	if (!docRoot.empty())
		env.push_back("DOCUMENT_ROOT=" + docRoot);

	std::string scriptPath = ctx.requested_path;
	if (!docRoot.empty() && scriptPath.find(docRoot) == 0)
	{
		scriptPath = scriptPath.substr(docRoot.length());
		if (scriptPath.empty() || scriptPath[0] != '/')
			scriptPath = "/" + scriptPath;
	}
	env.push_back("PATH_INFO=" + scriptPath);
	env.push_back("SCRIPT_NAME=" + scriptPath);
	env.push_back("SCRIPT_FILENAME=" + ctx.requested_path);
	env.push_back("QUERY_STRING=" + extractQueryString(ctx.path));
	env.push_back("REDIRECT_STATUS=200");
	env.push_back("REQUEST_URI=" + ctx.path);
	if (ctx.method == "POST")
	{
		std::string contentType = "application/x-www-form-urlencoded";
		if (ctx.headers.count("Content-Type"))
			contentType = ctx.headers.at("Content-Type");
		env.push_back("CONTENT_TYPE=" + contentType);
		std::string contentLength = std::to_string(ctx.body.length());
		if (ctx.headers.count("Content-Length"))
			contentLength = ctx.headers.at("Content-Length");
		env.push_back("CONTENT_LENGTH=" + contentLength);
	}
}

std::vector<std::string> Server::prepareCgiEnvironment(const Context& ctx)
{
	std::vector<std::string> env;

	if (ctx.requested_path.empty())
		return env;

	fillCgiEnvironmentVariables(ctx, env);

	for (const auto& header : ctx.headers)
	{
		if (header.first == "Content-Type" || header.first == "Content-Length")
			continue;
		std::string name = "HTTP_" + header.first;
		std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return std::toupper(c); });
		std::replace(name.begin(), name.end(), '-', '_');
		env.push_back(name + "=" + header.second);
	}

	if (!ctx.cookies.empty())
	{
		std::string cookie_str = "HTTP_COOKIE=";
		for (size_t i = 0; i < ctx.cookies.size(); i++)
		{
			if (i > 0)
				cookie_str += "; ";
			cookie_str += ctx.cookies[i].first + "=" + ctx.cookies[i].second;
		}
		env.push_back(cookie_str);
	}
	return env;
}

std::string Server::extractQueryString(const std::string& path)
{
	size_t pos = path.find('?');
	if (pos != std::string::npos)
		return path.substr(pos + 1);
	return "";
}

bool Server::detectHtmlStartAndHeaderSeparator(Context& ctx, const std::string& separator, const std::string& separator_alt, bool& startsWithHtml, std::vector<char>::iterator& it)
{
	startsWithHtml = false;

	std::string buffer_start(ctx.write_buffer.begin(),
							ctx.write_buffer.begin() + std::min(size_t(20), ctx.write_buffer.size()));

	if (buffer_start.find("<!DOCTYPE") != std::string::npos ||
		buffer_start.find("<html") != std::string::npos)
	{
		startsWithHtml = true;
		Logger::green("Response starts with HTML without headers");
	}

	it = std::search(ctx.write_buffer.begin(), ctx.write_buffer.end(),
					separator.begin(), separator.end());

	if (it == ctx.write_buffer.end())
	{
		it = std::search(ctx.write_buffer.begin(), ctx.write_buffer.end(),
						separator_alt.begin(), separator_alt.end());
	}

	return true;
}

bool Server::injectDefaultHtmlHeaders(Context& ctx)
{
	Logger::green("No headers found but content starts with HTML, adding default headers");

	std::string headers = "HTTP/1.1 200 OK\r\n";
	headers += "Content-Type: text/html; charset=UTF-8\r\n";
	headers += "Content-Length: " + std::to_string(ctx.write_buffer.size()) + "\r\n";
	headers += "Server: Webserv/1.0\r\n";

	time_t now = time(0);
	struct tm tm;
	char dateBuffer[100];
	gmtime_r(&now, &tm);
	strftime(dateBuffer, sizeof(dateBuffer), "%a, %d %b %Y %H:%M:%S GMT", &tm);
	headers += "Date: " + std::string(dateBuffer) + "\r\n";

	headers += "Connection: " + std::string(ctx.keepAlive ? "keep-alive" : "close") + "\r\n";
	for (const auto& cookie : ctx.setCookies)
		headers += "Set-Cookie: " + cookie.first + "=" + cookie.second + "\r\n";

	headers += "\r\n";

	std::vector<char> newBuffer(headers.begin(), headers.end());
	newBuffer.insert(newBuffer.end(), ctx.write_buffer.begin(), ctx.write_buffer.end());
	ctx.write_buffer = newBuffer;
	ctx.cgi_headers_send = true;

	return true;
}

void Server::parseCgiHeaders(const std::string& existingHeaders,
							int& cgiStatusCode,
							std::string& cgiStatusMessage,
							std::string& cgiLocation,
							bool& cgiRedirect)
{
	std::istringstream headerStream(existingHeaders);
	std::string line;

	while (std::getline(headerStream, line))
	{
		if (!line.empty() && line.back() == '\r')
			line.pop_back();
		if (line.empty())
			continue;

		if (line.compare(0, 7, "Status:") == 0 || line.compare(0, 7, "status:") == 0)
		{
			std::string statusLine = line.substr(7);
			size_t firstNonSpace = statusLine.find_first_not_of(" \t");
			if (firstNonSpace != std::string::npos)
				statusLine = statusLine.substr(firstNonSpace);

			std::stringstream ss(statusLine);
			ss >> cgiStatusCode;

			if (ss.peek() == ' ')
			{
				ss.ignore();
				std::getline(ss, cgiStatusMessage);
			}
			if (cgiStatusCode >= 300 && cgiStatusCode < 400)
				cgiRedirect = true;
		}

		if (line.compare(0, 9, "Location:") == 0 || line.compare(0, 9, "location:") == 0)
		{
			cgiLocation = line.substr(9);
			size_t firstNonSpace = cgiLocation.find_first_not_of(" \t");
			if (firstNonSpace != std::string::npos)
				cgiLocation = cgiLocation.substr(firstNonSpace);
			cgiRedirect = true;
			if (cgiStatusCode == 200)
			{
				cgiStatusCode = 302;
				cgiStatusMessage = "Found";
			}
		}
	}
}

std::string Server::buildInitialCgiResponseHeader(const Context& ctx,
												int cgiStatusCode,
												const std::string& cgiStatusMessage,
												bool cgiRedirect,
												bool& isRedirect)
{
	isRedirect = cgiRedirect || (ctx.error_code >= 300 && ctx.error_code < 400);
	std::string headers;

	if (cgiRedirect || cgiStatusCode != 200)
		headers = "HTTP/1.1 " + std::to_string(cgiStatusCode) + " " + cgiStatusMessage + "\r\n";
	else if (ctx.error_code > 0)
		headers = "HTTP/1.1 " + std::to_string(ctx.error_code) + " " + ctx.error_message + "\r\n";
	else
		headers = "HTTP/1.1 200 OK\r\n";

	if (!isRedirect && ctx.location.return_code.length() > 0)
	{
		int redirectCode = std::stoi(ctx.location.return_code);
		if (redirectCode >= 300 && redirectCode < 400)
		{
			isRedirect = true;
			headers = "HTTP/1.1 " + ctx.location.return_code + " ";
			if (redirectCode == 301)
				headers += "Moved Permanently";
			else if (redirectCode == 302)
				headers += "Found";
			else if (redirectCode == 303)
				headers += "See Other";
			else if (redirectCode == 307)
				headers += "Temporary Redirect";
			else if (redirectCode == 308)
				headers += "Permanent Redirect";
			else
				headers += "Redirect";
			headers += "\r\n";
		}
	}
	return headers;
}

void Server::appendStandardOrCgiHeaders(const Context& ctx,
	bool hasHeaders,
	const std::string& existingHeaders,
	std::string& headers,
	const std::string& cgiLocation,
	bool isRedirect,
	bool hasContentType)
{

	if (isRedirect && headers.find("Location:") == std::string::npos)
	{
		std::string redirectUrl;

		if (!cgiLocation.empty())
			redirectUrl = cgiLocation;
		else if (ctx.error_code >= 300 && ctx.error_code < 400 && !ctx.error_message.empty())
			redirectUrl = ctx.error_message;
		else if (!ctx.location.return_url.empty())
			redirectUrl = ctx.location.return_url;
		if (!redirectUrl.empty())
			headers += "Location: " + redirectUrl + "\r\n";
	}
	if (hasContentType && hasHeaders)
	{
		std::istringstream headerStream(existingHeaders);
		std::string line;

		while (std::getline(headerStream, line))
		{
			if (!line.empty() && line.back() == '\r')
				line.pop_back();

			if (line.compare(0, 12, "Content-Type:") == 0 || line.compare(0, 12, "content-type:") == 0)
			{
				headers += line + "\r\n";
				break;
			}
		}
	}
	else if (!isRedirect)
		headers += "Content-Type: text/html; charset=UTF-8\r\n";
}

void Server::finalizeCgiHeaders(const Context& ctx,
	std::string& headers,
	const std::string& existingHeaders,
	size_t headerEnd,
	size_t separatorSize,
	bool hasHeaders,
	bool isRedirect,
	bool hasContentLength,
	bool hasServer,
	bool hasDate,
	bool hasConnection)
{
	if (!hasContentLength && hasHeaders && !isRedirect)
	{
		size_t bodySize = ctx.write_buffer.size() - (headerEnd + separatorSize);
		headers += "Content-Length: " + std::to_string(bodySize) + "\r\n";
	}
	else if (hasContentLength && hasHeaders)
	{
		std::istringstream headerStream(existingHeaders);
		std::string line;

		while (std::getline(headerStream, line))
		{
			if (!line.empty() && line.back() == '\r')
				line.pop_back();

			if (line.compare(0, 14, "Content-Length:") == 0 || line.compare(0, 14, "content-length:") == 0)
			{
				headers += line + "\r\n";
				break;
			}
		}
	}
	else if (isRedirect && !hasContentLength)
		headers += "Content-Length: 0\r\n";
	if (!hasServer)
		headers += "Server: Webserv/1.0\r\n";
	if (!hasDate)
	{
		time_t now = time(0);
		struct tm tm;
		char dateBuffer[100];
		gmtime_r(&now, &tm);
		strftime(dateBuffer, sizeof(dateBuffer), "%a, %d %b %Y %H:%M:%S GMT", &tm);
		headers += "Date: " + std::string(dateBuffer) + "\r\n";
	}
	if (!hasConnection)
		headers += "Connection: " + std::string(ctx.keepAlive ? "keep-alive" : "close") + "\r\n";
	for (const auto& cookie : ctx.setCookies)
		headers += "Set-Cookie: " + cookie.first + "=" + cookie.second + "\r\n";
}

void Server::finalizeCgiWriteBuffer(Context& ctx,
	const std::string& existingHeaders,
	std::string& headers,
	size_t headerEnd,
	size_t separatorSize,
	bool hasHeaders,
	bool isRedirect,
	bool cgiRedirect)
{
	if (hasHeaders)
	{
		std::istringstream headerStream(existingHeaders);
		std::string line;

		while (std::getline(headerStream, line))
		{
			if (!line.empty() && line.back() == '\r')
				line.pop_back();

			if (line.empty()
				|| line.compare(0, 7, "Status:") == 0 || line.compare(0, 7, "status:") == 0
				|| line.compare(0, 9, "Location:") == 0 || line.compare(0, 9, "location:") == 0
				|| line.compare(0, 12, "Content-Type:") == 0 || line.compare(0, 12, "content-type:") == 0
				|| line.compare(0, 14, "Content-Length:") == 0 || line.compare(0, 14, "content-length:") == 0)
				continue;
			headers += line + "\r\n";
		}
	}

	headers += "\r\n";
	std::vector<char> body;
	if (hasHeaders && headerEnd + separatorSize < ctx.write_buffer.size())
	{
		body.insert(body.begin(),
		ctx.write_buffer.begin() + headerEnd + separatorSize,
		ctx.write_buffer.end());
	}

	if (isRedirect && cgiRedirect)
		body.clear();
	std::vector<char> newBuffer(headers.begin(), headers.end());
	if (!body.empty())
		newBuffer.insert(newBuffer.end(), body.begin(), body.end());
	ctx.write_buffer = newBuffer;
	ctx.cgi_headers_send = true;
}


bool Server::prepareCgiHeaders(Context& ctx)
{
    std::string separator = "\r\n\r\n";
    std::string separator_alt = "\n\n";
	bool startsWithHtml = false;
	std::vector<char>::iterator it;

	detectHtmlStartAndHeaderSeparator(ctx, separator, separator_alt, startsWithHtml, it);

    bool hasHeaders = (it != ctx.write_buffer.end());

	if (!hasHeaders && startsWithHtml)
		return injectDefaultHtmlHeaders(ctx);

    size_t headerEnd = hasHeaders ? std::distance(ctx.write_buffer.begin(), it) : 0;
    size_t separatorSize = (it != ctx.write_buffer.end() &&
                        std::string(it, it + 4) == "\r\n\r\n") ? 4 : 2;

    std::string existingHeaders;
    if (hasHeaders)
        existingHeaders = std::string(ctx.write_buffer.begin(), it);

    int cgiStatusCode = 200;
    std::string cgiStatusMessage = "OK";
    std::string cgiLocation;
    bool cgiRedirect = false;

	if (hasHeaders)
		parseCgiHeaders(existingHeaders, cgiStatusCode, cgiStatusMessage, cgiLocation, cgiRedirect);

	bool isRedirect = false;
	std::string headers = buildInitialCgiResponseHeader(ctx, cgiStatusCode, cgiStatusMessage, cgiRedirect, isRedirect);

	bool hasContentType = hasHeaders && (existingHeaders.find("Content-Type:") != std::string::npos
		|| existingHeaders.find("content-type:") != std::string::npos);
	bool hasContentLength = hasHeaders && (existingHeaders.find("Content-Length:") != std::string::npos
		|| existingHeaders.find("content-length:") != std::string::npos);
	bool hasServer = hasHeaders && existingHeaders.find("Server:") != std::string::npos;
	bool hasDate = hasHeaders && existingHeaders.find("Date:") != std::string::npos;
	bool hasConnection = hasHeaders && existingHeaders.find("Connection:") != std::string::npos;
	appendStandardOrCgiHeaders(ctx, hasHeaders, existingHeaders, headers, cgiLocation, isRedirect, hasContentType);
    finalizeCgiHeaders(ctx, headers, existingHeaders, headerEnd, separatorSize,
		hasHeaders, isRedirect, hasContentLength, hasServer, hasDate, hasConnection);
	finalizeCgiWriteBuffer(ctx, existingHeaders, headers, headerEnd, separatorSize,
		hasHeaders, isRedirect, cgiRedirect);
    return true;
}


bool Server::handleInitialCgiWritePhase(Context& ctx)
{
	if (!ctx.cgi_headers_send)
	{
		prepareCgiHeaders(ctx);
		ctx.cgi_headers_send = true;
	}

	ssize_t sent = send(ctx.client_fd, &ctx.write_buffer[0], ctx.write_buffer.size(), MSG_NOSIGNAL);

	if (sent < 0)
	{
		if (errno == EAGAIN || errno == EWOULDBLOCK)
			return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET);

		cleanupCgiResources(ctx);
		return false;
	}
	else if (sent > 0)
	{
		ctx.write_buffer.erase(
			ctx.write_buffer.begin(),
			ctx.write_buffer.begin() + sent
		);

		if (ctx.write_buffer.empty() && !ctx.cgi_terminated)
		{
			ctx.cgi_output_phase = true;
			return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET);
		}
		else if (!ctx.write_buffer.empty())
			return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET);
	}

	return true;
}

bool Server::handleCgiOutputPhase(Context& ctx)
{
	bool readComplete = checkAndReadCgiPipe(ctx);

	if (readComplete)
	{
		if (ctx.write_buffer.empty() && !ctx.cgi_terminated)
			return true;

		std::string buffer_str(ctx.write_buffer.begin(), ctx.write_buffer.end());
		bool hasLocationHeader = (buffer_str.find("Location:") != std::string::npos ||
			buffer_str.find("location:") != std::string::npos);

		if (hasLocationHeader)
		{
			std::string headers_end_crlf = "\r\n\r\n";
			std::string headers_end_lf = "\n\n";

			auto it_crlf = std::search(ctx.write_buffer.begin(), ctx.write_buffer.end(),
				headers_end_crlf.begin(), headers_end_crlf.end());
			auto it_lf = std::search(ctx.write_buffer.begin(), ctx.write_buffer.end(),
				headers_end_lf.begin(), headers_end_lf.end());

			if (it_crlf == ctx.write_buffer.end() && it_lf == ctx.write_buffer.end())
			{
				Logger::yellow("Adding missing header ending for Location redirect");
				ctx.write_buffer.insert(ctx.write_buffer.end(), {'\r', '\n', '\r', '\n'});
			}
		}

		if (!ctx.write_buffer.empty() || ctx.cgi_terminated)
		{
			ctx.cgi_output_phase = false;
			return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET);
		}
	}
	return true;
}

bool Server::handleCgiTermination(Context& ctx)
{
	if (ctx.keepAlive)
	{
		resetContext(ctx);
		return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
	}
	else
	{
		delFromEpoll(ctx.epoll_fd, ctx.client_fd);
		return false;
	}
}

bool Server::sendCgiResponse(Context& ctx)
{
	if (!ctx.cgi_pipe_ready)
		return true;

	if (ctx.write_buffer.empty() && !ctx.cgi_output_phase)
	{
		ctx.cgi_terminated = true;
		cleanupCgiResources(ctx);
		return handleCgiTermination(ctx);
	}

	if (ctx.cgi_output_phase)
		return handleCgiOutputPhase(ctx);
	else
		return handleInitialCgiWritePhase(ctx);

	if (ctx.cgi_terminated)
		return handleCgiTermination(ctx);

	return true;
}

bool Server::checkAndReadCgiPipe(Context& ctx)
{
	if (ctx.req.cgi_out_fd <= 0)
		return false;

	char buffer[DEFAULT_CGIBUFFER_SIZE];
	ssize_t bytes_read = 0;

	bytes_read = read(ctx.req.cgi_out_fd, buffer, sizeof(buffer));

	if (bytes_read > 0)
	{
		ctx.write_buffer.insert(ctx.write_buffer.end(), buffer, buffer + bytes_read);
		ctx.last_activity = std::chrono::steady_clock::now();
		std::string headers_end_crlf = "\r\n\r\n";
		std::string headers_end_lf = "\n\n";

		auto it_crlf = std::search(ctx.write_buffer.begin(), ctx.write_buffer.end(),
						headers_end_crlf.begin(), headers_end_crlf.end());
		auto it_lf = std::search(ctx.write_buffer.begin(), ctx.write_buffer.end(),
						headers_end_lf.begin(), headers_end_lf.end());
		std::string buffer_str(ctx.write_buffer.begin(), ctx.write_buffer.end());
		bool hasLocationHeader = (buffer_str.find("Location:") != std::string::npos ||
								buffer_str.find("location:") != std::string::npos);

		if (it_crlf != ctx.write_buffer.end() || it_lf != ctx.write_buffer.end())
			return true;
		if (hasLocationHeader && bytes_read < DEFAULT_CGIBUFFER_SIZE)
			return true;
		return false;
	}
	else if (bytes_read == 0)
	{
		if (ctx.req.cgi_out_fd > 0)
		{
			epoll_ctl(ctx.epoll_fd, EPOLL_CTL_DEL, ctx.req.cgi_out_fd, nullptr);
			close(ctx.req.cgi_out_fd);
			globalFDS.cgi_pipe_to_client_fd.erase(ctx.req.cgi_out_fd);
			ctx.req.cgi_out_fd = -1;
			ctx.cgi_terminated = true;
		}
	}
	return true;
}

void Server::cleanupCgiResources(Context& ctx)
{
	if (ctx.req.cgi_out_fd > 0)
	{
		epoll_ctl(ctx.epoll_fd, EPOLL_CTL_DEL, ctx.req.cgi_out_fd, nullptr);
		globalFDS.cgi_pipe_to_client_fd.erase(ctx.req.cgi_out_fd);
		close(ctx.req.cgi_out_fd);
		ctx.req.cgi_out_fd = -1;
	}

	globalFDS.cgi_pid_to_client_fd.erase(ctx.req.cgi_pid);

	ctx.cgi_terminate = true;
	ctx.cgi_terminated = true;
	for (auto pipeIt = globalFDS.cgi_pipe_to_client_fd.begin(); pipeIt != globalFDS.cgi_pipe_to_client_fd.end(); )
	{
		if (pipeIt->second == ctx.client_fd)
			pipeIt = globalFDS.cgi_pipe_to_client_fd.erase(pipeIt);
		else
			++pipeIt;
	}
	for (auto pid_fd = globalFDS.cgi_pid_to_client_fd.begin(); pid_fd != globalFDS.cgi_pid_to_client_fd.end(); )
	{
		if (pid_fd->second == ctx.client_fd)
			pid_fd = globalFDS.cgi_pid_to_client_fd.erase(pid_fd);
		else
			++pid_fd;
	}
	ctx.cgi_run_to_timeout = true;
}
