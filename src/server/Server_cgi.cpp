#include "Server.hpp"

bool Server::executeCgi(Context& ctx)
{
	Logger::green("executeCgi " + ctx.requested_path);

	// Validierung wie bisher...
	if (ctx.requested_path.empty() || !fileExists(ctx.requested_path) || !fileReadable(ctx.requested_path)) {
		Logger::error("CGI script validation failed: " + ctx.requested_path);
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

	if (pid == 0) { // Child process
		// Child-Prozess-Code bleibt unverändert
		// Redirect stdin to input_pipe
		dup2(input_pipe[0], STDIN_FILENO);
		// Redirect stdout to output_pipe
		dup2(output_pipe[1], STDOUT_FILENO);
		// Also redirect stderr to stdout so we can capture PHP errors
		dup2(output_pipe[1], STDERR_FILENO);

		// Close unused pipe ends
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

		logContext(ctx, "CGI");
		std::string interpreter;
		if (ctx.location.cgi_filetype == ".py") {
			interpreter = "/usr/bin/python";
		} else if (ctx.location.cgi_filetype == ".php") {
			interpreter = "/usr/bin/php-cgi"; // Make sure to use php-cgi, not php
		} else {
			// If no specific CGI filetype is matched, check if the file itself is executable
			if (fileExecutable(ctx.requested_path)) {
				interpreter = ctx.requested_path;
			}
		}

		char* args[3];
		args[0] = const_cast<char*>(interpreter.c_str());

		// For PHP-CGI, we need to pass the script path as an argument
		if (ctx.location.cgi_filetype == ".php") {
			args[1] = const_cast<char*>(ctx.requested_path.c_str());
			args[2] = nullptr;
		} else {
			// For other interpreters or executable scripts, we might not need an additional argument
			args[1] = nullptr;
		}

		execve(interpreter.c_str(), args, env_pointers.data());
		// Falls execve fehlschlägt, mit einem Fehler beenden
		exit(1);
	}

	// Parent process
	// Close unused pipe ends
	close(input_pipe[0]);
	close(output_pipe[1]);

	ctx.req.cgi_in_fd = input_pipe[1];
	ctx.req.cgi_out_fd = output_pipe[0];
	ctx.req.cgi_pid = pid;
	ctx.req.state = RequestBody::STATE_CGI_RUNNING;

	ctx.cgi_pipe_ready = false;
	ctx.cgi_read_attempts = 0;
	ctx.cgi_start_time = std::chrono::steady_clock::now();

	// Speichere Zuordnungen in den globalen Maps
	globalFDS.cgi_pipe_to_client_fd[ctx.req.cgi_out_fd] = ctx.client_fd;
	globalFDS.cgi_pid_to_client_fd[ctx.req.cgi_pid] = ctx.client_fd;

	// NEUES FEATURE: CGI output pipe zum epoll-Set hinzufügen
	struct epoll_event ev;
	std::memset(&ev, 0, sizeof(ev)); // Um Speicherprobleme zu vermeiden
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = ctx.req.cgi_out_fd;
	if (epoll_ctl(ctx.epoll_fd, EPOLL_CTL_ADD, ctx.req.cgi_out_fd, &ev) < 0) {
		Logger::error("Failed to add CGI output pipe to epoll: " + std::string(strerror(errno)));
		// Wir machen hier weiter, auch wenn die epoll-Registrierung fehlgeschlagen ist
		// Der normale non-blocking Poll-Mechanismus wird als Fallback funktionieren
	} else {
		Logger::green("Added CGI output pipe " + std::to_string(ctx.req.cgi_out_fd) + " to epoll");
	}

	// Write POST data to the CGI input if it exists
	if (!ctx.body.empty()) {
		// For PHP, make sure we're writing properly formatted POST data
		write(ctx.req.cgi_in_fd, ctx.body.c_str(), ctx.body.length());
	}

	// Close the input pipe after writing all data
	close(ctx.req.cgi_in_fd);
	ctx.req.cgi_in_fd = -1;

	ctx.last_activity = std::chrono::steady_clock::now();

	ctx.cgi_executed = true;
	return (modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET));
}


