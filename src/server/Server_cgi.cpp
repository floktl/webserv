#include "Server.hpp"

bool Server::executeCgi(Context& ctx)
{
	//Logger::green("executeCgi " + ctx.requested_path);

	if (ctx.requested_path.empty() || !fileExists(ctx.requested_path) || !fileReadable(ctx.requested_path)) {
		//Logger::error("CGI script validation failed: " + ctx.requested_path);
		return updateErrorStatus(ctx, 404, "Not Found - CGI Script");
	}

	int input_pipe[2];
	int output_pipe[2];

	if (pipe(input_pipe) < 0 || pipe(output_pipe) < 0) {
		Logger::error("Failed to create pipes for CGI");
		Logger::file("Failed to create pipes for CGI");
		return updateErrorStatus(ctx, 500, "Internal Server Error - Pipe Creation Failed");
	}

	if (setNonBlocking(input_pipe[0]) < 0 || setNonBlocking(input_pipe[1]) < 0 ||
		setNonBlocking(output_pipe[0]) < 0 || setNonBlocking(output_pipe[1]) < 0) {
		Logger::error("Failed to set pipes to non-blocking mode");
		Logger::file("Failed to set pipes to non-blocking mode");
		close(input_pipe[0]);
		close(input_pipe[1]);
		close(output_pipe[0]);
		close(output_pipe[1]);
		return updateErrorStatus(ctx, 500, "Internal Server Error - Non-blocking Pipe Failed");
	}

	pid_t pid = fork();

	if (pid < 0) {
		Logger::error("Failed to fork for CGI execution");
		Logger::file("Failed to fork for CGI execution");
		close(input_pipe[0]);
		close(input_pipe[1]);
		close(output_pipe[0]);
		close(output_pipe[1]);
		return updateErrorStatus(ctx, 500, "Internal Server Error - Fork Failed");
	}

	if (pid == 0) {
		dup2(input_pipe[0], STDIN_FILENO);
		dup2(output_pipe[1], STDOUT_FILENO);
		dup2(output_pipe[1], STDERR_FILENO);

		close(input_pipe[0]);
		close(input_pipe[1]);
		close(output_pipe[0]);
		close(output_pipe[1]);

		std::vector<std::string> env_vars = prepareCgiEnvironment(ctx);
		std::vector<char*> env_pointers;
		for (auto& var : env_vars) {
			env_pointers.push_back(const_cast<char*>(var.c_str()));
		}
		env_pointers.push_back(nullptr);

		//logContext(ctx, "CGI");

		char* args[3];
		args[0] = const_cast<char*>(ctx.location.cgi.c_str());

		args[1] = const_cast<char*>(ctx.requested_path.c_str());
		args[2] = nullptr;

		execve(ctx.location.cgi.c_str(), args, env_pointers.data());
		exit(1);
	}

	close(input_pipe[0]);
	close(output_pipe[1]);

	ctx.req.cgi_in_fd = input_pipe[1];
	ctx.req.cgi_out_fd = output_pipe[0];
	ctx.req.cgi_pid = pid;
	ctx.req.state = RequestBody::STATE_CGI_RUNNING;

	ctx.cgi_pipe_ready = false;
	ctx.cgi_start_time = std::chrono::steady_clock::now();
	//Logger::red("Start time (seconds since epoch-ish): " +
	//	std::to_string(ctx.cgi_start_time.time_since_epoch().count()));



	globalFDS.cgi_pipe_to_client_fd[ctx.req.cgi_out_fd] = ctx.client_fd;
	globalFDS.cgi_pid_to_client_fd[ctx.req.cgi_pid] = ctx.client_fd;

	struct epoll_event ev;
	std::memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = ctx.req.cgi_out_fd;
	if (epoll_ctl(ctx.epoll_fd, EPOLL_CTL_ADD, ctx.req.cgi_out_fd, &ev) < 0) {
		Logger::error("Failed to add CGI output pipe to epoll: " + std::string(strerror(errno)));
	}// else {
	//	Logger::green("Added CGI output pipe " + std::to_string(ctx.req.cgi_out_fd) + " to epoll");
	//}

	if (!ctx.body.empty()) {
		write(ctx.req.cgi_in_fd, ctx.body.c_str(), ctx.body.length());
	}

	close(ctx.req.cgi_in_fd);
	ctx.req.cgi_in_fd = -1;

	ctx.last_activity = std::chrono::steady_clock::now();

	ctx.cgi_executed = true;
	return (modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET));
}


