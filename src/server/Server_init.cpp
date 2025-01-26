#include "Server.hpp"

Server::Server(GlobalFDS &_globalFDS) :
	globalFDS(_globalFDS),
	clientHandler(new ClientHandler(*this)),
	cgiHandler(new CgiHandler(*this)),
	requestHandler(new RequestHandler(*this)),
	errorHandler(new ErrorHandler(*this)),
	taskManager(new TaskManager(*this)),
	timeout(30)
{
	//signal(SIGINT, Server::handle_sigint);
}

Server::~Server()
{
	delete clientHandler;
	delete cgiHandler;
	delete requestHandler;
	delete errorHandler;
	delete taskManager;
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
    Logger::yellow() << "\nServer is shutting down gracefully...";
    removeAddedServerNamesFromHosts();
}

ClientHandler* Server::getClientHandler(void) { return clientHandler; }
CgiHandler* Server::getCgiHandler(void) { return cgiHandler; }
RequestHandler* Server::getRequestHandler(void) { return requestHandler; }
ErrorHandler* Server::getErrorHandler(void) { return errorHandler; }
TaskManager* Server::getTaskManager(void) { return taskManager; }
GlobalFDS& Server::getGlobalFds(void) { return globalFDS; }

int Server::server_init(std::vector<ServerBlock> configs) {
	int epoll_fd = initEpoll();
	if (epoll_fd < 0)
		return EXIT_FAILURE;

	if (!initServerSockets(epoll_fd, configs))
		return EXIT_FAILURE;

	TaskManager* tm = getTaskManager();
	struct epoll_event ev;
	ev.events = EPOLLIN;
	ev.data.fd = tm->getPipeReadFd();
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, tm->getPipeReadFd(), &ev) < 0) {
		perror("epoll_ctl ADD TaskManager pipe_fd");
		return EXIT_FAILURE;
	}

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

bool Server::addServerNameToHosts(const std::string &server_name)
{
	const std::string hosts_file = "/etc/hosts";
	std::ifstream infile(hosts_file);
	std::string line;

	// Check if the server_name already exists in /etc/hosts
	while (std::getline(infile, line)) {
		if (line.find(server_name) != std::string::npos) {
			return true; // Already present, no need to add
		}
	}

	// Add server_name to /etc/hosts
	std::ofstream outfile(hosts_file, std::ios::app);
	if (!outfile.is_open()) {
		throw std::runtime_error("Failed to open /etc/hosts");
	}
	outfile << "127.0.0.1 " << server_name << "\n";
	outfile.close();

	// Store the added server name
	added_server_names.push_back(server_name);
	return true;
}

void Server::removeAddedServerNamesFromHosts()
{
	const std::string hosts_file = "/etc/hosts";
	std::ifstream infile(hosts_file);
	if (!infile.is_open()) {
		throw std::runtime_error("Failed to open /etc/hosts");
	}
	Logger::red("test");
	std::vector<std::string> lines;
	std::string line;
	while (std::getline(infile, line)) {
		bool shouldRemove = false;
		for (const auto &name : added_server_names) {
			if (line.find(name) != std::string::npos) {
				shouldRemove = true;
				break;
			}
		}
		if (!shouldRemove) {
			lines.push_back(line);
		}
	}
	infile.close();

	std::ofstream outfile(hosts_file, std::ios::trunc);
	if (!outfile.is_open()) {
		throw std::runtime_error("Failed to open /etc/hosts for writing");
	}
	for (const auto &l : lines) {
		outfile << l << "\n";
	}
	outfile.close();

	// Clear the added server names
	added_server_names.clear();
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

		setTimeout(conf.timeout);

		setNonBlocking(conf.server_fd);
		modEpoll(epoll_fd, conf.server_fd, EPOLLIN);
		// Add server_name to /etc/hosts
        try
		{
            if (!addServerNameToHosts(conf.name)) {
                Logger::file("Warning: Failed to add server_name to /etc/hosts\n");
            }
        }
		catch (const std::exception &e) {
            Logger::file(std::string("Error updating /etc/hosts: ") + e.what());
        }
		std::stringstream ss;
		ss << "Server listening on port: " << conf.port;
		Logger::green() << ss.str() << "\n";
	}
	return true;
}