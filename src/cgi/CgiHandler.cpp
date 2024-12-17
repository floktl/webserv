/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiHandler.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/11 14:42:00 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/17 10:58:34 by fkeitel          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "CgiHandler.hpp"

CgiHandler::CgiHandler(Server& _server) : server(_server) {}

void CgiHandler::cleanupCGI(RequestState &req) {
	std::stringstream ss;
	ss << "\n=== CGI Cleanup Start ===\n"
	<< "Cleaning up CGI resources for client_fd " << req.client_fd;
	Logger::file(ss.str());

	if (req.cgi_pid > 0) {
		int status;
		pid_t wait_result = waitpid(req.cgi_pid, &status, WNOHANG);

		ss.str("");
		ss << "CGI process (PID " << req.cgi_pid << ") status check:\n"
		<< "- waitpid result: " << wait_result;
		if (wait_result > 0) {
			ss << "\n- exit status: " << WEXITSTATUS(status);
			if (WIFSIGNALED(status)) {
				ss << "\n- terminated by signal: " << WTERMSIG(status);
			}
		}
		Logger::file(ss.str());

		if (wait_result == 0) {
			Logger::file("CGI process still running, sending SIGTERM");
			kill(req.cgi_pid, SIGTERM);
			usleep(100000);
			wait_result = waitpid(req.cgi_pid, &status, WNOHANG);

			if (wait_result == 0) {
				Logger::file("Process didn't terminate, sending SIGKILL");
				kill(req.cgi_pid, SIGKILL);
				waitpid(req.cgi_pid, &status, 0);
			}
		}
		req.cgi_pid = -1;
	}

	if (req.cgi_in_fd != -1) {
		ss.str("");
		ss << "Closing CGI input pipe (fd " << req.cgi_in_fd << ")";
		Logger::file(ss.str());
		epoll_ctl(server.getGlobalFds().epoll_FD, EPOLL_CTL_DEL, req.cgi_in_fd, NULL);
		close(req.cgi_in_fd);
		server.getGlobalFds().svFD_to_clFD_map.erase(req.cgi_in_fd);
		req.cgi_in_fd = -1;
	}

	if (req.cgi_out_fd != -1) {
		ss.str("");
		ss << "Closing CGI output pipe (fd " << req.cgi_out_fd << ")";
		Logger::file(ss.str());
		epoll_ctl(server.getGlobalFds().epoll_FD, EPOLL_CTL_DEL, req.cgi_out_fd, NULL);
		close(req.cgi_out_fd);
		server.getGlobalFds().svFD_to_clFD_map.erase(req.cgi_out_fd);
		req.cgi_out_fd = -1;
	}

	Logger::file("=== CGI Cleanup End ===\n");
}

void CgiHandler::startCGI(RequestState &req, const std::string &method, const std::string &query) {
	int pipe_in[2];
	int pipe_out[2];

	std::stringstream ss;
	ss << "Starting CGI Process - Method: " << method << " Query: " << query;
	Logger::file(ss.str());

	if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) {
		Logger::file("Error creating pipes: " + std::string(strerror(errno)));
		return;
	}

	req.cgi_in_fd = pipe_in[1];
	req.cgi_out_fd = pipe_out[0];

	server.getGlobalFds().svFD_to_clFD_map[req.cgi_in_fd] = req.client_fd;
	server.getGlobalFds().svFD_to_clFD_map[req.cgi_out_fd] = req.client_fd;

	server.setNonBlocking(req.cgi_in_fd);
	server.setNonBlocking(req.cgi_out_fd);

	server.modEpoll(server.getGlobalFds().epoll_FD, req.cgi_in_fd, EPOLLOUT);
	server.modEpoll(server.getGlobalFds().epoll_FD, req.cgi_out_fd, EPOLLIN);

	pid_t pid = fork();
	if (pid < 0) {
		Logger::file("Fork failed: " + std::string(strerror(errno)));
		cleanupCGI(req);
		return;
	}

	if (pid == 0) {
		close(pipe_in[1]);
		close(pipe_out[0]);

		if (dup2(pipe_in[0], STDIN_FILENO) < 0) {
			Logger::file("dup2 stdin failed: " + std::string(strerror(errno)));
			_exit(1);
		}
		if (dup2(pipe_out[1], STDOUT_FILENO) < 0) {
			Logger::file("dup2 stdout failed: " + std::string(strerror(errno)));
			_exit(1);
		}

		close(pipe_in[0]);
		close(pipe_out[1]);

		std::string requested_file = req.requested_path;
		size_t last_slash = requested_file.find_last_of('/');
		if (last_slash != std::string::npos) {
			requested_file = requested_file.substr(last_slash + 1);
		}

		if (requested_file.empty() || requested_file == "?") {
			requested_file = "index.php";
		}

		std::string script_path = "/app/var/www/php/" + requested_file;
		std::string content_length = std::to_string(req.request_buffer.size());

		std::vector<std::string> env_strings = {
			"REQUEST_METHOD=" + method,
			"QUERY_STRING=" + query,
			"CONTENT_LENGTH=" + content_length,
			"SCRIPT_FILENAME=" + script_path,
			"GATEWAY_INTERFACE=CGI/1.1",
			"SERVER_PROTOCOL=HTTP/1.1",
			"SERVER_SOFTWARE=MySimpleWebServer/1.0",
			"SERVER_NAME=localhost",
			"SERVER_PORT=" + std::to_string(req.associated_conf->port),
			"REDIRECT_STATUS=200",
			"REQUEST_URI=" + req.requested_path,
			"SCRIPT_NAME=/" + requested_file,
			"CONTENT_TYPE=application/x-www-form-urlencoded"
		};

		std::vector<char*> env;
		for (const auto& s : env_strings) {
			env.push_back(strdup(s.c_str()));
		}
		env.push_back(nullptr);

		char* const args[] = {
			(char*)"/usr/bin/php-cgi",
			(char*)script_path.c_str(),
			nullptr
		};

		execve(args[0], args, env.data());

		for (char* ptr : env) {
			free(ptr);
		}

		Logger::file("execve failed: " + std::string(strerror(errno)));
		_exit(1);
	}

	close(pipe_in[0]);
	close(pipe_out[1]);

	req.cgi_pid = pid;
	req.state = RequestState::STATE_CGI_RUNNING;
	req.cgi_done = false;

	Logger::file("CGI setup complete");
}

