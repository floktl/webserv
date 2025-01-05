#include "Server.hpp"

Server::Server(GlobalFDS &_globalFDS) :
	globalFDS(_globalFDS),
	staticHandler(new StaticHandler(*this)),
	cgiHandler(new CgiHandler(*this)),
	requestHandler(new RequestHandler(*this)),
	errorHandler(new ErrorHandler(*this)),
	taskManager(new TaskManager(*this))
{}


Server::~Server()
{
    delete staticHandler;
    delete cgiHandler;
    delete requestHandler;
    delete errorHandler;
    delete taskManager;
}

StaticHandler*		Server::getStaticHandler(void) { return staticHandler; }
CgiHandler*			Server::getCgiHandler(void) { return cgiHandler; }
RequestHandler*		Server::getRequestHandler(void) { return requestHandler; }
ErrorHandler*		Server::getErrorHandler(void) { return errorHandler; }
TaskManager*		Server::getTaskManager(void) { return taskManager; }
GlobalFDS&			Server::getGlobalFds(void) { return globalFDS; }

int Server::server_init(std::vector<ServerBlock> configs)
{
	int epoll_fd = initEpoll();
	if (epoll_fd < 0)
		return EXIT_FAILURE;

	if (!initServerSockets(epoll_fd, configs))
		return EXIT_FAILURE;

	return runEventLoop(epoll_fd, configs);
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
