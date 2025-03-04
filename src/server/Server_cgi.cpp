#include "Server.hpp"

bool Server::executeCgi(Context& ctx) {
	Logger::green("executeCgi");
	Logger::file("executeCgi started");

	int input_pipe[2];
	int output_pipe[2];

	if (pipe(input_pipe) < 0 || pipe(output_pipe) < 0) {
		Logger::error("Failed to create pipes for CGI");
		Logger::file("Failed to create pipes for CGI");
		ctx.error_code = 500;
		return false;
	}

	pid_t pid = fork();

	if (pid < 0) {
		Logger::error("Failed to fork for CGI execution");
		Logger::file("Failed to fork for CGI execution");
		close(input_pipe[0]);
		close(input_pipe[1]);
		close(output_pipe[0]);
		close(output_pipe[1]);
		ctx.error_code = 500;
		return false;
	}

	if (pid == 0) {
		// Redirect Stdin to input_pipe
		dup2(input_pipe[0], STDIN_FILENO);
		// Redirect stdout to output_pipe
		dup2(output_pipe[1], STDOUT_FILENO);

		// Close Unused Pipe Ends
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
			interpreter = "/usr/bin/php";
		} else {
			interpreter = ctx.requested_path;
		}

		char* args[] = {
			const_cast<char*>(interpreter.c_str()),
			const_cast<char*>(ctx.requested_path.c_str()),
			nullptr
		};

		Logger::file("Executing CGI script: " + ctx.requested_path + " with interpreter: " + interpreter);
		execve(interpreter.c_str(), args, env_pointers.data());

		Logger::file("execve failed for CGI execution");
		exit(1);
	}

	// Close Unused Pipe Ends
	close(input_pipe[0]);
	close(output_pipe[1]);

	ctx.req.cgi_in_fd = input_pipe[1];
	ctx.req.cgi_out_fd = output_pipe[0];
	ctx.req.cgi_pid = pid;
	ctx.req.state = RequestBody::STATE_CGI_RUNNING;

	Logger::file("CGI process started with PID: " + std::to_string(pid));

	if (!ctx.body.empty()) {
		write(ctx.req.cgi_in_fd, ctx.body.c_str(), ctx.body.length());
		Logger::file("CGI input written: " + std::to_string(ctx.body.length()) + " bytes");
	}

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

	env.push_back("GATEWAY_INTERFACE=CGI/1.1");
	env.push_back("SERVER_PROTOCOL=" + ctx.version);
	env.push_back("SERVER_SOFTWARE=Webserv/1.0");
	env.push_back("SERVER_NAME=" + ctx.name);
	env.push_back("SERVER_PORT=" + std::to_string(ctx.port));
	env.push_back("REQUEST_METHOD=" + ctx.method);
	env.push_back("PATH_INFO=" + ctx.path);
	env.push_back("SCRIPT_NAME=" + ctx.path);
	env.push_back("SCRIPT_FILENAME=" + ctx.root + ctx.path);
	env.push_back("QUERY_STRING=" + extractQueryString(ctx.path));
