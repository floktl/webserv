#include "Server.hpp"

bool Server::executeCgi(Context& ctx) {
	Logger::green("executeCgi");
	Logger::file("executeCgi started");

	// Validate that we have a valid script path before proceeding
	if (ctx.requested_path.empty()) {
		Logger::error("Empty script path for CGI execution");
		Logger::file("Empty script path for CGI execution");
		return updateErrorStatus(ctx, 404, "Not Found - Empty CGI Path");
	}

	// Check if the requested file exists and is accessible
	if (!fileExists(ctx.requested_path)) {
		Logger::error("CGI script not found: " + ctx.requested_path);
		Logger::file("CGI script not found: " + ctx.requested_path);
		return updateErrorStatus(ctx, 404, "Not Found - CGI Script");
	}

	if (!fileReadable(ctx.requested_path)) {
		Logger::error("CGI script not readable: " + ctx.requested_path);
		Logger::file("CGI script not readable: " + ctx.requested_path);
		return updateErrorStatus(ctx, 403, "Forbidden - Cannot read CGI script");
	}

	int input_pipe[2];
	int output_pipe[2];

	if (pipe(input_pipe) < 0 || pipe(output_pipe) < 0) {
		Logger::error("Failed to create pipes for CGI");
		Logger::file("Failed to create pipes for CGI");
		return updateErrorStatus(ctx, 500, "Internal Server Error - Pipe Creation Failed");
	}

	// Set pipes to non-blocking mode
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
			} else {
				Logger::file("No valid interpreter for CGI script: " + ctx.requested_path);
				exit(1);
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

		Logger::file("Executing CGI script: " + ctx.requested_path + " with interpreter: " + interpreter);
		execve(interpreter.c_str(), args, env_pointers.data());

		// If we get here, execve failed
		Logger::file("execve failed for CGI execution: " + std::string(strerror(errno)));
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

	Logger::file("CGI process started with PID: " + std::to_string(pid));

	// Write POST data to the CGI input if it exists
	if (!ctx.body.empty()) {
		// For PHP, make sure we're writing properly formatted POST data
		ssize_t written = write(ctx.req.cgi_in_fd, ctx.body.c_str(), ctx.body.length());
		if (written < 0) {
			Logger::file("Failed to write to CGI input pipe: " + std::string(strerror(errno)));
		} else {
			Logger::file("CGI input written: " + std::to_string(written) + " bytes");
		}
	}

	// Close the input pipe after writing all data
	close(ctx.req.cgi_in_fd);
	ctx.req.cgi_in_fd = -1;

	ctx.last_activity = std::chrono::steady_clock::now();
	Logger::yellow("executeCgi completed successfully");

	ctx.cgi_executed = true;
	modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET);
	return true;
}


