/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/26 12:58:21 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/14 18:14:52 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./structs/webserv.hpp"
#include "./utils/ConfigHandler.hpp"
#include "./utils/Logger.hpp"
#include "main.hpp"

#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <map>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>

static std::map<int, RequestState> g_requests;
static std::map<int, int> g_fd_to_client;

static int g_epfd = -1;

static int setNonBlocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void modEpoll(int epfd, int fd, uint32_t events) {
	struct epoll_event ev;
	std::memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.fd = fd;

	if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
		if (errno == ENOENT) {
			if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
				Logger::file("epoll_ctl ADD failed: " + std::string(strerror(errno)));
			}
		} else {
			Logger::file("epoll_ctl MOD failed: " + std::string(strerror(errno)));
		}
	}
}


static void delFromEpoll(int epfd, int fd) {
std::stringstream ss;
ss << "Removing fd " << fd << " from epoll";
Logger::file(ss.str());

epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);

std::map<int,int>::iterator it = g_fd_to_client.find(fd);
if (it != g_fd_to_client.end()) {
	int client_fd = it->second;
	RequestState &req = g_requests[client_fd];

	ss.str("");
	ss << "Found associated client fd " << client_fd;
	Logger::file(ss.str());

	if (fd == req.cgi_in_fd) {
		Logger::file("Closing CGI input pipe");
		req.cgi_in_fd = -1;
	}
	if (fd == req.cgi_out_fd) {
		Logger::file("Closing CGI output pipe");
		req.cgi_out_fd = -1;
	}

	if (req.cgi_in_fd == -1 && req.cgi_out_fd == -1 &&
		req.state != RequestState::STATE_SENDING_RESPONSE) {
		Logger::file("Cleaning up request state");
		g_requests.erase(client_fd);
	}

	g_fd_to_client.erase(it);
} else {
	RequestState &req = g_requests[fd];
	Logger::file("Cleaning up client connection resources");

	if (req.cgi_in_fd != -1) {
		ss.str("");
		ss << "Cleaning up CGI input pipe " << req.cgi_in_fd;
		Logger::file(ss.str());
		epoll_ctl(epfd, EPOLL_CTL_DEL, req.cgi_in_fd, NULL);
		close(req.cgi_in_fd);
		g_fd_to_client.erase(req.cgi_in_fd);
	}
	if (req.cgi_out_fd != -1) {
		ss.str("");
		ss << "Cleaning up CGI output pipe " << req.cgi_out_fd;
		Logger::file(ss.str());
		epoll_ctl(epfd, EPOLL_CTL_DEL, req.cgi_out_fd, NULL);
		close(req.cgi_out_fd);
		g_fd_to_client.erase(req.cgi_out_fd);
	}

	g_requests.erase(fd);
}

close(fd);
ss.str("");
ss << "Closed fd " << fd;
Logger::file(ss.str());
}

static void cleanupCGI(RequestState &req) {
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
		epoll_ctl(g_epfd, EPOLL_CTL_DEL, req.cgi_in_fd, NULL);
		close(req.cgi_in_fd);
		g_fd_to_client.erase(req.cgi_in_fd);
		req.cgi_in_fd = -1;
	}

	if (req.cgi_out_fd != -1) {
		ss.str("");
		ss << "Closing CGI output pipe (fd " << req.cgi_out_fd << ")";
		Logger::file(ss.str());
		epoll_ctl(g_epfd, EPOLL_CTL_DEL, req.cgi_out_fd, NULL);
		close(req.cgi_out_fd);
		g_fd_to_client.erase(req.cgi_out_fd);
		req.cgi_out_fd = -1;
	}

	Logger::file("=== CGI Cleanup End ===\n");
}

