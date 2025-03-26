#include "Server.hpp"

bool Server::close_CGI_pipes_error(Context& ctx, std::string err_str, int (&input_pipe)[2], int (&output_pipe)[2])
{
	Logger::error(err_str);
	close(input_pipe[0]);
	close(input_pipe[1]);
	close(output_pipe[0]);
	close(output_pipe[1]);
	return updateErrorStatus(ctx, 500, "Internal Server Error");
}

bool Server::create_CGI_pipes(Context& ctx, int (&input_pipe)[2], int (&output_pipe)[2])
{
	if (pipe(input_pipe) < 0 || pipe(output_pipe) < 0)
	{
		Logger::error("Failed to create pipes for CGI");
		Logger::file("Failed to create pipes for CGI");
		return updateErrorStatus(ctx, 500, "Internal Server Error - Pipe Creation Failed");
	}

	if (setNonBlocking(input_pipe[0]) < 0 || setNonBlocking(input_pipe[1]) < 0 ||
		setNonBlocking(output_pipe[0]) < 0 || setNonBlocking(output_pipe[1]) < 0)
	{
		close_CGI_pipes_error(ctx, "Failed to set pipes to non-blocking mode", input_pipe, output_pipe);
	}
	return true;
}

void Server::execute_CGI_child_process(Context& ctx, int (&input_pipe)[2], int (&output_pipe)[2])
{
	dup2(input_pipe[0], STDIN_FILENO);
	dup2(output_pipe[1], STDOUT_FILENO);
	dup2(output_pipe[1], STDERR_FILENO);

	close(input_pipe[0]);
	close(input_pipe[1]);
	close(output_pipe[0]);
	close(output_pipe[1]);

	std::vector<std::string> env_vars = prepareCgiEnvironment(ctx);
	std::vector<char*> env_pointers;
	for (auto& var : env_vars)
		env_pointers.push_back(const_cast<char*>(var.c_str()));
	env_pointers.push_back(nullptr);
	char* args[3];
	args[0] = const_cast<char*>(ctx.location.cgi.c_str());
	args[1] = const_cast<char*>(ctx.requested_path.c_str());
	args[2] = nullptr;
	execve(ctx.location.cgi.c_str(), args, env_pointers.data());
	exit(1);
}

bool Server::executeCgi(Context& ctx)
{
	int input_pipe[2];
	int output_pipe[2];

	if (ctx.requested_path.empty() || !fileExists(ctx.requested_path) || !fileReadable(ctx.requested_path))
		return updateErrorStatus(ctx, 404, "Not Found - CGI Script");
	if (!create_CGI_pipes(ctx, input_pipe, output_pipe))
		return false;

	pid_t pid = fork();
	if (pid < 0)
		return close_CGI_pipes_error(ctx, "Failed to fork for CGI execution", input_pipe, output_pipe);
	else if (pid == 0)
		execute_CGI_child_process(ctx, input_pipe, output_pipe);
	close(input_pipe[0]);
	close(output_pipe[1]);

	//parent process:
	ctx.req.cgi_in_fd = input_pipe[1];
	ctx.req.cgi_out_fd = output_pipe[0];
	ctx.req.cgi_pid = pid;
	ctx.cgi_pipe_ready = false;
	ctx.cgi_start_time = std::chrono::steady_clock::now();
	globalFDS.cgi_pipe_to_client_fd[ctx.req.cgi_out_fd] = ctx.client_fd;
	globalFDS.cgi_pid_to_client_fd[ctx.req.cgi_pid] = ctx.client_fd;
	struct epoll_event ev;
	std::memset(&ev, 0, sizeof(ev));
	ev.events = EPOLLIN | EPOLLET;
	ev.data.fd = ctx.req.cgi_out_fd;
	if (epoll_ctl(ctx.epoll_fd, EPOLL_CTL_ADD, ctx.req.cgi_out_fd, &ev) < 0)
		Logger::error("Failed to add CGI output pipe to epoll: " + std::string(strerror(errno)));
	if (!ctx.body.empty())
		write(ctx.req.cgi_in_fd, ctx.body.c_str(), ctx.body.length());
	close(ctx.req.cgi_in_fd);
	ctx.req.cgi_in_fd = -1;
	ctx.last_activity = std::chrono::steady_clock::now();
	ctx.cgi_executed = true;
	return (modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET));
}

bool Server::handleCgiPipeEvent(int incoming_fd)
{
	auto pipe_iter = globalFDS.cgi_pipe_to_client_fd.find(incoming_fd);
	if (pipe_iter != globalFDS.cgi_pipe_to_client_fd.end())
	{
		int client_fd = pipe_iter->second;

		auto ctx_iter = globalFDS.context_map.find(client_fd);
		if (ctx_iter != globalFDS.context_map.end())
		{
			Context& ctx = ctx_iter->second;
			ctx.cgi_pipe_ready = true;
			ctx.cgi_output_phase = true;
			return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
		}
	}
	return false;
}