std::vector<std::string> Server::prepareCgiEnvironment(const Context& ctx) {
	std::vector<std::string> env;

	// Validate that we have a valid script path
	if (ctx.requested_path.empty()) {
		Logger::error("Empty script path for CGI environment");
		Logger::file("Empty script path for CGI environment");
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
	Logger::file("SCRIPT_FILENAME=" + ctx.requested_path);

	// Extract query string properly
	std::string queryString = extractQueryString(ctx.path);
	env.push_back("QUERY_STRING=" + queryString);

	// Critical for PHP-CGI
	env.push_back("REDIRECT_STATUS=200");
	env.push_back("REQUEST_URI=" + ctx.path);

	Logger::file("REQUEST_METHOD=" + ctx.method);

	// For PHP POST requests, these are critical
	if (ctx.method == "POST") {
		// Set content type directly (not as HTTP_ header)
		std::string contentType = "application/x-www-form-urlencoded";
		if (ctx.headers.count("Content-Type")) {
			contentType = ctx.headers.at("Content-Type");
		}
		env.push_back("CONTENT_TYPE=" + contentType);
		Logger::file("CONTENT_TYPE=" + contentType);

		// Set content length directly (not as HTTP_ header)
		std::string contentLength = std::to_string(ctx.body.length());
		if (ctx.headers.count("Content-Length")) {
			contentLength = ctx.headers.at("Content-Length");
		}
		env.push_back("CONTENT_LENGTH=" + contentLength);
		Logger::file("CONTENT_LENGTH=" + contentLength);
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

	// Debug output for environment variables
	for (const auto& var : env) {
		Logger::file("CGI ENV: " + var);
	}

	return env;
}

std::string Server::extractQueryString(const std::string& path) {
	size_t pos = path.find('?');
	if (pos != std::string::npos) {
		return path.substr(pos + 1);
	}
	return "";
}

bool Server::sendCgiResponse(Context& ctx) {
	// Handle case where the CGI process has been terminated
	if (ctx.req.cgi_out_fd < 0 && ctx.cgi_terminated) {
		Logger::file("CGI process already terminated, cleaning up");
		return true;
	}

	ctx.last_activity = std::chrono::steady_clock::now();

	if (ctx.cgi_output_phase) {
		// First phase: Read from CGI output
		if (!ctx.cgi_headers_send) {
			Logger::file("Preparing CGI response headers");
			std::stringstream response;

			// We'll start with our own HTTP headers
			response << "HTTP/1.1 200 OK\r\n";

			// Add content type if not specified by the CGI script
			// We'll use text/html as default for PHP scripts
			response << "Content-Type: text/html\r\n";

			// Add connection header based on keep-alive setting
			response << "Connection: " << (ctx.keepAlive ? "keep-alive" : "close") << "\r\n";

			// Add any session cookies
			for (const auto& cookiePair : ctx.setCookies) {
				Cookie cookie;
				cookie.name = cookiePair.first;
				cookie.value = cookiePair.second;
				cookie.path = "/";
				response << generateSetCookieHeader(cookie) << "\r\n";
			}

			// End headers section
			response << "\r\n";
			ctx.cgi_headers_send = true;

			// Convert headers to string and store in the write buffer
			std::string responseStr = response.str();
			ctx.write_buffer.assign(responseStr.begin(), responseStr.end());
		}

		Logger::file("Reading from CGI output pipe");
		ctx.cgi_output_phase = false;
		char buffer[DEFAULT_REQUESTBUFFER_SIZE];
		std::memset(buffer, 0, sizeof(buffer));

		ssize_t bytes = read(ctx.req.cgi_out_fd, buffer, sizeof(buffer));

		if (bytes < 0) {
			// Handle read error
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				// No data available right now, try again later
				Logger::file("No data available from CGI, will retry");
				ctx.cgi_output_phase = true;
				modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
				return true;
			}

			// Some other error occurred
			Logger::error("Failed to read from CGI output: " + std::string(strerror(errno)));
			if (ctx.req.cgi_out_fd > 0) {
				close(ctx.req.cgi_out_fd);
				ctx.req.cgi_out_fd = -1;
			}
			return updateErrorStatus(ctx, 500, "Internal Server Error - CGI Read Failed");
		}

		if (bytes == 0) {
			// End of CGI output
			Logger::file("End of CGI output, closing pipe");
			close(ctx.req.cgi_out_fd);
			ctx.req.cgi_out_fd = -1;

			// If we haven't sent any data yet, add a marker to show we're done
			if (ctx.write_buffer.empty()) {
				const char* end_marker = "\r\n\r\n"; // Empty response
				ctx.write_buffer.assign(end_marker, end_marker + 4);
			}

			ctx.cgi_terminate = true;
			Logger::file("CGI output complete, terminal flag set: " + std::to_string(ctx.cgi_terminate));
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
			return true;
		}

		if (ctx.cgi_terminated) {
			// We've already sent all the data, process is terminated
			if (ctx.keepAlive) {
				resetContext(ctx);
				modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
			} else {
				delFromEpoll(ctx.epoll_fd, ctx.client_fd);
			}
			return true;
		}

		// Append new data to existing buffer
		ctx.write_buffer.insert(ctx.write_buffer.end(), buffer, buffer + bytes);

		Logger::file("Read " + std::to_string(bytes) + " bytes from CGI output");
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		return true;
	}
	else {
		// Second phase: Send data to client
		if (ctx.write_buffer.empty()) {
			// No data to send, go back to reading phase
			ctx.cgi_output_phase = true;
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
			return true;
		}

		// Try to send all the data
		ssize_t sent = send(ctx.client_fd, ctx.write_buffer.data(), ctx.write_buffer.size(), MSG_NOSIGNAL);

		if (sent < 0) {
			// Handle send error
			if (errno == EAGAIN || errno == EWOULDBLOCK) {
				// Would block, try again later
				Logger::file("Send would block, will retry");
				modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
				return true;
			}

			// Some other error occurred
			Logger::error("Failed to send CGI response: " + std::string(strerror(errno)));
			if (ctx.req.cgi_out_fd > 0) {
				close(ctx.req.cgi_out_fd);
				ctx.req.cgi_out_fd = -1;
			}
			return updateErrorStatus(ctx, 500, "Failed to send CGI response");
		}

		// Update our tracking of how much we've sent
		ctx.req.current_body_length += sent;

		// Remove the data we've sent from the buffer
		if (static_cast<size_t>(sent) < ctx.write_buffer.size()) {
			// We sent some but not all data, keep remaining data in buffer
			ctx.write_buffer.erase(ctx.write_buffer.begin(), ctx.write_buffer.begin() + sent);
			// Continue in send phase
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
			return true;
		}

		// All data sent, clear buffer
		ctx.write_buffer.clear();

		// If we're done with the CGI process, mark as terminated
		if (ctx.cgi_terminate) {
			ctx.cgi_terminated = true;
		}

		// Go back to reading phase
		ctx.cgi_output_phase = true;
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		return true;
	}

	return false;
}