std::vector<std::string> Server::prepareCgiEnvironment(const Context& ctx) {
	std::vector<std::string> env;

	if (ctx.requested_path.empty()) {
		Logger::error("Empty script path for CGI environment");
		return env;
	}

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("SERVER_PROTOCOL=" + ctx.version);
	env.push_back("SERVER_SOFTWARE=Webserv/1.0");
	env.push_back("SERVER_NAME=" + ctx.name);
	env.push_back("SERVER_PORT=" + std::to_string(ctx.port));
	env.push_back("REQUEST_METHOD=" + ctx.method);

	std::string pathWithoutQuery = ctx.path;
	size_t queryPos = pathWithoutQuery.find('?');
	if (queryPos != std::string::npos) {
		pathWithoutQuery = pathWithoutQuery.substr(0, queryPos);
	}

	std::string docRoot = ctx.location.root.empty() ? ctx.root : ctx.location.root;
	if (!docRoot.empty()) {
		env.push_back("DOCUMENT_ROOT=" + docRoot);
	}

	std::string scriptPath = ctx.requested_path;
	if (!docRoot.empty() && scriptPath.find(docRoot) == 0) {
		scriptPath = scriptPath.substr(docRoot.length());
		if (scriptPath.empty() || scriptPath[0] != '/') {
			scriptPath = "/" + scriptPath;
		}
	}

	env.push_back("PATH_INFO=" + scriptPath);
	env.push_back("SCRIPT_NAME=" + scriptPath);
	env.push_back("SCRIPT_FILENAME=" + ctx.requested_path);

	std::string queryString = extractQueryString(ctx.path);
	env.push_back("QUERY_STRING=" + queryString);

	env.push_back("REDIRECT_STATUS=200");
	env.push_back("REQUEST_URI=" + ctx.path);


	if (ctx.method == "POST") {
		std::string contentType = "application/x-www-form-urlencoded";
		if (ctx.headers.count("Content-Type")) {
			contentType = ctx.headers.at("Content-Type");
		}
		env.push_back("CONTENT_TYPE=" + contentType);

		std::string contentLength = std::to_string(ctx.body.length());
		if (ctx.headers.count("Content-Length")) {
			contentLength = ctx.headers.at("Content-Length");
		}
		env.push_back("CONTENT_LENGTH=" + contentLength);
	}

	for (const auto& header : ctx.headers) {
		if (header.first == "Content-Type" || header.first == "Content-Length") {
			continue;
		}

		std::string name = "HTTP_" + header.first;
		std::transform(name.begin(), name.end(), name.begin(),
			[](unsigned char c) { return std::toupper(c); });
		std::replace(name.begin(), name.end(), '-', '_');

		env.push_back(name + "=" + header.second);
	}

	if (!ctx.cookies.empty()) {
		std::string cookie_str = "HTTP_COOKIE=";
		for (size_t i = 0; i < ctx.cookies.size(); i++) {
			if (i > 0) cookie_str += "; ";
			cookie_str += ctx.cookies[i].first + "=" + ctx.cookies[i].second;
		}
		env.push_back(cookie_str);
	}
	return env;
}

std::string Server::extractQueryString(const std::string& path)
{
	size_t pos = path.find('?');
	if (pos != std::string::npos) {
		return path.substr(pos + 1);
	}
	return "";
}

