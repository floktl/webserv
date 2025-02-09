#include "Server.hpp"

// Constructor initializing the Server with a reference to GlobalFDS and setting default values
Server::Server(GlobalFDS &_globalFDS) : globalFDS(_globalFDS),
										// cgiHandler(new CgiHandler(*this)),
										errorHandler(new ErrorHandler(*this)),
										timeout(30)
{
}

// Destructor cleaning up allocated resources and logging server shutdown
Server::~Server()
{
	// delete cgiHandler;
	delete errorHandler;
	Logger::yellow() << "\nCleaning up server resources...";
}

// Sets the server timeout value
void Server::setTimeout(int t)
{
	timeout = t;
}

// Retrieves the current server timeout value
int Server::getTimeout() const
{
	return timeout;
}

// Performs cleanup operations such as removing added server names from /etc/hosts
void Server::cleanup()
{
	removeAddedServerNamesFromHosts();
}

// CgiHandler* Server::getCgiHandler(void)
//{
//	return cgiHandler;
//}

// Returns a pointer to the error handler instance
ErrorHandler *Server::getErrorHandler(void)
{
	return errorHandler;
}

// Returns a reference to the global file descriptor structure
GlobalFDS &Server::getGlobalFds(void)
{
	return globalFDS;
}

// Initializes the server by setting up epoll and server sockets, then starts the event loop
int Server::server_init(std::vector<ServerBlock> configs)
{
	int epoll_fd = initEpoll();
	if (epoll_fd < 0)
		return EXIT_FAILURE;

	if (!initServerSockets(epoll_fd, configs))
		return EXIT_FAILURE;
	configData = configs;
	return runEventLoop(epoll_fd, configs);
}

// Creates an epoll instance and stores its file descriptor in GlobalFDS
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

// Initializes server sockets based on configuration, binds them, and adds them to epoll
bool Server::initServerSockets(int epoll_fd, std::vector<ServerBlock> &configs)
{
	Logger::green("Server listening on the ports:");
	for (auto &conf : configs)
	{
		conf.server_fd = socket(AF_INET, SOCK_STREAM, 0);
		if (conf.server_fd < 0)
			return false;

		int opt = 1;
		if (setsockopt(conf.server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
			return false;

		struct sockaddr_in addr;
		std::memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(conf.port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);

		if (bind(conf.server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
		{
			close(conf.server_fd);
			conf.server_fd = -1;
			return false;
		}

		if (listen(conf.server_fd, SOMAXCONN) < 0)
		{
			close(conf.server_fd);
			conf.server_fd = -1;
			return false;
		}

		if (setNonBlocking(conf.server_fd) < 0)
			return false;

		modEpoll(epoll_fd, conf.server_fd, EPOLLIN | EPOLLET);
		setTimeout(conf.timeout);
		addServerNameToHosts(conf.name);
		if ((conf.port == 80 && serverInstance->has_gate) || conf.port != 80)
			Logger::green("Port: " + std::to_string(conf.port) + ", Servername: " + conf.name);
	}
	return true;
}