std::vector<std::string> Server::prepareCgiEnvironment(const Context& ctx) {
	std::vector<std::string> env;

	// Validate that we have a valid script path
	if (ctx.requested_path.empty()) {
		Logger::error("Empty script path for CGI environment");
		return env; // Return empty environment if path is invalid
	}

	// Standard CGI environment variables
	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("SERVER_PROTOCOL=" + ctx.version);
	env.push_back("SERVER_SOFTWARE=Webserv/1.0");
	env.push_back("SERVER_NAME=" + ctx.name);
	env.push_back("SERVER_PORT=" + std::to_string(ctx.port));
	env.push_back("REQUEST_METHOD=" + ctx.method);

	// Extract the path without query string for PATH_INFO
	std::string pathWithoutQuery = ctx.path;
	size_t queryPos = pathWithoutQuery.find('?');
	if (queryPos != std::string::npos) {
		pathWithoutQuery = pathWithoutQuery.substr(0, queryPos);
	}

	// Determine DOCUMENT_ROOT (we'll use the server root or location root if available)
	std::string docRoot = ctx.location.root.empty() ? ctx.root : ctx.location.root;
	if (!docRoot.empty()) {
		env.push_back("DOCUMENT_ROOT=" + docRoot);
	}

	// Determine the script path relative to document root
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

	// Extract query string properly
	std::string queryString = extractQueryString(ctx.path);
	env.push_back("QUERY_STRING=" + queryString);

	// Critical for PHP-CGI
	env.push_back("REDIRECT_STATUS=200");
	env.push_back("REQUEST_URI=" + ctx.path);


	// For PHP POST requests, these are critical
	if (ctx.method == "POST") {
		// Set content type directly (not as HTTP_ header)
		std::string contentType = "application/x-www-form-urlencoded";
		if (ctx.headers.count("Content-Type")) {
			contentType = ctx.headers.at("Content-Type");
		}
		env.push_back("CONTENT_TYPE=" + contentType);

		// Set content length directly (not as HTTP_ header)
		std::string contentLength = std::to_string(ctx.body.length());
		if (ctx.headers.count("Content-Length")) {
			contentLength = ctx.headers.at("Content-Length");
		}
		env.push_back("CONTENT_LENGTH=" + contentLength);
	}

	// Process other HTTP headers (excluding the ones we already handled specially)
	for (const auto& header : ctx.headers) {
		// Skip content-type and content-length since we handled those specially
		if (header.first == "Content-Type" || header.first == "Content-Length") {
			continue;
		}

		std::string name = "HTTP_" + header.first;
		// Convert to uppercase and replace dashes with underscores
		std::transform(name.begin(), name.end(), name.begin(),
			[](unsigned char c) { return std::toupper(c); });
		std::replace(name.begin(), name.end(), '-', '_');

		env.push_back(name + "=" + header.second);
	}

	// Handle cookies - properly format them as per CGI spec
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
	// Find the header-body separator in a vector<char>
	std::string separator = "\r\n\r\n";
	std::string separator_alt = "\n\n"; // Alternativer Separator für Unix-Style Zeilenumbrüche

	auto it = std::search(ctx.write_buffer.begin(), ctx.write_buffer.end(),
						separator.begin(), separator.end());

	// Falls \r\n\r\n nicht gefunden wurde, suche nach \n\n
	if (it == ctx.write_buffer.end()) {
		it = std::search(ctx.write_buffer.begin(), ctx.write_buffer.end(),
						separator_alt.begin(), separator_alt.end());
	}

	bool hasHeaders = (it != ctx.write_buffer.end());
	size_t headerEnd = hasHeaders ? std::distance(ctx.write_buffer.begin(), it) : 0;
	size_t separatorSize = (it != ctx.write_buffer.end() &&
						std::string(it, it + 4) == "\r\n\r\n") ? 4 : 2;

	// Extract existing headers if present
	std::string existingHeaders;
	if (hasHeaders) {
		existingHeaders = std::string(ctx.write_buffer.begin(), it);
	}

	// Parse CGI headers to look for Status and Location
	int cgiStatusCode = 200;
	std::string cgiStatusMessage = "OK";
	std::string cgiLocation;
	bool cgiRedirect = false;

	if (hasHeaders) {
		std::istringstream headerStream(existingHeaders);
		std::string line;

		while (std::getline(headerStream, line)) {
			// Entferne abschließendes \r falls vorhanden
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}

			// Überspringe leere Zeilen
			if (line.empty()) {
				continue;
			}

			// Suche nach Status-Header
			if (line.compare(0, 7, "Status:") == 0 || line.compare(0, 7, "status:") == 0) {
				std::string statusLine = line.substr(7);
				std::stringstream ss(statusLine);
				ss >> cgiStatusCode;

				// Extrahiere Status-Message wenn vorhanden
				if (ss.peek() == ' ') {
					ss.ignore();
					std::getline(ss, cgiStatusMessage);
				}

				// Prüfe, ob es sich um einen Redirect handelt
				if (cgiStatusCode >= 300 && cgiStatusCode < 400) {
					cgiRedirect = true;
				}
			}

			// Suche nach Location-Header
			if (line.compare(0, 9, "Location:") == 0 || line.compare(0, 9, "location:") == 0) {
				cgiLocation = line.substr(9);
				while (!cgiLocation.empty() && cgiLocation[0] == ' ') {
					cgiLocation.erase(0, 1);
				}

				// Wenn Location-Header vorhanden ist, handelt es sich vermutlich um einen Redirect
				cgiRedirect = true;

				// Wenn kein spezifischer Status-Code gesetzt wurde, verwende 302 Found als Standard
				if (cgiStatusCode == 200) {
					cgiStatusCode = 302;
					cgiStatusMessage = "Found";
				}
			}
		}
	}

	// Check if this is a redirect response from status code or CGI
	bool isRedirect = cgiRedirect || (ctx.error_code >= 300 && ctx.error_code < 400);

	// Prepare basic response header with appropriate status code
	std::string headers;

	// Priorität: CGI-Status > ctx.error_code > default 200
	if (cgiRedirect || cgiStatusCode != 200) {
		headers = "HTTP/1.1 " + std::to_string(cgiStatusCode) + " " + cgiStatusMessage + "\r\n";
	} else if (ctx.error_code > 0) {
		headers = "HTTP/1.1 " + std::to_string(ctx.error_code) + " " + ctx.error_message + "\r\n";
	} else {
		headers = "HTTP/1.1 200 OK\r\n";
	}

	// Check if we need to handle redirect from location block
	if (!isRedirect && ctx.location.return_code.length() > 0) {
		int redirectCode = std::stoi(ctx.location.return_code);
		if (redirectCode >= 300 && redirectCode < 400) {
			isRedirect = true;
			headers = "HTTP/1.1 " + ctx.location.return_code + " ";

			// Map common redirect status codes to their messages
			if (redirectCode == 301) headers += "Moved Permanently";
			else if (redirectCode == 302) headers += "Found";
			else if (redirectCode == 303) headers += "See Other";
			else if (redirectCode == 307) headers += "Temporary Redirect";
			else if (redirectCode == 308) headers += "Permanent Redirect";
			else headers += "Redirect";

			headers += "\r\n";
		}
	}

	// Check which headers already exist in CGI output that we want to preserve
	bool hasContentType = hasHeaders && (existingHeaders.find("Content-Type:") != std::string::npos ||
										existingHeaders.find("content-type:") != std::string::npos);
	bool hasContentLength = hasHeaders && (existingHeaders.find("Content-Length:") != std::string::npos ||
										existingHeaders.find("content-length:") != std::string::npos);
	bool hasServer = hasHeaders && existingHeaders.find("Server:") != std::string::npos;
	bool hasDate = hasHeaders && existingHeaders.find("Date:") != std::string::npos;
	bool hasConnection = hasHeaders && existingHeaders.find("Connection:") != std::string::npos;

	// Add Location header for redirects
	if (isRedirect && headers.find("Location:") == std::string::npos) {
		std::string redirectUrl;

		// Priorität: CGI-Location > ctx.error_message > location.return_url
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

	// Übernehme Content-Type aus CGI-Antwort, falls vorhanden
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
		// Standardwert für Content-Type, falls nicht in CGI-Antwort
		headers += "Content-Type: text/html; charset=UTF-8\r\n";
	}

	// Add Content-Length if not present and we can calculate it
	if (!hasContentLength && hasHeaders && !isRedirect) {
		size_t bodySize = ctx.write_buffer.size() - (headerEnd + separatorSize);
		headers += "Content-Length: " + std::to_string(bodySize) + "\r\n";
	} else if (isRedirect && !hasContentLength) {
		// For redirects, we might want a minimal or empty body
		headers += "Content-Length: 0\r\n";
	} else if (hasContentLength && hasHeaders) {
		// Übernehme Content-Length aus CGI-Antwort, falls vorhanden
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

	// Add Server header if not present
	if (!hasServer) {
		headers += "Server: Webserv/1.0\r\n";
	}

	// Add Date header if not present
	if (!hasDate) {
		time_t now = time(0);
		struct tm tm;
		char dateBuffer[100];
		gmtime_r(&now, &tm);
		strftime(dateBuffer, sizeof(dateBuffer), "%a, %d %b %Y %H:%M:%S GMT", &tm);
		headers += "Date: " + std::string(dateBuffer) + "\r\n";
	}

	// Add Connection header based on keepAlive flag
	if (!hasConnection) {
		headers += "Connection: " + std::string(ctx.keepAlive ? "keep-alive" : "close") + "\r\n";
	}

	// Add any Set-Cookie headers
	for (const auto& cookie : ctx.setCookies) {
		headers += "Set-Cookie: " + cookie.first + "=" + cookie.second + "\r\n";
	}

	// Übernehme alle anderen wichtigen Header aus der CGI-Antwort
	if (hasHeaders) {
		std::istringstream headerStream(existingHeaders);
		std::string line;

		while (std::getline(headerStream, line)) {
			if (!line.empty() && line.back() == '\r') {
				line.pop_back();
			}

			// Überspringe bereits verarbeitete oder leere Header
			if (line.empty() ||
				line.compare(0, 7, "Status:") == 0 || line.compare(0, 7, "status:") == 0 ||
				line.compare(0, 9, "Location:") == 0 || line.compare(0, 9, "location:") == 0 ||
				line.compare(0, 12, "Content-Type:") == 0 || line.compare(0, 12, "content-type:") == 0 ||
				line.compare(0, 14, "Content-Length:") == 0 || line.compare(0, 14, "content-length:") == 0) {
				continue;
			}

			// Übernehme andere Header wie Set-Cookie, Cache-Control, etc.
			headers += line + "\r\n";
		}
	}

	// Finalize headers
	headers += "\r\n"; // Headers end

	// Extrahiere den Body aus der CGI-Antwort, falls vorhanden
	std::vector<char> body;
	if (hasHeaders && headerEnd + separatorSize < ctx.write_buffer.size()) {
		body.insert(body.begin(),
					ctx.write_buffer.begin() + headerEnd + separatorSize,
					ctx.write_buffer.end());
	}

	// Bei Redirects kann der Body leer sein
	if (isRedirect && cgiRedirect) {
		body.clear();
	}

	// Erstelle den neuen Buffer mit HTTP-Headern und Body
	std::vector<char> newBuffer(headers.begin(), headers.end());
	if (!body.empty()) {
		newBuffer.insert(newBuffer.end(), body.begin(), body.end());
	}

	// Ersetze den alten Buffer
	ctx.write_buffer = newBuffer;
	ctx.cgi_headers_send = true;

	return true;
}