bool Server::prepareCgiHeaders(Context& ctx) {
    std::string separator = "\r\n\r\n";
    std::string separator_alt = "\n\n";

    // Check if the response starts with HTML content
    bool startsWithHtml = false;
    std::string buffer_start(ctx.write_buffer.begin(),
                            ctx.write_buffer.begin() + std::min(size_t(20), ctx.write_buffer.size()));

    if (buffer_start.find("<!DOCTYPE") != std::string::npos ||
        buffer_start.find("<html") != std::string::npos) {
        startsWithHtml = true;
        Logger::green("Response starts with HTML without headers");
    }

    auto it = std::search(ctx.write_buffer.begin(), ctx.write_buffer.end(),
                        separator.begin(), separator.end());

    if (it == ctx.write_buffer.end()) {
        it = std::search(ctx.write_buffer.begin(), ctx.write_buffer.end(),
                        separator_alt.begin(), separator_alt.end());
    }

    bool hasHeaders = (it != ctx.write_buffer.end());

    // If no headers are found but the content starts with HTML, treat as no headers
    if (!hasHeaders && startsWithHtml) {
        Logger::green("No headers found but content starts with HTML, adding default headers");

        std::string headers = "HTTP/1.1 200 OK\r\n";
        headers += "Content-Type: text/html; charset=UTF-8\r\n";
        headers += "Content-Length: " + std::to_string(ctx.write_buffer.size()) + "\r\n";
        headers += "Server: Webserv/1.0\r\n";

        // Add date header
        time_t now = time(0);
        struct tm tm;
        char dateBuffer[100];
        gmtime_r(&now, &tm);
        strftime(dateBuffer, sizeof(dateBuffer), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        headers += "Date: " + std::string(dateBuffer) + "\r\n";

        // Add connection header
        headers += "Connection: " + std::string(ctx.keepAlive ? "keep-alive" : "close") + "\r\n";

        // Add cookies if any
        for (const auto& cookie : ctx.setCookies) {
            headers += "Set-Cookie: " + cookie.first + "=" + cookie.second + "\r\n";
        }

        headers += "\r\n";

        // Create new buffer with headers + original content
        std::vector<char> newBuffer(headers.begin(), headers.end());
        newBuffer.insert(newBuffer.end(), ctx.write_buffer.begin(), ctx.write_buffer.end());

        ctx.write_buffer = newBuffer;
        ctx.cgi_headers_send = true;

        return true;
    }

    size_t headerEnd = hasHeaders ? std::distance(ctx.write_buffer.begin(), it) : 0;
    size_t separatorSize = (it != ctx.write_buffer.end() &&
                        std::string(it, it + 4) == "\r\n\r\n") ? 4 : 2;

    std::string existingHeaders;
    if (hasHeaders) {
        existingHeaders = std::string(ctx.write_buffer.begin(), it);
       // Logger::green("CGI Headers: " + existingHeaders);
    }

    int cgiStatusCode = 200;
    std::string cgiStatusMessage = "OK";
    std::string cgiLocation;
    bool cgiRedirect = false;

    if (hasHeaders) {
        std::istringstream headerStream(existingHeaders);
        std::string line;

        while (std::getline(headerStream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.empty()) {
                continue;
            }

            if (line.compare(0, 7, "Status:") == 0 || line.compare(0, 7, "status:") == 0) {
                std::string statusLine = line.substr(7);
                size_t firstNonSpace = statusLine.find_first_not_of(" \t");
                if (firstNonSpace != std::string::npos) {
                    statusLine = statusLine.substr(firstNonSpace);
                }

                std::stringstream ss(statusLine);
                ss >> cgiStatusCode;

                if (ss.peek() == ' ') {
                    ss.ignore();
                    std::getline(ss, cgiStatusMessage);
                }

                //Logger::green("Found Status header: " + std::to_string(cgiStatusCode) + " " + cgiStatusMessage);

                if (cgiStatusCode >= 300 && cgiStatusCode < 400) {
                    cgiRedirect = true;
                   // Logger::green("Setting redirect flag based on status code");
                }
            }

            if (line.compare(0, 9, "Location:") == 0 || line.compare(0, 9, "location:") == 0) {
                cgiLocation = line.substr(9);
                size_t firstNonSpace = cgiLocation.find_first_not_of(" \t");
                if (firstNonSpace != std::string::npos) {
                    cgiLocation = cgiLocation.substr(firstNonSpace);
                }

              // Logger::green("Found Location header: " + cgiLocation);

                cgiRedirect = true;
                //Logger::green("Setting redirect flag based on Location header");

                if (cgiStatusCode == 200) {
                    cgiStatusCode = 302;
                    cgiStatusMessage = "Found";
                    //Logger::green("Using default redirect status: 302 Found");
                }
            }
        }
    }

    bool isRedirect = cgiRedirect || (ctx.error_code >= 300 && ctx.error_code < 400);

    std::string headers;

    if (cgiRedirect || cgiStatusCode != 200) {
        headers = "HTTP/1.1 " + std::to_string(cgiStatusCode) + " " + cgiStatusMessage + "\r\n";
       // Logger::green("Using CGI status code: " + std::to_string(cgiStatusCode));
    } else if (ctx.error_code > 0) {
        headers = "HTTP/1.1 " + std::to_string(ctx.error_code) + " " + ctx.error_message + "\r\n";
    } else {
        headers = "HTTP/1.1 200 OK\r\n";
    }

    if (!isRedirect && ctx.location.return_code.length() > 0) {
        int redirectCode = std::stoi(ctx.location.return_code);
        if (redirectCode >= 300 && redirectCode < 400) {
            isRedirect = true;
            headers = "HTTP/1.1 " + ctx.location.return_code + " ";

            if (redirectCode == 301) headers += "Moved Permanently";
            else if (redirectCode == 302) headers += "Found";
            else if (redirectCode == 303) headers += "See Other";
            else if (redirectCode == 307) headers += "Temporary Redirect";
            else if (redirectCode == 308) headers += "Permanent Redirect";
            else headers += "Redirect";

            headers += "\r\n";
        }
    }

    bool hasContentType = hasHeaders && (existingHeaders.find("Content-Type:") != std::string::npos ||
                                        existingHeaders.find("content-type:") != std::string::npos);
    bool hasContentLength = hasHeaders && (existingHeaders.find("Content-Length:") != std::string::npos ||
                                        existingHeaders.find("content-length:") != std::string::npos);
    bool hasServer = hasHeaders && existingHeaders.find("Server:") != std::string::npos;
    bool hasDate = hasHeaders && existingHeaders.find("Date:") != std::string::npos;
    bool hasConnection = hasHeaders && existingHeaders.find("Connection:") != std::string::npos;

    if (isRedirect && headers.find("Location:") == std::string::npos) {
        std::string redirectUrl;

        if (!cgiLocation.empty()) {
            redirectUrl = cgiLocation;
        } else if (ctx.error_code >= 300 && ctx.error_code < 400 && !ctx.error_message.empty()) {
            redirectUrl = ctx.error_message;
        } else if (!ctx.location.return_url.empty()) {
            redirectUrl = ctx.location.return_url;
        }

        if (!redirectUrl.empty()) {
            headers += "Location: " + redirectUrl + "\r\n";
        }
    }

    if (hasContentType && hasHeaders) {
        std::istringstream headerStream(existingHeaders);
        std::string line;

        while (std::getline(headerStream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.compare(0, 12, "Content-Type:") == 0 || line.compare(0, 12, "content-type:") == 0) {
                headers += line + "\r\n";
                break;
            }
        }
    } else if (!isRedirect) {
        headers += "Content-Type: text/html; charset=UTF-8\r\n";
    }

    if (!hasContentLength && hasHeaders && !isRedirect) {
        size_t bodySize = ctx.write_buffer.size() - (headerEnd + separatorSize);
        headers += "Content-Length: " + std::to_string(bodySize) + "\r\n";
    } else if (isRedirect && !hasContentLength) {
        headers += "Content-Length: 0\r\n";
    } else if (hasContentLength && hasHeaders) {
        std::istringstream headerStream(existingHeaders);
        std::string line;

        while (std::getline(headerStream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.compare(0, 14, "Content-Length:") == 0 || line.compare(0, 14, "content-length:") == 0) {
                headers += line + "\r\n";
                break;
            }
        }
    }

    if (!hasServer) {
        headers += "Server: Webserv/1.0\r\n";
    }

    if (!hasDate) {
        time_t now = time(0);
        struct tm tm;
        char dateBuffer[100];
        gmtime_r(&now, &tm);
        strftime(dateBuffer, sizeof(dateBuffer), "%a, %d %b %Y %H:%M:%S GMT", &tm);
        headers += "Date: " + std::string(dateBuffer) + "\r\n";
    }

    if (!hasConnection) {
        headers += "Connection: " + std::string(ctx.keepAlive ? "keep-alive" : "close") + "\r\n";
    }

    for (const auto& cookie : ctx.setCookies) {
        headers += "Set-Cookie: " + cookie.first + "=" + cookie.second + "\r\n";
    }

    if (hasHeaders) {
        std::istringstream headerStream(existingHeaders);
        std::string line;

        while (std::getline(headerStream, line)) {
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }

            if (line.empty() ||
                line.compare(0, 7, "Status:") == 0 || line.compare(0, 7, "status:") == 0 ||
                line.compare(0, 9, "Location:") == 0 || line.compare(0, 9, "location:") == 0 ||
                line.compare(0, 12, "Content-Type:") == 0 || line.compare(0, 12, "content-type:") == 0 ||
                line.compare(0, 14, "Content-Length:") == 0 || line.compare(0, 14, "content-length:") == 0) {
                continue;
            }

            headers += line + "\r\n";
        }
    }

    headers += "\r\n";

    std::vector<char> body;
    if (hasHeaders && headerEnd + separatorSize < ctx.write_buffer.size()) {
        body.insert(body.begin(),
                    ctx.write_buffer.begin() + headerEnd + separatorSize,
                    ctx.write_buffer.end());
    }

    if (isRedirect && cgiRedirect) {
        body.clear();
    }

    std::vector<char> newBuffer(headers.begin(), headers.end());
    if (!body.empty()) {
        newBuffer.insert(newBuffer.end(), body.begin(), body.end());
    }

    ctx.write_buffer = newBuffer;
    ctx.cgi_headers_send = true;

    return true;
}