// env.push_Back ("remote_addr = 127.0.0.1");

	for (const auto& header : ctx.headers) {
		std::string name = "HTTP_" + header.first;
		std::transform(name.begin(), name.end(), name.begin(), ::toupper);
		std::replace(name.begin(), name.end(), '-', '_');
		env.push_back(name + "=" + header.second);
	}

	if (ctx.headers.count("Content-Type")) {
		env.push_back("CONTENT_TYPE=" + ctx.headers.at("Content-Type"));
	}

	if (ctx.content_length > 0) {
		env.push_back("CONTENT_LENGTH=" + std::to_string(ctx.content_length));
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

std::string Server::extractQueryString(const std::string& path) {
	size_t pos = path.find('?');
	if (pos != std::string::npos) {
		return path.substr(pos + 1);
	}
	return "";
}

bool Server::sendCgiResponse(Context& ctx) {
	Logger::yellow("sendCgiResponse - buffer size: " + std::to_string(ctx.write_buffer.size()));
	Logger::yellow(std::to_string(ctx.cgi_output_phase));
	std::stringstream response;

	if (ctx.req.cgi_out_fd < 0 && ctx.cgi_terminated)
	{
		Logger::red("warum diese raus gehen");
		Logger::green("prosieben die kanal in die fernsieher");
		return true;
	}

	ctx.last_activity = std::chrono::steady_clock::now();

	if (ctx.cgi_output_phase)
	{
		if (!ctx.cgi_headers_send)
		{
			Logger::magenta("read cgi headers prep");
			response << "HTTP/1.1 200 OK\r\n"
					<< "Content-Type: " << "text/html" << "\r\n"
					//<< "Content-Length: " << ctx.req.expected_body_length << "\r\n"
					//<< "Content-Disposition: attachment; filen
					//ame=\"" << filename << "\"\r\n"
					<< "Connection: " << (ctx.keepAlive ? "keep-alive" : "close") << "\r\n";

			for (const auto& cookiePair : ctx.setCookies) {
				Cookie cookie;
				cookie.name = cookiePair.first;
				cookie.value = cookiePair.second;
				cookie.path = "/";
				response << generateSetCookieHeader(cookie) << "\r\n";
			}

			response << "\r\n";
			ctx.cgi_headers_send = true;
		}
		Logger::magenta("read cgi pipe");
		ctx.cgi_output_phase = false;
		char buffer[DEFAULT_REQUESTBUFFER_SIZE];
		std::memset(buffer, 0, sizeof(buffer));
		ssize_t bytes = read(ctx.req.cgi_out_fd, buffer, sizeof(buffer));
		if (bytes < 0) {
			if (ctx.req.cgi_out_fd > 0) {
				close(ctx.req.cgi_out_fd);
				ctx.req.cgi_out_fd = -1;
				ctx.write_buffer.clear();
			}
			return updateErrorStatus(ctx, 500, "Internal Server Error Wediellll");
		}

		if (bytes == 0) {
			Logger::yellow("finished cgi output");
			close(ctx.req.cgi_out_fd);
			ctx.req.cgi_out_fd = -1;
			Logger::blue("keepAlive: " + std::to_string(ctx.keepAlive));
			Logger::blue("cgi_terminate: " + std::to_string(ctx.cgi_terminate));
			Logger::blue("cgi_terminated: " + std::to_string(ctx.cgi_terminated));
			const char* end_marker = "\r\n\r\n";
			ctx.write_buffer.clear();
			ctx.write_buffer.insert(ctx.write_buffer.end(), end_marker, end_marker + 4);
			ctx.cgi_terminate = true;
			Logger::blue("cgi_terminate: " + std::to_string(ctx.cgi_terminate));
			modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
			return true;
		}
		if (ctx.cgi_terminated)
		{
			if (ctx.keepAlive) {
				resetContext(ctx);
				modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLIN | EPOLLET);
			} else if (ctx.cgi_terminated) {
				delFromEpoll(ctx.epoll_fd, ctx.client_fd);
			}
			return true;
		}
		// Fixed: Convert the stringstream to a string and copy its contents
		std::string responseStr = response.str();
		ctx.write_buffer.clear();
		ctx.write_buffer.insert(ctx.write_buffer.end(), responseStr.begin(), responseStr.end());
		// Fixed: Use insert instead of append
		ctx.write_buffer.insert(ctx.write_buffer.end(), buffer, buffer + bytes);

		Logger::cyan(std::string(ctx.write_buffer.begin(), ctx.write_buffer.end()));
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		return true;
	}
	else
	{
		Logger::magenta("write cgi buffer");
		//Logger::magenta("sending chunk to client");
		if (send(ctx.client_fd, ctx.write_buffer.data(), ctx.write_buffer.size(), MSG_NOSIGNAL) < 0) {
			close(ctx.req.cgi_out_fd);
			ctx.req.cgi_out_fd = -1;
			return updateErrorStatus(ctx, 500, "Failed to send file content");
		}
		ctx.req.current_body_length += ctx.write_buffer.size();
		ctx.write_buffer.clear();
		ctx.cgi_output_phase = true;
		if(ctx.cgi_terminate)
			ctx.cgi_terminated = true;
		Logger::red("keepAlive: " + std::to_string(ctx.keepAlive));
		Logger::red("cgi_terminate: " + std::to_string(ctx.cgi_terminate));
		Logger::red("cgi_terminated: " + std::to_string(ctx.cgi_terminated));
		modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		return true;
	}

	return false;
}