static void startCGI(RequestState &req, const std::string &method, const std::string &query) {
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

	g_fd_to_client[req.cgi_in_fd] = req.client_fd;
	g_fd_to_client[req.cgi_out_fd] = req.client_fd;

	setNonBlocking(req.cgi_in_fd);
	setNonBlocking(req.cgi_out_fd);

	modEpoll(g_epfd, req.cgi_in_fd, EPOLLOUT);
	modEpoll(g_epfd, req.cgi_out_fd, EPOLLIN);

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

static const Location* findMatchingLocation(const ServerBlock* conf, const std::string& path) {
	for (size_t i = 0; i < conf->locations.size(); i++) {
		const Location &loc = conf->locations[i];
		if (path.find(loc.path) == 0) {
			return &loc;
		}
	}
	return NULL;
}

static bool needsCGI(const ServerBlock* conf, const std::string &path) {
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

static void buildResponse(RequestState &req) {
	const ServerBlock* conf = req.associated_conf;
	if (!conf) return;

	std::string content;

	content = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!";
	req.response_buffer.assign(content.begin(), content.end());

	std::stringstream ss;
	ss.str("");
	ss << "Response\n" << content;
	Logger::file(ss.str());
}

static void parseRequest(RequestState &req) {
	std::string request(req.request_buffer.begin(), req.request_buffer.end());

	size_t pos = request.find("\r\n");
	if (pos == std::string::npos)
		return;
	std::string requestLine = request.substr(0, pos);

	std::string method, path, version;
	{
		size_t firstSpace = requestLine.find(' ');
		if (firstSpace == std::string::npos) return;
		method = requestLine.substr(0, firstSpace);

		size_t secondSpace = requestLine.find(' ', firstSpace + 1);
		if (secondSpace == std::string::npos) return;
		path = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);

		version = requestLine.substr(secondSpace + 1);
	}

	std::string query;
	size_t qpos = path.find('?');
	if (qpos != std::string::npos) {
		query = path.substr(qpos + 1);
		path = path.substr(0, qpos);
	}

	req.requested_path = "http://localhost:" + std::to_string(req.associated_conf->port) + path;
	req.cgi_output_buffer.clear();

	if (needsCGI(req.associated_conf, req.requested_path)) {
		req.state = RequestState::STATE_PREPARE_CGI;
		startCGI(req, method, query);
	} else {
		buildResponse(req);
		Logger::file("here we dont !");
		req.state = RequestState::STATE_SENDING_RESPONSE;
		Logger::file("here we dont go!");
		modEpoll(g_epfd, req.client_fd, EPOLLOUT);
		Logger::file("here we dont go further!");
	}
}
static void handleClientRead(int epfd, int fd) {
	std::stringstream ss;
	ss << "Handling client read on fd " << fd;
	Logger::file(ss.str());

	RequestState &req = g_requests[fd];
	char buf[1024];
	ssize_t n = read(fd, buf, sizeof(buf));

	if (n == 0) {
		Logger::file("Client closed connection");
		delFromEpoll(epfd, fd);
		return;
	}

	if (n < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			Logger::file("Read error: " + std::string(strerror(errno)));
			delFromEpoll(epfd, fd);
		}
		return;
	}

	ss.str("");
	ss << "Read " << n << " bytes from client";
	Logger::file(ss.str());

	req.request_buffer.insert(req.request_buffer.end(), buf, buf + n);

	if (req.request_buffer.size() > 4) {
		std::string req_str(req.request_buffer.begin(), req.request_buffer.end());
		if (req_str.find("\r\n\r\n") != std::string::npos) {
			Logger::file("Complete request received, parsing");
			parseRequest(req);

			if (!needsCGI(req.associated_conf, req.requested_path)) {
				Logger::file("Processing as normal request");
				buildResponse(req);
				req.state = RequestState::STATE_SENDING_RESPONSE;
				modEpoll(epfd, fd, EPOLLOUT);
			} else {
				Logger::file("Request requires CGI processing");
			}
		}
	}
}


static void handleClientWrite(int epfd, int fd) {
	std::stringstream ss;
	ss << "Handling client write on fd " << fd;
	Logger::file(ss.str());

	RequestState &req = g_requests[fd];
	if (req.state == RequestState::STATE_SENDING_RESPONSE) {
		ss.str("");
		ss << "Attempting to write response, buffer size: " << req.response_buffer.size()
		<< ", content: " << std::string(req.response_buffer.begin(), req.response_buffer.end());
		Logger::file(ss.str());

		int error = 0;
		socklen_t len = sizeof(error);
		if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
			Logger::file("Socket error detected: " + std::string(strerror(error)));
			delFromEpoll(epfd, fd);
			return;
		}

		ssize_t n = send(fd, req.response_buffer.data(), req.response_buffer.size(), MSG_NOSIGNAL);

		if (n > 0) {
			ss.str("");
			ss << "Successfully wrote " << n << " bytes to client";
			Logger::file(ss.str());

			req.response_buffer.erase(
				req.response_buffer.begin(),
				req.response_buffer.begin() + n
			);

			if (req.response_buffer.empty()) {
				Logger::file("Response fully sent, closing connection");
				delFromEpoll(epfd, fd);
			} else {
				modEpoll(epfd, fd, EPOLLOUT);
			}
		} else if (n < 0) {
			if (errno != EAGAIN && errno != EWOULDBLOCK) {
				ss.str("");
				ss << "Write error: " << strerror(errno);
				Logger::file(ss.str());
				delFromEpoll(epfd, fd);
			} else {
				modEpoll(epfd, fd, EPOLLOUT);
			}
		}
	}
}