const Location* CgiHandler::findMatchingLocation(const ServerBlock* conf, const std::string& path) {
	for (size_t i = 0; i < conf->locations.size(); i++) {
		const Location &loc = conf->locations[i];
		if (path.find(loc.path) == 0) {
			return &loc;
		}
	}
	return NULL;
}

bool CgiHandler::needsCGI(const ServerBlock* conf, const std::string &path) {
	if (path.length() >= 4 && path.substr(path.length() - 4) == ".php") {
		return true;
	}

	const Location* loc = findMatchingLocation(conf, path);
	if (conf->port == 8001)
	{
		return true;
	}
	if (!loc)
		return false;
	if (!loc->cgi.empty())
		return true;
	return false;
}

void CgiHandler::handleCGIWrite(int epfd, int fd) {
	if (server.getGlobalFds().svFD_to_clFD_map.find(fd) == server.getGlobalFds().svFD_to_clFD_map.end()) {
		Logger::file("No client mapping found for CGI fd " + std::to_string(fd));
		return;
	}

	int client_fd = server.getGlobalFds().svFD_to_clFD_map[fd];
	RequestState &req = server.getGlobalFds().request_state_map[client_fd];

	if (req.request_buffer.empty()) {
		Logger::file("No data to write to CGI");
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
		close(fd);
		req.cgi_in_fd = -1;
		server.getGlobalFds().svFD_to_clFD_map.erase(fd);
		return;
	}

	ssize_t n = write(fd, req.request_buffer.data(), req.request_buffer.size());

	std::stringstream ss;
	ss << "Wrote " << n << " bytes to CGI";
	Logger::file(ss.str());

	if (n > 0) {
		req.request_buffer.erase(req.request_buffer.begin(), req.request_buffer.begin() + n);
		if (req.request_buffer.empty()) {
			Logger::file("Finished writing to CGI");
			epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
			close(fd);
			req.cgi_in_fd = -1;
			server.getGlobalFds().svFD_to_clFD_map.erase(fd);
		}
	} else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		Logger::file("Error writing to CGI: " + std::string(strerror(errno)));
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
		close(fd);
		req.cgi_in_fd = -1;
		server.getGlobalFds().svFD_to_clFD_map.erase(fd);
	}
}

void CgiHandler::handleCGIRead(int epfd, int fd) {
	std::stringstream ss;
	ss << "\n=== CGI Read Handler Start ===\n"
	<< "Handling fd=" << fd;
	Logger::file(ss.str());

	if (server.getGlobalFds().svFD_to_clFD_map.find(fd) == server.getGlobalFds().svFD_to_clFD_map.end()) {
		Logger::file("ERROR: No client mapping found for CGI fd");
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
		return;
	}

	int client_fd = server.getGlobalFds().svFD_to_clFD_map[fd];
	RequestState &req = server.getGlobalFds().request_state_map[client_fd];

	ss.str("");
	ss << "CGI State Info:\n"
	<< "- Client FD: " << client_fd << "\n"
	<< "- CGI in_fd: " << req.cgi_in_fd << "\n"
	<< "- CGI out_fd: " << req.cgi_out_fd << "\n"
	<< "- CGI PID: " << req.cgi_pid << "\n"
	<< "- Current buffer size: " << req.cgi_output_buffer.size();
	Logger::file(ss.str());

	char buf[4096];
	ssize_t n = read(fd, buf, sizeof(buf));

	ss.str("");
	ss << "Read attempt returned: " << n;
	if (n < 0) {
		ss << " (errno: " << errno << " - " << strerror(errno) << ")";
	}
	Logger::file(ss.str());

	if (n > 0) {
		std::string debug_output(buf, std::min(n, (ssize_t)200));
		ss.str("");
		ss << "CGI Output Preview:\n" << debug_output;
		Logger::file(ss.str());

		req.cgi_output_buffer.insert(req.cgi_output_buffer.end(), buf, buf + n);

		ss.str("");
		ss << "Total buffer size now: " << req.cgi_output_buffer.size();
		Logger::file(ss.str());
	} else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		ss.str("");
		ss << "Error reading from CGI: " << strerror(errno);
		Logger::file(ss.str());
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
	}

	Logger::file("=== CGI Read Handler End ===\n");
}