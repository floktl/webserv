/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/26 12:58:21 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/14 16:38:37 by jeberle          ###   ########.fr       */
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

static void logMessage(const std::string& message) {
	FILE* logfile = fopen("/app/tmp/webserv.log", "a");
	if (logfile) {
		time_t now = time(0);
		char* timestamp = ctime(&now);
		timestamp[strlen(timestamp) - 1] = '\0';
		fprintf(logfile, "[%s] %s\n", timestamp, message.c_str());
		fclose(logfile);
	}
}

static int setNonBlocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static void modEpoll(int epfd, int fd, uint32_t events) {
    struct epoll_event ev;
    memset(&ev, 0, sizeof(ev));
    ev.events = events;
    ev.data.fd = fd;

    // Try to modify first, if that fails (probably because it doesn't exist), add it
    if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) < 0) {
        if (errno == ENOENT) {
            if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0) {
                logMessage("epoll_ctl ADD failed: " + std::string(strerror(errno)));
            }
        } else {
            logMessage("epoll_ctl MOD failed: " + std::string(strerror(errno)));
        }
    }
}


static void delFromEpoll(int epfd, int fd) {
std::stringstream ss;
ss << "Removing fd " << fd << " from epoll";
logMessage(ss.str());

epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);

std::map<int,int>::iterator it = g_fd_to_client.find(fd);
if (it != g_fd_to_client.end()) {
	int client_fd = it->second;
	RequestState &req = g_requests[client_fd];

	ss.str("");
	ss << "Found associated client fd " << client_fd;
	logMessage(ss.str());

	if (fd == req.cgi_in_fd) {
		logMessage("Closing CGI input pipe");
		req.cgi_in_fd = -1;
	}
	if (fd == req.cgi_out_fd) {
		logMessage("Closing CGI output pipe");
		req.cgi_out_fd = -1;
	}

	if (req.cgi_in_fd == -1 && req.cgi_out_fd == -1 &&
		req.state != RequestState::STATE_SENDING_RESPONSE) {
		logMessage("Cleaning up request state");
		g_requests.erase(client_fd);
	}

	g_fd_to_client.erase(it);
} else {
	RequestState &req = g_requests[fd];
	logMessage("Cleaning up client connection resources");

	if (req.cgi_in_fd != -1) {
		ss.str("");
		ss << "Cleaning up CGI input pipe " << req.cgi_in_fd;
		logMessage(ss.str());
		epoll_ctl(epfd, EPOLL_CTL_DEL, req.cgi_in_fd, NULL);
		close(req.cgi_in_fd);
		g_fd_to_client.erase(req.cgi_in_fd);
	}
	if (req.cgi_out_fd != -1) {
		ss.str("");
		ss << "Cleaning up CGI output pipe " << req.cgi_out_fd;
		logMessage(ss.str());
		epoll_ctl(epfd, EPOLL_CTL_DEL, req.cgi_out_fd, NULL);
		close(req.cgi_out_fd);
		g_fd_to_client.erase(req.cgi_out_fd);
	}

	g_requests.erase(fd);
}

close(fd);
ss.str("");
ss << "Closed fd " << fd;
logMessage(ss.str());
}

