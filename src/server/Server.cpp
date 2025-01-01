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

int Server::initializeEpoll() {
	int epoll_fd = epoll_create1(0);
	if (epoll_fd < 0) {
		Logger::red() << "Failed to create epoll\n";
		return -1;
	}
	globalFDS.epoll_fd = epoll_fd;
	return epoll_fd;
}

int Server::setupServerSocket(ServerBlock& conf) {
	conf.server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (conf.server_fd < 0) {
		Logger::red() << "Failed to create socket on port: " << conf.port;
		return -1;
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
		return -1;
	}

	if (listen(conf.server_fd, SOMAXCONN) < 0) {
		Logger::red() << "Failed to listen on port: " << conf.port;
		close(conf.server_fd);
		return -1;
	}

	setNonBlocking(conf.server_fd);
	Logger::green() << "Server listening on port: " << conf.port << "\n";
	return conf.server_fd;
}

void Server::handleCGIEvent(int epoll_fd, int fd, uint32_t ev) {
	int client_fd = globalFDS.svFD_to_clFD_map[fd];
	RequestState &req = globalFDS.request_state_map[client_fd];

	if (fd == req.cgi_out_fd) {
		if (ev & EPOLLIN) {
			cgiHandler->handleCGIRead(epoll_fd, fd);
		}
		if (ev & EPOLLHUP) {
			if (!req.cgi_output_buffer.empty()) {
				std::string output(req.cgi_output_buffer.begin(), req.cgi_output_buffer.end());
				std::string response;
				size_t header_end = output.find("\r\n\r\n");

				if (output.find("Content-type:") != std::string::npos ||
					output.find("Content-Type:") != std::string::npos) {
					std::string headers = output.substr(0, header_end);
					std::string body = output.substr(header_end + 4);
					response = "HTTP/1.1 200 OK\r\n";
					if (headers.find("Content-Length:") == std::string::npos)
						response += "Content-Length: " + std::to_string(body.length()) + "\r\n";
					if (headers.find("Connection:") == std::string::npos)
						response += "Connection: close\r\n";
					response += headers + "\r\n" + body;
				} else {
					response = "HTTP/1.1 200 OK\r\n"
							"Content-Type: text/html\r\n"
							"Content-Length: " + std::to_string(output.length()) + "\r\n"
							"Connection: close\r\n"
							"\r\n" + output;
				}
				req.response_buffer.assign(response.begin(), response.end());
				req.state = RequestState::STATE_SENDING_RESPONSE;
				modEpoll(epoll_fd, client_fd, EPOLLOUT);
			}
			cgiHandler->cleanupCGI(req);
		}
	}
	else if (fd == req.cgi_in_fd) {
		if (ev & EPOLLOUT) {
			cgiHandler->handleCGIWrite(epoll_fd, fd, ev);
		}
		if (ev & (EPOLLHUP | EPOLLERR)) {
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
			close(fd);
			req.cgi_in_fd = -1;
			globalFDS.svFD_to_clFD_map.erase(fd);
		}
	}
}

void Server::handleClientEvent(int epoll_fd, int fd, uint32_t ev, RequestState &req) {
	req.last_activity = std::chrono::steady_clock::now();

	if (ev & (EPOLLHUP | EPOLLERR)) {
		delFromEpoll(epoll_fd, fd);
		return;
	}

	if (ev & EPOLLIN) {
		staticHandler->handleClientRead(epoll_fd, fd);
	}

	if (ev & EPOLLOUT) {
		if (req.state == RequestState::STATE_CGI_RUNNING ||
			req.state == RequestState::STATE_PREPARE_CGI) {
			if (!req.response_buffer.empty()) {
				ssize_t written = write(fd, req.response_buffer.data(), req.response_buffer.size());
				if (written > 0) {
					req.response_buffer.erase(req.response_buffer.begin(),
											req.response_buffer.begin() + written);
					if (req.response_buffer.empty()) {
						delFromEpoll(epoll_fd, fd);
					}
				}
				else if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK) {
					delFromEpoll(epoll_fd, fd);
				}
			}
		} else {
			const Location* loc = requestHandler->findMatchingLocation(req.associated_conf, req.location_path);

			Logger::file("handleClientEvent allowGet " + std::to_string(loc->allowGet));
			Logger::file("handleClientEvent allowPost " + std::to_string(loc->allowPost));
			Logger::file("handleClientEvent allowDelete " + std::to_string(loc->allowDelete));
			Logger::file("handleClientEvent allowCookie " + std::to_string(loc->allowCookie));
			staticHandler->handleClientWrite(epoll_fd, fd);
		}
	}
}

