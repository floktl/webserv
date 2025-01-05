#include "CgiHandler.hpp"

void CgiHandler::cleanupCGI(RequestState &req)
{

	if (req.cgi_pid > 0)
	{
		int status;
		pid_t wait_result = waitpid(req.cgi_pid, &status, WNOHANG);

		if (wait_result == 0)
		{
			kill(req.cgi_pid, SIGTERM);
			usleep(100000);
			wait_result = waitpid(req.cgi_pid, &status, WNOHANG);

			if (wait_result == 0) {
				kill(req.cgi_pid, SIGKILL);
				waitpid(req.cgi_pid, &status, 0);
			}
		}
		req.cgi_pid = -1;
	}

	for (int fd : {req.cgi_in_fd, req.cgi_out_fd})
	{
		if (fd != -1)
		{
			epoll_ctl(server.getGlobalFds().epoll_fd, EPOLL_CTL_DEL, fd, NULL);
			close(fd);
			server.getGlobalFds().svFD_to_clFD_map.erase(fd);
		}
	}
	req.cgi_in_fd = -1;
	req.cgi_out_fd = -1;
}

void CgiHandler::cleanup_tunnel(CgiTunnel &tunnel)
{
	int in_fd = tunnel.in_fd;
	int out_fd = tunnel.out_fd;
	pid_t pid = tunnel.pid;

	if (in_fd != -1) {
		epoll_ctl(server.getGlobalFds().epoll_fd, EPOLL_CTL_DEL, in_fd, NULL);
		close(in_fd);
		server.getGlobalFds().svFD_to_clFD_map.erase(in_fd);
		fd_to_tunnel.erase(in_fd);
	}

	if (out_fd != -1) {
		epoll_ctl(server.getGlobalFds().epoll_fd, EPOLL_CTL_DEL, out_fd, NULL);
		close(out_fd);
		server.getGlobalFds().svFD_to_clFD_map.erase(out_fd);
		fd_to_tunnel.erase(out_fd);
	}

	if (pid > 0) {
		kill(pid, SIGTERM);
		usleep(100000);
		if (waitpid(pid, NULL, WNOHANG) == 0) {
			kill(pid, SIGKILL);
			waitpid(pid, NULL, 0);
		}
	}

	tunnels.erase(in_fd);
}

void CgiHandler::cleanup_pipes(int pipe_in[2], int pipe_out[2])
{
	if (pipe_in[0] != -1) close(pipe_in[0]);
	if (pipe_in[1] != -1) close(pipe_in[1]);
	if (pipe_out[0] != -1) close(pipe_out[0]);
	if (pipe_out[1] != -1) close(pipe_out[1]);
}