static void startCGI(RequestState &req, const std::string &method, const std::string &query) {
    int pipe_in[2];
    int pipe_out[2];

    std::stringstream ss;
    ss << "Starting CGI Process - Method: " << method << " Query: " << query;
    logMessage(ss.str());

    if (pipe(pipe_in) < 0 || pipe(pipe_out) < 0) {
        logMessage("Error creating pipes: " + std::string(strerror(errno)));
        return;
    }

    // Setze beide Pipe-Enden auf non-blocking
    if (setNonBlocking(pipe_in[0]) < 0 || setNonBlocking(pipe_in[1]) < 0 ||
        setNonBlocking(pipe_out[0]) < 0 || setNonBlocking(pipe_out[1]) < 0) {
        logMessage("Failed to set pipes non-blocking");
        close(pipe_in[0]); close(pipe_in[1]);
        close(pipe_out[0]); close(pipe_out[1]);
        return;
    }

    req.cgi_in_fd = pipe_in[1];
    req.cgi_out_fd = pipe_out[0];

    pid_t pid = fork();
    if (pid < 0) {
        logMessage("Fork failed: " + std::string(strerror(errno)));
        close(pipe_in[0]); close(pipe_in[1]);
        close(pipe_out[0]); close(pipe_out[1]);
        return;
    }

    if (pid == 0) { // Child process
        ss.str("");
        ss << "Child process started (PID: " << getpid() << ")";
        logMessage(ss.str());

        close(pipe_in[1]);
        close(pipe_out[0]);

        if (dup2(pipe_in[0], STDIN_FILENO) < 0) {
            logMessage("dup2 stdin failed: " + std::string(strerror(errno)));
            _exit(1);
        }
        if (dup2(pipe_out[1], STDOUT_FILENO) < 0) {
            logMessage("dup2 stdout failed: " + std::string(strerror(errno)));
            _exit(1);
        }

        close(pipe_in[0]);
        close(pipe_out[1]);

        // Setze wichtige Umgebungsvariablen
        std::string script_path = "/app/var/www/php/index.php";
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
            "SCRIPT_NAME=/index.php",
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

        // Aufräumen falls execve fehlschlägt
        for (char* ptr : env) {
            free(ptr);
        }

        logMessage("execve failed: " + std::string(strerror(errno)));
        _exit(1);
    }

    // Parent process
    ss.str("");
    ss << "Parent process continued, child PID: " << pid;
    logMessage(ss.str());

    close(pipe_in[0]);
    close(pipe_out[1]);

    // Registriere die Pipes für epoll
    modEpoll(g_epfd, req.cgi_in_fd, EPOLLOUT);
    modEpoll(g_epfd, req.cgi_out_fd, EPOLLIN);

    g_fd_to_client[req.cgi_in_fd] = req.client_fd;
    g_fd_to_client[req.cgi_out_fd] = req.client_fd;

    req.cgi_pid = pid;
    req.state = RequestState::STATE_CGI_RUNNING;

    logMessage("CGI setup complete");
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

	std::stringstream ss;
	ss.str("");
	ss << "Response\n" << content;
	logMessage(ss.str());
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
		logMessage("here we dont !");
		req.state = RequestState::STATE_SENDING_RESPONSE;
		logMessage("here we dont go!");
		modEpoll(g_epfd, req.client_fd, EPOLLOUT);
		logMessage("here we dont go further!");
	}
}
static void handleClientRead(int epfd, int fd) {
	std::stringstream ss;
	ss << "Handling client read on fd " << fd;
	logMessage(ss.str());

	RequestState &req = g_requests[fd];
	char buf[1024];
	ssize_t n = read(fd, buf, sizeof(buf));

	if (n == 0) {
		logMessage("Client closed connection");
		delFromEpoll(epfd, fd);
		return;
	}

	if (n < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			logMessage("Read error: " + std::string(strerror(errno)));
			delFromEpoll(epfd, fd);
		}
		return;
	}

	ss.str("");
	//ss << "Read " << n << " bytes from client\n" << buf;
	ss << "Read " << n << " bytes from client";
	logMessage(ss.str());

	req.request_buffer.insert(req.request_buffer.end(), buf, buf + n);

	if (req.request_buffer.size() > 4) {
		std::string req_str(req.request_buffer.begin(), req.request_buffer.end());
		if (req_str.find("\r\n\r\n") != std::string::npos) {
			logMessage("Complete request received, parsing");
			parseRequest(req);

			if (!needsCGI(req.associated_conf, req.requested_path)) {
				logMessage("Processing as normal request");
				buildResponse(req);
				req.state = RequestState::STATE_SENDING_RESPONSE;
				modEpoll(epfd, fd, EPOLLOUT);
			} else {
				logMessage("Request requires CGI processing");
			}
		}
	}
}


