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
	return staticHandler;
}

CgiHandler* Server::getCgiHandler(void)
{
	return cgiHandler;
}

RequestHandler* Server::getRequestHandler(void)
{
	return requestHandler;
}

ErrorHandler* Server::getErrorHandler(void)
{
	return errorHandler;
}

GlobalFDS& Server::getGlobalFds(void)
{
	return globalFDS;
}

int Server::initEpoll()
{
	int epoll_fd = epoll_create1(0);
	if (epoll_fd < 0)
	{
		Logger::red() << "Failed to create epoll\n";
		return -1;
	}
	globalFDS.epoll_fd = epoll_fd;
	return epoll_fd;
}

bool Server::isMethodAllowed(const RequestState &req, const std::string &method) const
{
	const Location* loc = requestHandler->findMatchingLocation(req.associated_conf, req.location_path);
	if (!loc)
		return false;

	if (method == "GET"    && loc->allowGet)    return true;
	if (method == "POST"   && loc->allowPost)   return true;
	if (method == "DELETE" && loc->allowDelete) return true;
	if (method == "COOKIE" && loc->allowCookie) return true;
	return false;
}

bool Server::initServerSockets(int epoll_fd, std::vector<ServerBlock> &configs)
{
	for (auto &conf : configs)
	{
		conf.server_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (conf.server_fd < 0)
		{
			std::stringstream ss;
			ss << "Failed to create socket on port: " << conf.port;
			Logger::red() << ss.str();
			return false;
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
			close(conf.server_fd);
			return false;
		}

		if (listen(conf.server_fd, SOMAXCONN) < 0)
		{
			close(conf.server_fd);
			return false;
		}

		setNonBlocking(conf.server_fd);
		modEpoll(epoll_fd, conf.server_fd, EPOLLIN);

		std::stringstream ss;
		ss << "Server listening on port: " << conf.port;
		Logger::green() << ss.str() << "\n";
	}
	return true;
}

bool Server::processMethod(RequestState &req, int epoll_fd)
{
	if (req.request_buffer.empty())
		return true;

	std::string method = requestHandler->getMethod(req.request_buffer);
	if (!isMethodAllowed(req, method))
	{
		std::string response = getErrorHandler()->generateErrorResponse(405, "Method Not Allowed", req);
		req.response_buffer.assign(response.begin(), response.end());
		req.state = RequestState::STATE_SENDING_RESPONSE;
		modEpoll(epoll_fd, req.client_fd, EPOLLOUT);
		return false;
	}
	return true;
}

bool Server::dispatchEvent(int epoll_fd, int fd, uint32_t ev, std::vector<ServerBlock> &configs)
{
	if (const ServerBlock* conf = findServerBlock(configs, fd))
	{
		handleNewConnection(epoll_fd, fd, *conf);
		return true;
	}

	if (globalFDS.svFD_to_clFD_map.find(fd) != globalFDS.svFD_to_clFD_map.end())
	{
		handleCGIEvent(epoll_fd, fd, ev);
		return true;
	}

	if (globalFDS.request_state_map.find(fd) != globalFDS.request_state_map.end())
	{
		handleClientEvent(epoll_fd, fd, ev);
		return true;
	}

	epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
	return false;
}

int Server::runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs)
{
	const int max_events = 64;
	struct epoll_event events[max_events];
	const int timeout_ms = 5000;

	while (true)
	{
		int n = epoll_wait(epoll_fd, events, max_events, timeout_ms);

		if (n < 0)
		{
			if (errno == EINTR)
				continue;
			break;
		}
		else if (n == 0)
		{
			continue;
		}

		for (int i = 0; i < n; i++)
		{
			int fd = events[i].data.fd;
			uint32_t ev = events[i].events;

			dispatchEvent(epoll_fd, fd, ev, configs);
		}
	}
	close(epoll_fd);
	return EXIT_SUCCESS;
}

const ServerBlock* Server::findServerBlock(const std::vector<ServerBlock> &configs, int fd)
{
	for (const auto &conf : configs)
	{
		if (conf.server_fd == fd)
			return &conf;
	}
	return nullptr;
}

