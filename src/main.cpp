/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/26 12:58:21 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/13 16:04:37 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

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

struct RequestState {
	int client_fd;
	int cgi_in_fd;
	int cgi_out_fd;
	pid_t cgi_pid;
	enum State {
		STATE_READING_REQUEST,
		STATE_PREPARE_CGI,
		STATE_CGI_RUNNING,
		STATE_SENDING_RESPONSE
	} state;
	std::vector<char> request_buffer;
	std::vector<char> response_buffer;
	std::vector<char> cgi_output_buffer;
	bool cgi_done;

	const ServerBlock* associated_conf;
	std::string requested_path;
};

static std::map<int, RequestState> g_requests;
static std::map<int, int> g_fd_to_client;

static int g_epfd = -1;

static int setNonBlocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void addToEpoll(int epfd, int fd, uint32_t events) {
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.fd = fd;
	if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		Logger::red() << "epoll_ctl ADD failed\n";
	}
}

static void modEpoll(int epfd, int fd, uint32_t events) {
	struct epoll_event ev;
	memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.fd = fd;
	if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
		Logger::red() << "epoll_ctl MOD failed\n";
	}
}

static void delFromEpoll(int epfd, int fd) {
	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);
	close(fd);

	// Falls es sich um einen cgi_out_fd handelt, entfernen wir den Mapping-Eintrag
	std::map<int,int>::iterator it = g_fd_to_client.find(fd);
	if (it != g_fd_to_client.end()) {
		g_fd_to_client.erase(it);
	} else {
		// Andernfalls löschen wir den Request aus g_requests
		g_requests.erase(fd);
	}
}

static void startCGI(RequestState &req, const std::string &method, const std::string &query) {
    int pipe_in[2];
    int pipe_out[2];

    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) {
        Logger::red() << "Error creating pipes for CGI\n";
        return;
    }

    req.cgi_in_fd = pipe_in[1];
    req.cgi_out_fd = pipe_out[0];

    pid_t pid = fork();
    if (pid < 0) {
        Logger::red() << "Fork failed\n";
        return;
    }
    if (pid == 0) {
        // Kindprozess (CGI)
        close(pipe_in[1]);
        close(pipe_out[0]);

        dup2(pipe_in[0], STDIN_FILENO);
        dup2(pipe_out[1], STDOUT_FILENO);
        close(pipe_in[0]);
        close(pipe_out[1]);

        std::string script_path = "/app/var/www/php/index.php";

        std::string method_env = "REQUEST_METHOD=" + method;
        std::string query_env = "QUERY_STRING=" + query;
        std::string script_env = "SCRIPT_FILENAME=" + script_path;
        std::string gateway_env = "GATEWAY_INTERFACE=CGI/1.1";
        std::string server_protocol_env = "SERVER_PROTOCOL=HTTP/1.1";
        std::string redirect_status_env = "REDIRECT_STATUS=200";
        std::string server_software_env = "SERVER_SOFTWARE=MySimpleWebServer/1.0";
        std::string server_name_env = "SERVER_NAME=localhost";
        std::string server_port_env = "SERVER_PORT=" + std::to_string(req.associated_conf->port);
        std::string request_uri_env = "REQUEST_URI=" + req.requested_path;
        std::string script_name_env = "SCRIPT_NAME=/index.php";

        char *env[] = {
            (char*)method_env.c_str(),
            (char*)query_env.c_str(),
            (char*)"CONTENT_LENGTH=0",
            (char*)script_env.c_str(),
            (char*)gateway_env.c_str(),
            (char*)server_protocol_env.c_str(),
            (char*)server_software_env.c_str(),
            (char*)server_name_env.c_str(),
            (char*)server_port_env.c_str(),
            (char*)redirect_status_env.c_str(),
            (char*)request_uri_env.c_str(),
            (char*)script_name_env.c_str(),
            NULL
        };

        char *args[] = {
            (char*)"/usr/bin/php-cgi",
            NULL
        };

        execve(args[0], args, env);
        _exit(1);
    }

    // Elternprozess:
    close(pipe_in[0]);
    close(pipe_out[1]);

    // Wichtige Änderung: Da wir kein Input senden, schließen wir den Schreib-Ende direkt.
    close(req.cgi_in_fd);
    req.cgi_in_fd = -1;

    setNonBlocking(req.cgi_out_fd);
    addToEpoll(g_epfd, req.cgi_out_fd, EPOLLIN);

    g_fd_to_client[req.cgi_out_fd] = req.client_fd;

    req.cgi_pid = pid;
    req.state = RequestState::STATE_CGI_RUNNING;
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
	req.state = RequestState::STATE_PREPARE_CGI;
	startCGI(req, method, query);
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
}


static void handleClientRead(int epfd, int fd) {
	RequestState &req = g_requests[fd];
	char buf[1024];
	ssize_t n = read(fd, buf, sizeof(buf));
	if (n == 0) {
		delFromEpoll(epfd, fd);
		return;
	}
	if (n < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			delFromEpoll(epfd, fd);
		}
		return;
	}
	req.request_buffer.insert(req.request_buffer.end(), buf, buf + n);

	if (req.request_buffer.size() > 4) {
		std::string req_str(req.request_buffer.begin(), req.request_buffer.end());
		if (req_str.find("\r\n\r\n") != std::string::npos) {

			parseRequest(req);
			if (!needsCGI(req.associated_conf, req.requested_path)) {
				buildResponse(req);
				req.state = RequestState::STATE_SENDING_RESPONSE;
				modEpoll(epfd, fd, EPOLLOUT);
			}
		}
	}
}