bool Server::sendCgiResponse(Context& ctx) {
	//Logger::green("sendCgiResponse");
	if (!ctx.cgi_pipe_ready)
	{
		//Logger::yellow("waiting for pipe");
		return true;
	}

	if (ctx.write_buffer.empty() && !ctx.cgi_output_phase)
	{
		//Logger::yellow("waiting for read");
		ctx.cgi_terminated = true;
		cleanupCgiResources(ctx);

		if (ctx.keepAlive) {
			resetContext(ctx);
			return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
		}
		return false;
	}

	if (ctx.cgi_output_phase) {
		bool readComplete = checkAndReadCgiPipe(ctx);

		if (readComplete) {
			// If we had EAGAIN/EWOULDBLOCK but still need more data, go back to epoll
			if (ctx.write_buffer.empty() && !ctx.cgi_terminated) {
				// Just wait for more data from epoll, don't continue processing yet
				return true;
			}

			std::string buffer_str(ctx.write_buffer.begin(), ctx.write_buffer.end());
			bool hasLocationHeader = (buffer_str.find("Location:") != std::string::npos ||
									buffer_str.find("location:") != std::string::npos);

			if (hasLocationHeader) {
				std::string headers_end_crlf = "\r\n\r\n";
				std::string headers_end_lf = "\n\n";

				auto it_crlf = std::search(ctx.write_buffer.begin(), ctx.write_buffer.end(),
								headers_end_crlf.begin(), headers_end_crlf.end());
				auto it_lf = std::search(ctx.write_buffer.begin(), ctx.write_buffer.end(),
								headers_end_lf.begin(), headers_end_lf.end());

				if (it_crlf == ctx.write_buffer.end() && it_lf == ctx.write_buffer.end()) {
					Logger::yellow("Adding missing header ending for Location redirect");
					ctx.write_buffer.insert(ctx.write_buffer.end(), '\r');
					ctx.write_buffer.insert(ctx.write_buffer.end(), '\n');
					ctx.write_buffer.insert(ctx.write_buffer.end(), '\r');
					ctx.write_buffer.insert(ctx.write_buffer.end(), '\n');
				}
			}

			// Only move to the next phase if we actually have data or process terminated
			if (!ctx.write_buffer.empty() || ctx.cgi_terminated) {
				ctx.cgi_output_phase = false;
				return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET);
			}
			return true;
		}
	} else {
		//Logger::magenta("send sendCGIResponse");
		if(!ctx.cgi_headers_send){
			//Logger::red("header shit");
			prepareCgiHeaders(ctx);
			ctx.cgi_headers_send = true;
		}
		//Logger::cyan(std::string(ctx.write_buffer.begin(), ctx.write_buffer.end()));
		ssize_t sent = send(ctx.client_fd, &ctx.write_buffer[0], ctx.write_buffer.size(), MSG_NOSIGNAL);

		if (sent < 0) {
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET);
			}
			cleanupCgiResources(ctx);
			return false;
		} else if (sent > 0) {
			ctx.write_buffer.erase(
				ctx.write_buffer.begin(),
				ctx.write_buffer.begin() + sent
			);

			if (ctx.write_buffer.empty() && !ctx.cgi_terminated) {
				ctx.cgi_output_phase = true;
				return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET);
			}
			else if (!ctx.write_buffer.empty()) {
				return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET);
			}
		}
	}

	if (ctx.cgi_terminated) {
		if (ctx.keepAlive) {
			resetContext(ctx);
			return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
		} else {
			delFromEpoll(ctx.epoll_fd, ctx.client_fd);
		}
	}

	return true;
}