bool Server::sendCgiResponse(Context& ctx) {
    Logger::green("sendCgiResponse");
    if (!ctx.cgi_pipe_ready)
    {
        Logger::yellow("waiting for pipe");
        return true;
    }

    if (ctx.write_buffer.empty() && !ctx.cgi_output_phase)
    {
        Logger::yellow("waiting for read");
        ctx.cgi_terminated = true;
        cleanupCgiResources(ctx);

        if (ctx.keepAlive) {
            resetContext(ctx);
            return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
        }
        return false;
    }

    // Nur entweder lesen ODER schreiben in einem Epoll-Event
    if (ctx.cgi_output_phase) {
        // CGI-Output einlesen
        if (checkAndReadCgiPipe(ctx)) {
            // Nach dem Lesen für das Senden vorbereiten
            ctx.cgi_output_phase = false;
            // Epoll erneut für OUT events registrieren
            return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET);
        }
    } else {
        // Daten an Client senden
        Logger::magenta("send sendCGIResponse");
        if(!ctx.cgi_headers_send){
            Logger::red("header shit");
            ctx.cgi_headers_send = true;
            prepareCgiHeaders(ctx);
        }
        Logger::cyan(std::string(ctx.write_buffer.begin(), ctx.write_buffer.end()));
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

            // Wenn wir alles gesendet haben und der CGI-Prozess noch läuft,
            // setzen wir cgi_output_phase auf true und registrieren für OUT-Event
            if (ctx.write_buffer.empty() && !ctx.cgi_terminated) {
                ctx.cgi_output_phase = true;
                return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET);
            }
            // Wenn noch Daten zum Senden vorhanden sind, erneut für OUT events registrieren
            else if (!ctx.write_buffer.empty()) {
                return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET);
            }
        }
    }

    Logger::yellow(std::to_string(ctx.cgi_terminated));
    if (ctx.cgi_terminated)
        delFromEpoll(ctx.epoll_fd, ctx.client_fd);

    return true;
}