static void handleClientWrite(int epfd, int fd) {
    std::stringstream ss;
    ss << "Handling client write on fd " << fd;
    logMessage(ss.str());

    RequestState &req = g_requests[fd];
    if (req.state == RequestState::STATE_SENDING_RESPONSE) {
        // Debug the response content
        ss.str("");
        ss << "Attempting to write response, buffer size: " << req.response_buffer.size()
           << ", content: " << std::string(req.response_buffer.begin(), req.response_buffer.end());
        logMessage(ss.str());

        // Make sure we're writing to a valid socket
        int error = 0;
        socklen_t len = sizeof(error);
        if (getsockopt(fd, SOL_SOCKET, SO_ERROR, &error, &len) < 0 || error != 0) {
            logMessage("Socket error detected: " + std::string(strerror(error)));
            delFromEpoll(epfd, fd);
            return;
        }

        ssize_t n = send(fd, req.response_buffer.data(), req.response_buffer.size(), MSG_NOSIGNAL);

        if (n > 0) {
            ss.str("");
            ss << "Successfully wrote " << n << " bytes to client";
            logMessage(ss.str());

            req.response_buffer.erase(
                req.response_buffer.begin(),
                req.response_buffer.begin() + n
            );

            if (req.response_buffer.empty()) {
                logMessage("Response fully sent, closing connection");
                delFromEpoll(epfd, fd);
            } else {
                // If there's more data to write, make sure we're still watching for EPOLLOUT
                modEpoll(epfd, fd, EPOLLOUT);
            }
        } else if (n < 0) {
            if (errno != EAGAIN && errno != EWOULDBLOCK) {
                ss.str("");
                ss << "Write error: " << strerror(errno);
                logMessage(ss.str());
                delFromEpoll(epfd, fd);
            } else {
                // If we would block, make sure we're still watching for EPOLLOUT
                modEpoll(epfd, fd, EPOLLOUT);
            }
        }
    }
}


static void handleCGIWrite(int epfd, int fd) {
	std::stringstream ss;
	ss << "CGI Write Event started for fd " << fd;
	logMessage(ss.str());

	if (g_fd_to_client.find(fd) == g_fd_to_client.end()) {
		logMessage("No client mapping found for fd " + std::to_string(fd));
		delFromEpoll(epfd, fd);
		return;
	}

	int client_fd = g_fd_to_client[fd];
	RequestState &req = g_requests[client_fd];

	if (req.request_buffer.empty()) {
		logMessage("No data to write, closing CGI input pipe");
		delFromEpoll(epfd, fd);
		close(fd);
		return;
	}

	ssize_t written = write(fd, req.request_buffer.data(), req.request_buffer.size());
	ss.str("");
	ss << "Write attempt returned " << written << " bytes";
	logMessage(ss.str());

	if (written > 0) {
		req.request_buffer.erase(
			req.request_buffer.begin(),
			req.request_buffer.begin() + written
		);

		if (req.request_buffer.empty()) {
			logMessage("All data written to CGI, closing input pipe");
			delFromEpoll(epfd, fd);
			close(fd);
		}
	} else if (written < 0) {
		if (errno != EAGAIN && errno != EWOULDBLOCK) {
			logMessage("Write error: " + std::string(strerror(errno)));
			delFromEpoll(epfd, fd);
			close(fd);
		}
	}
}