bool Server::checkAndReadCgiPipe(Context& ctx)
{
	//Logger::green("checkAndReadCgiPipe");
	if (ctx.req.cgi_out_fd <= 0) {
		return false;
	}

	char buffer[DEFAULT_CGIBUFFER_SIZE];
	ssize_t bytes_read = 0;

	//Logger::magenta("read checkAndReadCgiPipe");
	bytes_read = read(ctx.req.cgi_out_fd, buffer, sizeof(buffer));

	if (bytes_read > 0) {
		ctx.write_buffer.insert(ctx.write_buffer.end(), buffer, buffer + bytes_read);

		//Logger::yellow(std::string(ctx.write_buffer.begin(), ctx.write_buffer.end()));
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

		if (it_crlf != ctx.write_buffer.end() || it_lf != ctx.write_buffer.end()) {
			return true;
		}

		if (hasLocationHeader && bytes_read < DEFAULT_CGIBUFFER_SIZE) {
			Logger::yellow("Incomplete headers with Location - ensuring header ending");
			return true;
		}

		return false;
	}
	else if (bytes_read == 0) {
		//Logger::green("CGI process finished output");
		if (ctx.req.cgi_out_fd > 0) {
			int fd_to_remove = ctx.req.cgi_out_fd;
			epoll_ctl(ctx.epoll_fd, EPOLL_CTL_DEL, fd_to_remove, nullptr);
			close(fd_to_remove);
			globalFDS.cgi_pipe_to_client_fd.erase(fd_to_remove);
			ctx.req.cgi_out_fd = -1;
			ctx.cgi_terminated = true;
		}

		return true;  // Always return true when process finished to signal readComplete
	}
	else {
		// Handle EAGAIN/EWOULDBLOCK more gracefully
		if (errno == EAGAIN || errno == EWOULDBLOCK) {
			//Logger::red("would block");
			// Stop polling and wait for the next epoll notification
			return true;  // Return true to indicate read is "complete" for now
		}

		// For other errors, log them
		Logger::error("Error reading from CGI pipe: " + std::string(strerror(errno)));
		return true;  // Return true to avoid immediate repolling
	}
}


void Server::cleanupCgiResources(Context& ctx)
{
	if (ctx.req.cgi_out_fd > 0) {
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


bool Server::handleCgiPipeEvent(int incoming_fd)
{
	//Logger::yellow("handleCgiPipeEvent");
	auto pipe_iter = globalFDS.cgi_pipe_to_client_fd.find(incoming_fd);
	if (pipe_iter != globalFDS.cgi_pipe_to_client_fd.end()) {
		int client_fd = pipe_iter->second;

		auto ctx_iter = globalFDS.context_map.find(client_fd);
		if (ctx_iter != globalFDS.context_map.end()) {
			Context& ctx = ctx_iter->second;

			//Logger::green("CGI Pipe " + std::to_string(incoming_fd) + " ready for client " + std::to_string(client_fd));

			ctx.cgi_pipe_ready = true;
			ctx.cgi_output_phase = true;

			return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		}
	}

	return false;
}
