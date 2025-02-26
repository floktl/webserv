#include "Server.hpp"

// Constructor Initializing The Server With A Reference to Globalfds and Setting Default Values
Server::Server(GlobalFDS &_globalFDS, int &_g_shutdown_requested) :
											g_shutdown_requested(_g_shutdown_requested),
											globalFDS(_globalFDS),
											errorHandler(new ErrorHandler(*this)),
											timeout(30)
{
}

// Destructor Cleaning Up Allocated Resources and Logging Server Shutdown
Server::~Server()
{
	delete errorHandler;
	Logger::yellow() << "\nCleaning up server resources...";
}

// Sets the Server Timeout Value
void Server::setTimeout(int t)
{
	timeout = t;
}

// Performs Cleanup Operations Search as Removing Added Server Names from /etc /hosts
void Server::cleanup()
{
	removeAddedServerNamesFromHosts();
}

// CGISCHAUSLER* Server :: Getcgihandler (void)
// None
// Return Cgihandler;
// None

// Returns a Pointer to the Error Handler Instance
ErrorHandler *Server::getErrorHandler(void)
{
	return errorHandler;
}

// Returns a Reference to the Global File Descriptor Structure
GlobalFDS &Server::getGlobalFds(void)
{
	return globalFDS;
}


// Initializes the server by Setting Up Epoll and Server Sicke, Then Starts The Event Loop
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

// Creates to Epoll Instance and Stores Its File Descriptor in Globalfds
int Server::initEpoll() {

	int epoll_fd = epoll_create(1);
	if (epoll_fd < 0) {
		Logger::errorLog("Failed to create epoll: " + std::string(strerror(errno)));
		return -1;
	}

	int flags = fcntl(epoll_fd, F_GETFD);

	if (flags != -1) {
		flags |= FD_CLOEXEC;
		if (fcntl(epoll_fd, F_SETFD, flags) == -1) {
			Logger::errorLog("Failed to set FD_CLOEXEC: " + std::string(strerror(errno)));
			close(epoll_fd);
			return -1;
		}
	} else {
		Logger::errorLog("Failed to get initial flags: " + std::string(strerror(errno)));
	}

	int verify_flags = fcntl(epoll_fd, F_GETFD);

	if (verify_flags == -1) {
		Logger::errorLog("epoll_fd invalid after creation: " + std::string(strerror(errno)));
		close(epoll_fd);
		return -1;
	}

	if (!(verify_flags & FD_CLOEXEC)) {
		Logger::errorLog("WARNING: FD_CLOEXEC not set in verification");
	}

	globalFDS.epoll_fd = epoll_fd;

	return epoll_fd;
}

// Initializes Server Sickets Based on Configuration, Binds Them, and Adds Them to Epoll
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
		if ((conf.port == 80 && this->has_gate) || conf.port != 80)
			Logger::green("Port: " + std::to_string(conf.port) + ", Servername: " + conf.name);
	}
	return true;
}