static void handleCGIRead(int epfd, int fd) {
    std::stringstream ss;
    ss << "CGI Read Event started for fd " << fd;
    logMessage(ss.str());

    if (g_fd_to_client.find(fd) == g_fd_to_client.end()) {
        logMessage("No client mapping found for fd " + std::to_string(fd));
        delFromEpoll(epfd, fd);
        return;
    }

    int client_fd = g_fd_to_client[fd];
    RequestState &req = g_requests[client_fd];

    char buf[4096];  // Größerer Buffer für CGI-Output
    ssize_t n = read(fd, buf, sizeof(buf));

    ss.str("");
    ss << "Read attempt returned " << n << " bytes";
    logMessage(ss.str());

    if (n > 0) {
        // Daten vom CGI-Skript empfangen
        req.cgi_output_buffer.insert(req.cgi_output_buffer.end(), buf, buf + n);
        ss.str("");
        ss << "Added " << n << " bytes to CGI output buffer, total size: "
           << req.cgi_output_buffer.size();
        logMessage(ss.str());
        return;  // Warten auf mehr Daten
    }

    if (n == 0) {  // EOF - CGI hat seine Ausgabe beendet
        logMessage("CGI EOF received, processing output");
        req.cgi_done = true;

        // Debug-Ausgabe des CGI-Outputs
        std::string debug_output(req.cgi_output_buffer.begin(), req.cgi_output_buffer.end());
        ss.str("");
        ss << "Raw CGI output:\n" << debug_output;
        logMessage(ss.str());

        if (!req.cgi_output_buffer.empty()) {
            std::string cgi_output(req.cgi_output_buffer.begin(), req.cgi_output_buffer.end());

            // Überprüfen, ob der CGI-Output bereits HTTP-Header enthält
            if (cgi_output.find("HTTP/1.1") != 0) {
                // Wenn nicht, fügen wir eigene Header hinzu
                std::string response = "HTTP/1.1 200 OK\r\n"
                                     "Content-Type: text/html\r\n"
                                     "Connection: close\r\n"
                                     "Content-Length: " + std::to_string(cgi_output.length()) +
                                     "\r\n\r\n" + cgi_output;
                req.response_buffer.assign(response.begin(), response.end());
            } else {
                // CGI-Output enthält bereits Header
                req.response_buffer = req.cgi_output_buffer;
            }

            ss.str("");
            ss << "Prepared response of size: " << req.response_buffer.size();
            logMessage(ss.str());

            // CGI-Pipe schließen
            delFromEpoll(epfd, fd);
            close(fd);
            req.cgi_out_fd = -1;

            // Zum Senden der Antwort wechseln
            req.state = RequestState::STATE_SENDING_RESPONSE;
            modEpoll(epfd, req.client_fd, EPOLLOUT);
        } else {
            logMessage("No CGI output received, sending error response");
            std::string error_response = "HTTP/1.1 500 Internal Server Error\r\n"
                                       "Content-Type: text/plain\r\n"
                                       "Content-Length: 21\r\n"
                                       "Connection: close\r\n\r\n"
                                       "CGI produced no output";
            req.response_buffer.assign(error_response.begin(), error_response.end());
            req.state = RequestState::STATE_SENDING_RESPONSE;

            delFromEpoll(epfd, fd);
            close(fd);
            req.cgi_out_fd = -1;

            modEpoll(epfd, req.client_fd, EPOLLOUT);
        }

        // CGI-Prozess aufräumen
        if (req.cgi_pid > 0) {
            int status;
            pid_t result = waitpid(req.cgi_pid, &status, WNOHANG);
            if (result > 0) {
                ss.str("");
                ss << "CGI process " << req.cgi_pid << " exited with status " << WEXITSTATUS(status);
                logMessage(ss.str());
            }
        }
    } else if (n < 0) {
        if (errno != EAGAIN && errno != EWOULDBLOCK) {
            logMessage("Read error: " + std::string(strerror(errno)));
            delFromEpoll(epfd, fd);
            close(fd);
            req.cgi_out_fd = -1;
        }
    }
}