bool Server::checkAndReadCgiPipe(Context& ctx) {
    Logger::green("checkAndReadCgiPipe");
    if (ctx.req.cgi_out_fd <= 0) {
        return false;
    }

    char buffer[DEFAULT_REQUESTBUFFER_SIZE];
    ssize_t bytes_read = 0;

    // Non-blocking read from CGI output pipe
    Logger::magenta("read checkAndReadCgiPipe");
    bytes_read = read(ctx.req.cgi_out_fd, buffer, sizeof(buffer));

    if (bytes_read > 0) {
        ctx.write_buffer.insert(ctx.write_buffer.end(), buffer, buffer + bytes_read);

        Logger::yellow(std::string(ctx.write_buffer.begin(), ctx.write_buffer.end()));
        ctx.last_activity = std::chrono::steady_clock::now();
        return true;
    }
    else if (bytes_read == 0) {
        Logger::green("CGI process finished output");
        if (ctx.req.cgi_out_fd > 0) {
            int fd_to_remove = ctx.req.cgi_out_fd;
            epoll_ctl(ctx.epoll_fd, EPOLL_CTL_DEL, fd_to_remove, nullptr);
            close(fd_to_remove);
            globalFDS.cgi_pipe_to_client_fd.erase(fd_to_remove);
            ctx.req.cgi_out_fd = -1;
            ctx.cgi_terminated = true;
        }
        return true;  // Hier true zurückgeben, da wir das Ende des Prozesses erkannt haben
    }
    else {
        // EAGAIN/EWOULDBLOCK bedeutet, dass keine Daten verfügbar sind
        Logger::red("would block");
        return false;
    }
}


