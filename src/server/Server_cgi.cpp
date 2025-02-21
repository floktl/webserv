#include "Server.hpp"

bool Server::executeCgi(Context& ctx) {
	Logger::green("executeCgi");

	int input_pipe[2];
	int output_pipe[2];

	if (pipe(input_pipe) < 0 || pipe(output_pipe) < 0) {
		Logger::error("Failed to create pipes for CGI");
		ctx.error_code = 500;
		return false;
	}

	pid_t pid = fork();

	if (pid < 0) {
		Logger::error("Failed to fork for CGI execution");
		close(input_pipe[0]);
		close(input_pipe[1]);
		close(output_pipe[0]);
		close(output_pipe[1]);
		ctx.error_code = 500;
		return false;
	}

	if (pid == 0) {
		// Redirect stdin to input_pipe
		dup2(input_pipe[0], STDIN_FILENO);
		// Redirect stdout to output_pipe
		dup2(output_pipe[1], STDOUT_FILENO);

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

		std::string script_path = ctx.root + ctx.path;
		std::string interpreter;
		if (ctx.location.cgi_filetype == ".py") {
			interpreter = "/usr/bin/python";
		} else if (ctx.location.cgi_filetype == ".php") {
			interpreter = "/usr/bin/php";
		} else {
			interpreter = script_path;
		}

		char* args[] = {
			const_cast<char*>(interpreter.c_str()),
			const_cast<char*>(script_path.c_str()),
			nullptr
		};

		execve(interpreter.c_str(), args, env_pointers.data());

		Logger::error("execve failed for CGI execution");
		exit(1);
	}

	// Close unused pipe ends
	close(input_pipe[0]);
	close(output_pipe[1]);

	ctx.req.cgi_in_fd = input_pipe[1];
	ctx.req.cgi_out_fd = output_pipe[0];
	ctx.req.cgi_pid = pid;
	ctx.req.cgi_done = false;
	ctx.req.state = RequestBody::STATE_CGI_RUNNING;

	if (!ctx.body.empty()) {
		write(ctx.req.cgi_in_fd, ctx.body.c_str(), ctx.body.length());
	}

	close(ctx.req.cgi_in_fd);
	ctx.req.cgi_in_fd = -1;

	ctx.last_activity = std::chrono::steady_clock::now();

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
	//env.push_back("REMOTE_ADDR=127.0.0.1");

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