int main(int argc, char **argv, char **envp)
{
ConfigHandler utils;
Server server;

	FILE* test_log = fopen("/app/tmp/webserv.log", "a");
	if (!test_log) {
		std::cerr << "Cannot open log file at startup: " << strerror(errno) << std::endl;
		return EXIT_FAILURE;
	}
	fprintf(test_log, "=== Server starting ===\n");
	fclose(test_log);
try {
	utils.parseArgs(argc, argv, envp);
	if (!utils.getconfigFileValid()) {
		Logger::red() << "Invalid configuration file!";
		logMessage("Invalid configuration file");
		return EXIT_FAILURE;
	}

	std::vector<ServerBlock> configs = utils.get_registeredServerConfs();
	if (configs.empty()) {
		Logger::red() << "No configurations found!";
		logMessage("No configurations found");
		return EXIT_FAILURE;
	}

	int epfd = epoll_create1(0);
	if (epfd < 0) {
		Logger::red() << "Failed to create epoll\n";
		logMessage("Failed to create epoll: " + std::string(strerror(errno)));
		return EXIT_FAILURE;
	}
	g_epfd = epfd;
	logMessage("Server starting");

	for (auto &conf : configs) {
		conf.server_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (conf.server_fd < 0) {
			std::stringstream ss;
			ss << "Failed to create socket on port: " << conf.port;
			Logger::red() << ss.str();
			logMessage(ss.str());
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
			logMessage(ss.str());
			close(conf.server_fd);
			return EXIT_FAILURE;
		}

		if (listen(conf.server_fd, SOMAXCONN) < 0) {
			std::stringstream ss;
			ss << "Failed to listen on port: " << conf.port;
			Logger::red() << ss.str();
			logMessage(ss.str());
			close(conf.server_fd);
			return EXIT_FAILURE;
		}

		setNonBlocking(conf.server_fd);
		modEpoll(epfd, conf.server_fd, EPOLLIN);

		std::stringstream ss;
		ss << "Server listening on port: " << conf.port;
		Logger::green() << ss.str() << "\n";
		logMessage(ss.str());
	}

	const int max_events = 64;
	struct epoll_event events[max_events];

	while (true) {
		int n = epoll_wait(epfd, events, max_events, -1);

		std::stringstream wer;
		wer << "epoll_wait n " << n;
		logMessage(wer.str());
		if (n < 0) {
			std::stringstream ss;
			ss << "epoll_wait error: " << strerror(errno);
			logMessage(ss.str());
			if (errno == EINTR)
				continue;
			break;
		}

		std::stringstream ss;
		ss << "Event Loop Iteration: " << n << " events";
		logMessage(ss.str());

		for (int i = 0; i < n; i++) {
			ss.str("");
			ss << "Event " << i << ": fd=" << events[i].data.fd
				<< ", events=" << events[i].events;
			logMessage(ss.str());

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
				if (client_fd < 0) {
					logMessage("Accept failed: " + std::string(strerror(errno)));
					continue;
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
				req.associated_conf = associated_conf;
				g_requests[client_fd] = req;

				ss.str("");
				ss << "New client connection on fd " << client_fd;
				logMessage(ss.str());
			}
			else {
				logMessage("req handling entered");
				//exit(1);
				if (g_requests.find(fd) == g_requests.end()) {
					logMessage("No request state for fd " + std::to_string(fd));
					continue;
				}

				RequestState &r = g_requests[fd];

				if (r.state == RequestState::STATE_CGI_RUNNING && r.cgi_pid > 0) {
					int status;
					pid_t result = waitpid(r.cgi_pid, &status, WNOHANG);
					if (result > 0) {
						ss.str("");
						ss << "CGI process " << r.cgi_pid
							<< " ended with status " << WEXITSTATUS(status);
						logMessage(ss.str());
					}
				}

				if (ev & EPOLLIN) {
					logMessage("req handling EPOLLIN");
					if (r.state == RequestState::STATE_READING_REQUEST) {
						logMessage("Handling client read on fd " + std::to_string(fd));
						handleClientRead(epfd, fd);
					}
					else if (r.state == RequestState::STATE_CGI_RUNNING && fd == r.cgi_out_fd) {
						logMessage("Handling CGI read on fd " + std::to_string(fd));
						handleCGIRead(epfd, fd);
					}
				}

				if (ev & EPOLLOUT) {
					logMessage("req handling EPOLLOUT");
					if (r.state == RequestState::STATE_SENDING_RESPONSE && fd == r.client_fd) {
						logMessage("Handling client write on fd " + std::to_string(fd));
						handleClientWrite(epfd, fd);
					}
					else if (r.state == RequestState::STATE_CGI_RUNNING && fd == r.cgi_in_fd) {
						logMessage("Handling CGI write on fd " + std::to_string(fd));
						handleCGIWrite(epfd, fd);
					}
				}

				if (ev & (EPOLLHUP | EPOLLERR)) {
					ss.str("");
					ss << "Error/Hangup on fd " << fd;
					logMessage(ss.str());
					delFromEpoll(epfd, fd);
				}
			}
		}
	}
	logMessage("Server shutting down");
	close(epfd);
}
catch (const std::exception &e) {
	std::string err_msg = "Error: " + std::string(e.what());
	Logger::red() << err_msg;
	logMessage(err_msg);
	return EXIT_FAILURE;
}
return EXIT_SUCCESS;
}