void Server::cleanupCgiResources(Context& ctx) {
	if (ctx.req.cgi_out_fd > 0) {
		epoll_ctl(ctx.epoll_fd, EPOLL_CTL_DEL, ctx.req.cgi_out_fd, nullptr);
		globalFDS.cgi_pipe_to_client_fd.erase(ctx.req.cgi_out_fd);
		close(ctx.req.cgi_out_fd);
		ctx.req.cgi_out_fd = -1;
	}

	globalFDS.cgi_pid_to_client_fd.erase(ctx.req.cgi_pid);

	ctx.cgi_terminate = true;
	ctx.cgi_terminated = true;
}


bool Server::handleCgiPipeEvent(int incoming_fd) {
	Logger::yellow("handleCgiPipeEvent");
	auto pipe_iter = globalFDS.cgi_pipe_to_client_fd.find(incoming_fd);
	if (pipe_iter != globalFDS.cgi_pipe_to_client_fd.end()) {
		int client_fd = pipe_iter->second;

		auto ctx_iter = globalFDS.context_map.find(client_fd);
		if (ctx_iter != globalFDS.context_map.end()) {
			Context& ctx = ctx_iter->second;

			Logger::green("CGI Pipe " + std::to_string(incoming_fd) + " ready for client " + std::to_string(client_fd));

			ctx.cgi_pipe_ready = true;
			ctx.cgi_output_phase = true;

			return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		}
	}

	return false;
}