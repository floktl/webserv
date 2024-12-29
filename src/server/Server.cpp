#include "Server.hpp"

Server::Server(GlobalFDS &_globalFDS) :
	globalFDS(_globalFDS),
	staticHandler(new StaticHandler(*this)),
	cgiHandler(new CgiHandler(*this)),
	requestHandler(new RequestHandler(*this)),
	errorHandler(new ErrorHandler(*this))
{}

Server::~Server()
{
	delete staticHandler;
	delete cgiHandler;
	delete requestHandler;
	delete errorHandler;
}

StaticHandler* Server::getStaticHandler(void)
{
	return (staticHandler);
}

CgiHandler* Server::getCgiHandler(void)
{
	return (cgiHandler);
}

RequestHandler* Server::getRequestHandler(void)
{
	return (requestHandler);
}

ErrorHandler* Server::getErrorHandler(void)
{
	return (errorHandler);
}

GlobalFDS& Server::getGlobalFds(void)
{
	return globalFDS;
}

int Server::server_init(std::vector<ServerBlock> configs)
{
	int epoll_fd = epoll_create1(0);

	if (epoll_fd < 0)
	{
		Logger::red() << "Failed to create epoll\n";
		Logger::file("Failed to create epoll: " + std::string(strerror(errno)));
		return EXIT_FAILURE;
	}
	globalFDS.epoll_fd = epoll_fd;
	Logger::file("Server starting");

	for (auto &conf : configs)
	{
		conf.server_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (conf.server_fd < 0)
		{
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

		if (bind(conf.server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		{
			std::stringstream ss;
			ss << "Failed to bind socket on port: " << conf.port;
			Logger::red() << ss.str();
			Logger::file(ss.str());
			close(conf.server_fd);
			return EXIT_FAILURE;
		}

		if (listen(conf.server_fd, SOMAXCONN) < 0)
		{
			std::stringstream ss;
			ss << "Failed to listen on port: " << conf.port;
			Logger::red() << ss.str();
			Logger::file(ss.str());
			close(conf.server_fd);
			return EXIT_FAILURE;
		}

		setNonBlocking(conf.server_fd);
		modEpoll(epoll_fd, conf.server_fd, EPOLLIN);

		std::stringstream ss;
		ss << "Server listening on port: " << conf.port;
		Logger::green() << ss.str() << "\n";
		Logger::file(ss.str());
	}
	const int max_events = 64;
	struct epoll_event events[max_events];

	const int timeout_ms = 5000;
	while (true)
	{
		int n = epoll_wait(epoll_fd, events, max_events, timeout_ms);

		std::stringstream wer;
		wer << "epoll_wait n " << n;
		Logger::file(wer.str());
		if (n < 0)
		{
			std::stringstream ss;
			ss << "epoll_wait error: " << strerror(errno);
			Logger::file(ss.str());
			if (errno == EINTR)
				continue;
			break;
		}
		else if (n == 0)
		{
			Logger::file("timeout");
			continue;
		}
		std::stringstream ewrew;
		ewrew << "Event Loop Iteration: " << n << " events";
		Logger::file(ewrew.str());

		for (int i = 0; i < n; i++)
		{
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
			for (const auto &conf : configs)
			{
				if (conf.server_fd == fd)
				{
					associated_conf = &conf;
					break;
				}
			}

			if (associated_conf)
			{
				handleNewConnection(epoll_fd, fd, *associated_conf);
				continue;
			}

			auto client_it = globalFDS.request_state_map.find(fd);
			if (client_it != globalFDS.request_state_map.end())
			{
				RequestState &req = client_it->second;
				req.last_activity =  std::chrono::steady_clock::now();

				if (ev & (EPOLLHUP | EPOLLERR))
				{
					Logger::file("Client socket error/hangup detected");
					delFromEpoll(epoll_fd, fd);
					continue;
				}

				if (req.state == RequestState::STATE_CGI_RUNNING ||
					req.state == RequestState::STATE_PREPARE_CGI)
				{
					if (ev & EPOLLIN)
					{
						staticHandler->handleClientRead(epoll_fd, fd);
					}
					if (ev & EPOLLOUT && !req.response_buffer.empty())
					{
						ssize_t written = write(fd, req.response_buffer.data(), req.response_buffer.size());
						if (written > 0)
						{
							req.response_buffer.erase(req.response_buffer.begin(),
													req.response_buffer.begin() + written);
							if (req.response_buffer.empty())
							{
								delFromEpoll(epoll_fd, fd);
							}
						}
						else if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
						{
							delFromEpoll(epoll_fd, fd);
						}
					}
				}
				else
				{
					if (ev & EPOLLIN) staticHandler->handleClientRead(epoll_fd, fd);
					if (ev & EPOLLOUT) staticHandler->handleClientWrite(epoll_fd, fd);
				}
				continue;
			}

			auto cgi_it = globalFDS.svFD_to_clFD_map.find(fd);
			if (cgi_it != globalFDS.svFD_to_clFD_map.end())
			{
				int client_fd = cgi_it->second;
				RequestState &req = globalFDS.request_state_map[client_fd];

				ss.str("");
				ss << "CGI pipe event:\n"
				<< "- Related client_fd: " << client_fd << "\n"
				<< "- CGI in_fd: " << req.cgi_in_fd << "\n"
				<< "- CGI out_fd: " << req.cgi_out_fd << "\n"
				<< "- Current state: " << req.state;
				Logger::file(ss.str());

				if (fd == req.cgi_out_fd)
				{
					if (ev & EPOLLIN)
					{
						cgiHandler->handleCGIRead(epoll_fd, fd);
					}
					if (ev & EPOLLHUP)
					{
						Logger::file("CGI output pipe hangup detected, finalizing response");
						if (!req.cgi_output_buffer.empty())
						{
							std::string output(req.cgi_output_buffer.begin(), req.cgi_output_buffer.end());
							ss.str("");
							ss << "Preparing final response with " << output.length() << " bytes";
							Logger::file(ss.str());

							std::string response;
							size_t header_end = output.find("\r\n\r\n");

							if (output.find("Content-type:") != std::string::npos ||
								output.find("Content-Type:") != std::string::npos)
							{
								std::string headers = output.substr(0, header_end);
								std::string body = output.substr(header_end + 4);

								response = "HTTP/1.1 200 OK\r\n";
								if (headers.find("Content-Length:") == std::string::npos)
								{
									response += "Content-Length: " + std::to_string(body.length()) + "\r\n";
								}
								if (headers.find("Connection:") == std::string::npos)
								{
									response += "Connection: close\r\n";
								}
								response += headers + "\r\n" + body;
							}
							else
							{
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
							modEpoll(epoll_fd, client_fd, EPOLLOUT);
						}
						cgiHandler->cleanupCGI(req);
					}
				}
				else if (fd == req.cgi_in_fd)
				{
					if (ev & EPOLLOUT)
					{
						cgiHandler->handleCGIWrite(epoll_fd, fd, ev);
					}
					if (ev & (EPOLLHUP | EPOLLERR))
					{
						Logger::file("CGI input pipe error/hangup detected");
						epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
						close(fd);
						req.cgi_in_fd = -1;
						globalFDS.svFD_to_clFD_map.erase(fd);
					}
				}
				continue;
			}

			Logger::file("Unknown fd encountered, removing from epoll");
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
			Logger::file("=== Event Processing End ===\n");
		}
	}
	Logger::file("Server shutting down");
	close(epoll_fd);
	return EXIT_SUCCESS;
}

void Server::handleNewConnection(int epoll_fd, int fd, const ServerBlock& conf)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(fd, (struct sockaddr*)&client_addr, &client_len);

	if (client_fd < 0)
	{
		Logger::file("Accept failed: " + std::string(strerror(errno)));
		return;
	}

	setNonBlocking(client_fd);
	modEpoll(epoll_fd, client_fd, EPOLLIN);

	RequestState req;
	req.client_fd = client_fd;
	req.cgi_in_fd = -1;
	req.cgi_out_fd = -1;
	req.cgi_pid = -1;
	req.state = RequestState::STATE_READING_REQUEST;
	req.cgi_done = false;
	req.associated_conf = &conf;
	globalFDS.request_state_map[client_fd] = req;

	std::stringstream ss;
	ss << "New client connection on fd " << client_fd;
	Logger::file(ss.str());
}



void Server::modEpoll(int epfd, int fd, uint32_t events)
{
	struct epoll_event ev;
	std::memset(&ev, 0, sizeof(ev));
	ev.events = events;
	ev.data.fd = fd;

	if (epoll_ctl(epfd, EPOLL_CTL_MOD, fd, &ev) < 0)
	{
		if (errno == ENOENT)
		{
			if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev) < 0)
			{
				Logger::file("epoll_ctl ADD failed: " + std::string(strerror(errno)));
			}
		}
		else
		{
			Logger::file("epoll_ctl MOD failed: " + std::string(strerror(errno)));
		}
	}
}


void Server::delFromEpoll(int epfd, int fd)
{
	std::stringstream ss;
	ss << "Removing fd " << fd << " from epoll";
	Logger::file(ss.str());

	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);

	std::map<int,int>::iterator it = globalFDS.svFD_to_clFD_map.find(fd);
	if (it != globalFDS.svFD_to_clFD_map.end())
	{
		int client_fd = it->second;
		RequestState &req = globalFDS.request_state_map[client_fd];

		ss.str("");
		ss << "Found associated client fd " << client_fd;
		Logger::file(ss.str());

		if (fd == req.cgi_in_fd)
		{
			Logger::file("Closing CGI input pipe");
			req.cgi_in_fd = -1;
		}
		if (fd == req.cgi_out_fd)
		{
			Logger::file("Closing CGI output pipe");
			req.cgi_out_fd = -1;
		}

		if (req.cgi_in_fd == -1 && req.cgi_out_fd == -1 &&
			req.state != RequestState::STATE_SENDING_RESPONSE)
		{
			Logger::file("Cleaning up request state");
			globalFDS.request_state_map.erase(client_fd);
		}

		globalFDS.svFD_to_clFD_map.erase(it);
	}
	else
	{
		RequestState &req = globalFDS.request_state_map[fd];
		Logger::file("Cleaning up client connection resources");

		if (req.cgi_in_fd != -1)
		{
			ss.str("");
			ss << "Cleaning up CGI input pipe " << req.cgi_in_fd;
			Logger::file(ss.str());
			epoll_ctl(epfd, EPOLL_CTL_DEL, req.cgi_in_fd, NULL);
			close(req.cgi_in_fd);
			globalFDS.svFD_to_clFD_map.erase(req.cgi_in_fd);
		}
		if (req.cgi_out_fd != -1)
		{
			ss.str("");
			ss << "Cleaning up CGI output pipe " << req.cgi_out_fd;
			Logger::file(ss.str());
			epoll_ctl(epfd, EPOLL_CTL_DEL, req.cgi_out_fd, NULL);
			close(req.cgi_out_fd);
			globalFDS.svFD_to_clFD_map.erase(req.cgi_out_fd);
		}

		globalFDS.request_state_map.erase(fd);
	}

	close(fd);
	ss.str("");
	ss << "Closed fd " << fd;
	Logger::file(ss.str());
}

int Server::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}