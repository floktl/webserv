#include "CgiHandler.hpp"

//void CgiHandler::cleanupCGI(RequestBody &req)
//{

//	if (req.cgi_pid > 0)
//	{
//		int status;
//		pid_t wait_result = waitpid(req.cgi_pid, &status, WNOHANG);

//		if (wait_result == 0)
//		{
//			kill(req.cgi_pid, SIGTERM);
//			usleep(100000);
//			wait_result = waitpid(req.cgi_pid, &status, WNOHANG);

//			if (wait_result == 0) {
//				kill(req.cgi_pid, SIGKILL);
//				waitpid(req.cgi_pid, &status, 0);
//			}
//		}
//		req.cgi_pid = -1;
//	}

//	for (int fd : {req.cgi_in_fd, req.cgi_out_fd})
//	{
//		if (fd != -1)
//		{
//			epoll_ctl(server.getGlobalFds().epoll_fd, EPOLL_CTL_DEL, fd, NULL);
//			close(fd);
//			server.getGlobalFds().clFD_to_svFD_map.erase(fd);
//		}
//	}
//	req.cgi_in_fd = -1;
//	req.cgi_out_fd = -1;
//}

//void CgiHandler::cleanup_tunnel(CgiTunnel &tunnel) {
//	if (tunnel.in_fd != -1) {
//		epoll_ctl(server.getGlobalFds().epoll_fd, EPOLL_CTL_DEL, tunnel.in_fd, NULL);
//		close(tunnel.in_fd);
//		server.getGlobalFds().clFD_to_svFD_map.erase(tunnel.in_fd);
//		fd_to_tunnel.erase(tunnel.in_fd);
//		tunnel.in_fd = -1;
//	}
//	if (tunnel.out_fd != -1) {
//		epoll_ctl(server.getGlobalFds().epoll_fd, EPOLL_CTL_DEL, tunnel.out_fd, NULL);
//		close(tunnel.out_fd);
//		server.getGlobalFds().clFD_to_svFD_map.erase(tunnel.out_fd);
//		fd_to_tunnel.erase(tunnel.out_fd);
//		tunnel.out_fd = -1;
//	}
//	if (tunnel.pid != -1) {
//		waitpid(tunnel.pid, NULL, 0);
//		tunnel.pid = -1;
//	}
//	for(char* ptr : tunnel.envp) {
//		if(ptr) free(ptr);
//	}
//	tunnel.envp.clear();
//}


//void CgiHandler::cleanup_pipes(int pipe_in[2], int pipe_out[2])
//{
//	if (pipe_in[0] != -1) close(pipe_in[0]);
//	if (pipe_in[1] != -1) close(pipe_in[1]);
//	if (pipe_out[0] != -1) close(pipe_out[0]);
//	if (pipe_out[1] != -1) close(pipe_out[1]);
//}