static void handleClientWrite(int epfd, int fd) {
	RequestState &req = g_requests[fd];
	if (req.state == RequestState::STATE_SENDING_RESPONSE) {
		ssize_t n = write(fd, req.response_buffer.data(), req.response_buffer.size());
		if (n > 0) {
			req.response_buffer.erase(req.response_buffer.begin(), req.response_buffer.begin() + n);
			if (req.response_buffer.empty()) {
				delFromEpoll(epfd, fd);
			}
		} else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
			delFromEpoll(epfd, fd);
		}
	}
}


static void handleCGIRead(int epfd, int fd) {
	// Hier holen wir den client_fd anhand des cgi_out_fd
	if (g_fd_to_client.find(fd) == g_fd_to_client.end()) {
		Logger::red() << "handleCGIRead: No mapping found for fd " << fd << "\n";
		delFromEpoll(epfd, fd);
		return;
	}
	int client_fd = g_fd_to_client[fd];
	RequestState &req = g_requests[client_fd];

	char buf[1024];
	ssize_t n = read(fd, buf, sizeof(buf));
	if (n == 0) {
		req.cgi_done = true;
		req.response_buffer.assign(req.cgi_output_buffer.begin(), req.cgi_output_buffer.end());
		req.state = RequestState::STATE_SENDING_RESPONSE;
		modEpoll(epfd, req.client_fd, EPOLLOUT);
		delFromEpoll(epfd, fd); // cgi_out_fd entfernen
		return;
	}
	if (n < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			delFromEpoll(epfd, fd);
		}
		return;
	}
	req.cgi_output_buffer.insert(req.cgi_output_buffer.end(), buf, buf + n);
}

int main(int argc, char **argv, char **envp)
{
	ConfigHandler utils;
	Server server;

	try {
		utils.parseArgs(argc, argv, envp);
		if (!utils.getconfigFileValid()) {
			Logger::red() << "Invalid configuration file!";
			return EXIT_FAILURE;
		}

		std::vector<ServerBlock> configs = utils.get_registeredServerConfs();
		if (configs.empty()) {
			Logger::red() << "No configurations found!";
			return EXIT_FAILURE;
		}

		int epfd = epoll_create1(0);
		if (epfd < 0) {
			Logger::red() << "Failed to create epoll\n";
			return EXIT_FAILURE;
		}
		g_epfd = epfd;

		for (auto &conf : configs) {
			conf.server_fd = socket(AF_INET, SOCK_STREAM, 0);
			if (conf.server_fd < 0) {
				Logger::red() << "Failed to create socket on port: " << conf.port;
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
				Logger::red() << "Failed to bind socket on port: " << conf.port;
				close(conf.server_fd);
				return EXIT_FAILURE;
			}

			if (listen(conf.server_fd, SOMAXCONN) < 0) {
				Logger::red() << "Failed to listen on port: " << conf.port;
				close(conf.server_fd);
				return EXIT_FAILURE;
			}

			setNonBlocking(conf.server_fd);
			addToEpoll(epfd, conf.server_fd, EPOLLIN);
			Logger::green() << "Server listening on port: " << conf.port << "\n";
		}

		const int max_events = 64;
		struct epoll_event events[max_events];

		while (true) {
			int n = epoll_wait(epfd, events, max_events, -1);
			if (n < 0) {
				if (errno == EINTR)
					continue;
				Logger::red() << "epoll_wait error\n";
				break;
			}

			for (int i = 0; i < n; i++) {
				int fd = events[i].data.fd;
				uint32_t ev = events[i].events;

				const ServerBlock* associated_conf = NULL;
				for (auto &conf : configs) {
					if (conf.server_fd == fd) {
						associated_conf = &conf;
						break;
					}
				}

				if (associated_conf) {
					int client_fd = accept(fd, NULL, NULL);
					if (client_fd < 0) continue;
					setNonBlocking(client_fd);
					addToEpoll(epfd, client_fd, EPOLLIN);

					RequestState req;
					req.client_fd = client_fd;
					req.cgi_in_fd = -1;
					req.cgi_out_fd = -1;
					req.cgi_pid = -1;
					req.state = RequestState::STATE_READING_REQUEST;
					req.cgi_done = false;
					req.associated_conf = associated_conf;
					g_requests[client_fd] = req;
				} else {
					if (g_requests.find(fd) == g_requests.end()) {
						continue;
					}
					if (ev & EPOLLIN) {
						RequestState &r = g_requests[fd];
						if (r.state == RequestState::STATE_READING_REQUEST)
							handleClientRead(epfd, fd);
						else if (r.state == RequestState::STATE_CGI_RUNNING && fd == r.cgi_out_fd)
							handleCGIRead(epfd, fd);
					}
					if (ev & EPOLLOUT) {
						RequestState &r = g_requests[fd];
						if (r.state == RequestState::STATE_SENDING_RESPONSE && fd == r.client_fd)
							handleClientWrite(epfd, fd);
					}
					if (ev & (EPOLLHUP | EPOLLERR)) {
						delFromEpoll(epfd, fd);
					}
				}
			}
		}
		close(epfd);
	} catch (const std::exception &e) {
		Logger::red() << "Error: " << e.what();
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}