bool Server::handleClientEvent(int epoll_fd, int fd, uint32_t ev)
{
	RequestState &req = globalFDS.request_state_map[fd];
	req.last_activity = std::chrono::steady_clock::now();

	if (ev & (EPOLLHUP | EPOLLERR))
	{
		delFromEpoll(epoll_fd, fd);
		return true;
	}

	if (ev & EPOLLIN)
	{
		staticHandler->handleClientRead(epoll_fd, fd);
		if (!processMethod(req, epoll_fd))
			return true;
	}

	if (req.state != RequestState::STATE_CGI_RUNNING &&
		req.state != RequestState::STATE_PREPARE_CGI &&
		(ev & EPOLLOUT))
	{
		staticHandler->handleClientWrite(epoll_fd, fd);
	}

	if ((req.state == RequestState::STATE_CGI_RUNNING ||
		 req.state == RequestState::STATE_PREPARE_CGI) &&
		(ev & EPOLLOUT) && !req.response_buffer.empty())
	{
		ssize_t written = write(fd, req.response_buffer.data(), req.response_buffer.size());
		if (written > 0)
		{
			req.response_buffer.erase(req.response_buffer.begin(), req.response_buffer.begin() + written);
			if (req.response_buffer.empty())
				delFromEpoll(epoll_fd, fd);
		}
		else if (written < 0 && errno != EAGAIN && errno != EWOULDBLOCK)
			delFromEpoll(epoll_fd, fd);
	}

	return true;
}

bool Server::handleCGIEvent(int epoll_fd, int fd, uint32_t ev)
{
	int client_fd = globalFDS.svFD_to_clFD_map[fd];
	RequestState &req = globalFDS.request_state_map[client_fd];

	if (ev & EPOLLIN)
		cgiHandler->handleCGIRead(epoll_fd, fd);

	if (!processMethod(req, epoll_fd))
		return true;

	if (fd == req.cgi_out_fd && (ev & EPOLLHUP))
	{
		if (!req.cgi_output_buffer.empty())
			finalizeCgiResponse(req, epoll_fd, client_fd);
		cgiHandler->cleanupCGI(req);
	}

	if (fd == req.cgi_in_fd)
	{
		if (ev & EPOLLOUT)
			cgiHandler->handleCGIWrite(epoll_fd, fd, ev);

		if (ev & (EPOLLHUP | EPOLLERR))
		{
			epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, NULL);
			close(fd);
			req.cgi_in_fd = -1;
			globalFDS.svFD_to_clFD_map.erase(fd);
		}
	}

	return true;
}

void Server::finalizeCgiResponse(RequestState &req, int epoll_fd, int client_fd)
{
	std::string output(req.cgi_output_buffer.begin(), req.cgi_output_buffer.end());

	size_t header_end = output.find("\r\n\r\n");
	std::string response;
	if (header_end != std::string::npos &&
		(output.find("Content-type:") != std::string::npos ||
		 output.find("Content-Type:") != std::string::npos))
	{
		std::string headers = output.substr(0, header_end);
		std::string body    = output.substr(header_end + 4);

		response  = "HTTP/1.1 200 OK\r\n";
		if (headers.find("Content-Length:") == std::string::npos)
			response += "Content-Length: " + std::to_string(body.length()) + "\r\n";
		if (headers.find("Connection:") == std::string::npos)
			response += "Connection: close\r\n";
		response += headers + "\r\n" + body;
	}
	else
	{
		response =
			"HTTP/1.1 200 OK\r\n"
			"Content-Type: text/html\r\n"
			"Content-Length: " + std::to_string(output.size()) + "\r\n"
			"Connection: close\r\n\r\n" + output;
	}

	req.response_buffer.assign(response.begin(), response.end());
	req.state = RequestState::STATE_SENDING_RESPONSE;
	modEpoll(epoll_fd, client_fd, EPOLLOUT);
}

int Server::server_init(std::vector<ServerBlock> configs)
{
	int epoll_fd = initEpoll();
	if (epoll_fd < 0)
		return EXIT_FAILURE;

	if (!initServerSockets(epoll_fd, configs))
		return EXIT_FAILURE;

	return runEventLoop(epoll_fd, configs);
}

void Server::handleNewConnection(int epoll_fd, int fd, const ServerBlock &conf)
{
	struct sockaddr_in client_addr;
	socklen_t client_len = sizeof(client_addr);
	int client_fd = accept(fd, (struct sockaddr*)&client_addr, &client_len);

	if (client_fd < 0)
	{
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
	epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL);

	std::map<int,int>::iterator it = globalFDS.svFD_to_clFD_map.find(fd);
	if (it != globalFDS.svFD_to_clFD_map.end())
	{
		int client_fd = it->second;
		RequestState &req = globalFDS.request_state_map[client_fd];

		if (fd == req.cgi_in_fd)
		{
			req.cgi_in_fd = -1;
		}
		if (fd == req.cgi_out_fd)
		{
			req.cgi_out_fd = -1;
		}

		if (req.cgi_in_fd == -1 && req.cgi_out_fd == -1 &&
			req.state != RequestState::STATE_SENDING_RESPONSE)
		{
			globalFDS.request_state_map.erase(client_fd);
		}

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