int Server::server_init(std::vector<ServerBlock> configs) {
	int epoll_fd = initializeEpoll();
	if (epoll_fd < 0) return EXIT_FAILURE;

	for (auto &conf : configs) {
		if (setupServerSocket(conf) < 0) return EXIT_FAILURE;
		modEpoll(epoll_fd, conf.server_fd, EPOLLIN);
	}

	const int max_events = 64;
	struct epoll_event events[max_events];
	const int timeout_ms = 5000;

	while (true) {
		int n = epoll_wait(epoll_fd, events, max_events, timeout_ms);
		if (n < 0) {
			if (errno == EINTR) continue;
			break;
		}
		if (n == 0) continue;

		for (int i = 0; i < n; i++) {
			int fd = events[i].data.fd;
			uint32_t ev = events[i].events;

			const ServerBlock* associated_conf = nullptr;
			for (const auto &conf : configs) {
				if (conf.server_fd == fd) {
					associated_conf = &conf;
					break;
				}
			}

			if (associated_conf) {
				handleNewConnection(epoll_fd, fd, *associated_conf);
				continue;
			}

			auto client_it = globalFDS.request_state_map.find(fd);
			if (client_it != globalFDS.request_state_map.end()) {
				handleClientEvent(epoll_fd, fd, ev, client_it->second);
				continue;
			}

			auto cgi_it = globalFDS.svFD_to_clFD_map.find(fd);
			if (cgi_it != globalFDS.svFD_to_clFD_map.end()) {
				handleCGIEvent(epoll_fd, fd, ev);
				continue;
			}

			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
		}
	}

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
				Logger::file("epoll_ctl ADD failed: " + std::string(strerror(errno)));
		}
		else
			Logger::file("epoll_ctl MOD failed: " + std::string(strerror(errno)));
	}
}


void Server::delFromEpoll(int epfd, int fd)
{
	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);

	std::map<int,int>::iterator it = globalFDS.svFD_to_clFD_map.find(fd);
	if (it != globalFDS.svFD_to_clFD_map.end())
	{
		int client_fd = it->second;
		RequestState &req = globalFDS.request_state_map[client_fd];

		if (fd == req.cgi_in_fd)
			req.cgi_in_fd = -1;
		if (fd == req.cgi_out_fd)
			req.cgi_out_fd = -1;

		if (req.cgi_in_fd == -1 && req.cgi_out_fd == -1 && req.state != RequestState::STATE_SENDING_RESPONSE)
			globalFDS.request_state_map.erase(client_fd);

		globalFDS.svFD_to_clFD_map.erase(it);
	}
	else
	{
		RequestState &req = globalFDS.request_state_map[fd];
		if (req.cgi_in_fd != -1)
		{
			epoll_ctl(epfd, EPOLL_CTL_DEL, req.cgi_in_fd, NULL);
			close(req.cgi_in_fd);
			globalFDS.svFD_to_clFD_map.erase(req.cgi_in_fd);
		}
		if (req.cgi_out_fd != -1)
		{
			epoll_ctl(epfd, EPOLL_CTL_DEL, req.cgi_out_fd, NULL);
			close(req.cgi_out_fd);
			globalFDS.svFD_to_clFD_map.erase(req.cgi_out_fd);
		}

		globalFDS.request_state_map.erase(fd);
	}
	close(fd);
}

int Server::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}