static void handleCGIWrite(int epfd, int fd) {
	if (g_fd_to_client.find(fd) == g_fd_to_client.end()) {
		Logger::file("No client mapping found for CGI fd " + std::to_string(fd));
		return;
	}

	int client_fd = g_fd_to_client[fd];
	RequestState &req = g_requests[client_fd];

	if (req.request_buffer.empty()) {
		Logger::file("No data to write to CGI");
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
		close(fd);
		req.cgi_in_fd = -1;
		g_fd_to_client.erase(fd);
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
			g_fd_to_client.erase(fd);
		}
	} else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
		Logger::file("Error writing to CGI: " + std::string(strerror(errno)));
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
		close(fd);
		req.cgi_in_fd = -1;
		g_fd_to_client.erase(fd);
	}
}
static void handleNewConnection(int epfd, int fd, const ServerBlock& conf) {
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(fd, (struct sockaddr*)&client_addr, &client_len);

	if (client_fd < 0) {
		Logger::file("Accept failed: " + std::string(strerror(errno)));
		return;
	}

	setNonBlocking(client_fd);
	modEpoll(epfd, client_fd, EPOLLIN);

	RequestState req;
	req.client_fd = client_fd;
	req.cgi_in_fd = -1;
	req.cgi_out_fd = -1;
	req.cgi_pid = -1;
	req.state = RequestState::STATE_READING_REQUEST;
	req.cgi_done = false;
	req.associated_conf = &conf;
	g_requests[client_fd] = req;

	std::stringstream ss;
	ss << "New client connection on fd " << client_fd;
	Logger::file(ss.str());
}
static void handleCGIRead(int epfd, int fd) {
	std::stringstream ss;
	ss << "\n=== CGI Read Handler Start ===\n"
	<< "Handling fd=" << fd;
	Logger::file(ss.str());

	if (g_fd_to_client.find(fd) == g_fd_to_client.end()) {
		Logger::file("ERROR: No client mapping found for CGI fd");
		epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
		return;
	}

	int client_fd = g_fd_to_client[fd];
	RequestState &req = g_requests[client_fd];

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

int main(int argc, char **argv, char **envp)
{
ConfigHandler utils;

try {
	utils.parseArgs(argc, argv, envp);
	if (!utils.getconfigFileValid()) {
		Logger::red() << "Invalid configuration file!";
		Logger::file("Invalid configuration file");
		return EXIT_FAILURE;
	}

	std::vector<ServerBlock> configs = utils.get_registeredServerConfs();
	if (configs.empty()) {
		Logger::red() << "No configurations found!";
		Logger::file("No configurations found");
		return EXIT_FAILURE;
	}

	int epfd = epoll_create1(0);
	if (epfd < 0) {
		Logger::red() << "Failed to create epoll\n";
		Logger::file("Failed to create epoll: " + std::string(strerror(errno)));
		return EXIT_FAILURE;
	}
	g_epfd = epfd;
	Logger::file("Server starting");

	for (auto &conf : configs) {
		conf.server_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (conf.server_fd < 0) {
			std::stringstream ss;
			ss << "Failed to create socket on port: " << conf.port;
			Logger::red() << ss.str();
			Logger::file(ss.str());
			return EXIT_FAILURE;
		}

		int opt = 1;
		setsockopt(conf.server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

		struct sockaddr_in addr;
		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(conf.port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);

		if (bind(conf.server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
			std::stringstream ss;
			ss << "Failed to bind socket on port: " << conf.port;
			Logger::red() << ss.str();
			Logger::file(ss.str());
			close(conf.server_fd);
			return EXIT_FAILURE;
		}

		if (listen(conf.server_fd, SOMAXCONN) < 0) {
			std::stringstream ss;
			ss << "Failed to listen on port: " << conf.port;
			Logger::red() << ss.str();
			Logger::file(ss.str());
			close(conf.server_fd);
			return EXIT_FAILURE;
		}

		setNonBlocking(conf.server_fd);
		modEpoll(epfd, conf.server_fd, EPOLLIN);

		std::stringstream ss;
		ss << "Server listening on port: " << conf.port;
		Logger::green() << ss.str() << "\n";
		Logger::file(ss.str());
	}

	const int max_events = 64;
	struct epoll_event events[max_events];

	while (true) {
		int n = epoll_wait(epfd, events, max_events, -1);

		std::stringstream wer;
		wer << "epoll_wait n " << n;
		Logger::file(wer.str());
		if (n < 0) {
			std::stringstream ss;
			ss << "epoll_wait error: " << strerror(errno);
			Logger::file(ss.str());
			if (errno == EINTR)
				continue;
			break;
		}

		std::stringstream ewrew;
		ewrew << "Event Loop Iteration: " << n << " events";
		Logger::file(ewrew.str());

		for (int i = 0; i < n; i++) {
	int fd = events[i].data.fd;
	uint32_t ev = events[i].events;

	std::stringstream ss;
	ss << "\n=== Event Processing Start ===\n"
	<< "Processing fd=" << fd << " events=" << ev << "\n"
	<< "EPOLLIN=" << (ev & EPOLLIN) << "\n"
	<< "EPOLLOUT=" << (ev & EPOLLOUT) << "\n"
	<< "EPOLLHUP=" << (ev & EPOLLHUP) << "\n"
	<< "EPOLLERR=" << (ev & EPOLLERR);
	Logger::file(ss.str());

	const ServerBlock* associated_conf = nullptr;
	for (const auto &conf : configs) {
		if (conf.server_fd == fd) {
			associated_conf = &conf;
			break;
		}
	}

	if (associated_conf) {
		handleNewConnection(epfd, fd, *associated_conf);
		continue;
	}

	auto client_it = g_requests.find(fd);
	if (client_it != g_requests.end()) {
		if (ev & (EPOLLHUP | EPOLLERR)) {
			Logger::file("Client socket error/hangup detected");
			delFromEpoll(epfd, fd);
			continue;
		}
		if (ev & EPOLLIN) handleClientRead(epfd, fd);
		if (ev & EPOLLOUT) handleClientWrite(epfd, fd);
		continue;
	}

	auto cgi_it = g_fd_to_client.find(fd);
	if (cgi_it != g_fd_to_client.end()) {
		int client_fd = cgi_it->second;
		RequestState &req = g_requests[client_fd];

		ss.str("");
		ss << "CGI pipe event:\n"
		<< "- Related client_fd: " << client_fd << "\n"
		<< "- CGI in_fd: " << req.cgi_in_fd << "\n"
		<< "- CGI out_fd: " << req.cgi_out_fd << "\n"
		<< "- Current state: " << req.state;
		Logger::file(ss.str());

		if (fd == req.cgi_out_fd) {
			if (ev & EPOLLIN) {
				handleCGIRead(epfd, fd);
			}
			if (ev & EPOLLHUP) {
	Logger::file("CGI output pipe hangup detected, finalizing response");

	if (!req.cgi_output_buffer.empty()) {
		std::string output(req.cgi_output_buffer.begin(), req.cgi_output_buffer.end());
		ss.str("");
		ss << "Preparing final response with " << output.length() << " bytes";
		Logger::file(ss.str());

		std::string response;
		size_t header_end = output.find("\r\n\r\n");

		if (output.find("Content-type:") != std::string::npos ||
			output.find("Content-Type:") != std::string::npos) {
			std::string headers = output.substr(0, header_end);
			std::string body = output.substr(header_end + 4);

			response = "HTTP/1.1 200 OK\r\n";
			if (headers.find("Content-Length:") == std::string::npos) {
				response += "Content-Length: " + std::to_string(body.length()) + "\r\n";
			}
			if (headers.find("Connection:") == std::string::npos) {
				response += "Connection: close\r\n";
			}
			response += headers + "\r\n" + body;
		} else {
			response = "HTTP/1.1 200 OK\r\n"
					"Content-Type: text/html\r\n"
					"Content-Length: " + std::to_string(output.length()) + "\r\n"
					"Connection: close\r\n"
					"\r\n" + output;
		}

		ss.str("");
		ss << "Final response headers:\n" << response.substr(0, response.find("\r\n\r\n") + 4);
		Logger::file(ss.str());

		req.response_buffer.assign(response.begin(), response.end());
		req.state = RequestState::STATE_SENDING_RESPONSE;
		modEpoll(epfd, client_fd, EPOLLOUT);
	}

	cleanupCGI(req);
			}
		} else if (fd == req.cgi_in_fd) {
			if (ev & EPOLLOUT) {
				handleCGIWrite(epfd, fd);
			}
			if (ev & (EPOLLHUP | EPOLLERR)) {
				Logger::file("CGI input pipe error/hangup detected");
				epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
				close(fd);
				req.cgi_in_fd = -1;
				g_fd_to_client.erase(fd);
			}
		}
		continue;
	}

	Logger::file("Unknown fd encountered, removing from epoll");
	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
	Logger::file("=== Event Processing End ===\n");
}
	}
	Logger::file("Server shutting down");
	close(epfd);
}
catch (const std::exception &e) {
	std::string err_msg = "Error: " + std::string(e.what());
	Logger::red() << err_msg;
	Logger::file(err_msg);
	return EXIT_FAILURE;
}
return EXIT_SUCCESS;
}