#include "Server.hpp"

Server::Server(GlobalFDS &_globalFDS) :
	globalFDS(_globalFDS),
	// clientHandler(new ClientHandler(*this)),
	// cgiHandler(new CgiHandler(*this)),
	// requestHandler(new RequestHandler(*this)),
	errorHandler(new ErrorHandler(*this)),
	// taskManager(new TaskManager(*this)),
	timeout(30)
{
	//signal(SIGINT, Server::handle_sigint);
}

Server::~Server()
{
	// delete clientHandler;
	// delete cgiHandler;
	// delete requestHandler;
	delete errorHandler;
	// delete taskManager;
    Logger::yellow() << "\nCleaning up server resources...";
    //removeAddedServerNamesFromHosts();
}

//void Server::handle_sigint(int sig)
//{
//	(void)sig;
//	Logger::yellow() << "\nCaught SIGINT (CTRL+C). Shutting down gracefully...";
//	removeAddedServerNamesFromHosts();
//	exit(EXIT_SUCCESS);
//}

void Server::cleanup()
{
    removeAddedServerNamesFromHosts();
}

// ClientHandler* Server::getClientHandler(void) { return clientHandler; }
// CgiHandler* Server::getCgiHandler(void) { return cgiHandler; }
// RequestHandler* Server::getRequestHandler(void) { return requestHandler; }
ErrorHandler* Server::getErrorHandler(void) { return errorHandler; }
// TaskManager* Server::getTaskManager(void) { return taskManager; }
GlobalFDS& Server::getGlobalFds(void) { return globalFDS; }

int Server::server_init(std::vector<ServerBlock> configs) {
	int epoll_fd = initEpoll();
	if (epoll_fd < 0)
		return EXIT_FAILURE;

	if (!initServerSockets(epoll_fd, configs))
		return EXIT_FAILURE;

	//TaskManager* tm = getTaskManager();
	//struct epoll_event ev;
	//ev.events = EPOLLIN;
	//ev.data.fd = tm->getPipeReadFd();
	// if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tm->getPipeReadFd(), &ev) < 0) {
	// 	perror("epoll_ctl ADD TaskManager pipe_fd");
	// 	return EXIT_FAILURE;
	// }

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

bool Server::initServerSockets(int epoll_fd, std::vector<ServerBlock> &configs) {
   Logger::green("Server listening on the ports:");
   for (auto &conf : configs) {
       conf.server_fd = socket(AF_INET, SOCK_STREAM, 0);
       if (conf.server_fd < 0) {
           Logger::file("Socket error: " + std::string(strerror(errno)));
           return false;
       }

       int opt = 1;
       if (setsockopt(conf.server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
           Logger::file("Setsockopt error: " + std::string(strerror(errno)));
           return false;
       }

       struct sockaddr_in addr;
       std::memset(&addr, 0, sizeof(addr));
       addr.sin_family = AF_INET;
       addr.sin_port = htons(conf.port);
       addr.sin_addr.s_addr = htonl(INADDR_ANY);

       if (bind(conf.server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
           Logger::file("Bind error on port " + std::to_string(conf.port) +
                       ": " + std::string(strerror(errno)));
           close(conf.server_fd);
           return false;
       }

       if (listen(conf.server_fd, SOMAXCONN) < 0) {
           Logger::file("Listen error: " + std::string(strerror(errno)));
           close(conf.server_fd);
           return false;
       }

       if (setNonBlocking(conf.server_fd) < 0) {
           Logger::file("Failed to set non-blocking for fd " + std::to_string(conf.server_fd));
           return false;
       }

       modEpoll(epoll_fd, conf.server_fd, EPOLLIN | EPOLLET);

       setTimeout(conf.timeout);

       try {
           if (!addServerNameToHosts(conf.name)) {
               Logger::file("Warning: Failed to add server_name to /etc/hosts\n");
           }
       } catch (const std::exception &e) {
           Logger::file(std::string("Error updating /etc/hosts: ") + e.what());
       }

       Logger::green("Port: " + std::to_string(conf.port) + ", Servername: " + conf.name);
       Logger::file("Server socket " + std::to_string(conf.server_fd) +
                   " listening on port " + std::to_string(conf.port));
   }
   return true;
}