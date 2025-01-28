#include "main.hpp"
#include <csignal>
#include <iostream>
#include <memory>

std::string getEventDescription(uint32_t ev) {
    std::ostringstream description;

    if (ev & EPOLLIN) {
        description << "EPOLLIN ";
    }
    if (ev & EPOLLOUT) {
        description << "EPOLLOUT ";
    }
    if (ev & EPOLLHUP) {
        description << "EPOLLHUP ";
    }
    if (ev & EPOLLERR) {
        description << "EPOLLERR ";
    }
    if (ev & EPOLLRDHUP) {
        description << "EPOLLRDHUP ";
    }
    if (ev & EPOLLPRI) {
        description << "EPOLLPRI ";
    }
    if (ev & EPOLLET) {
        description << "EPOLLET ";
    }
    if (ev & EPOLLONESHOT) {
        description << "EPOLLONESHOT ";
    }

    // Remove the trailing space if there's any description
    std::string result = description.str();
    if (!result.empty() && result.back() == ' ') {
        result.pop_back();
    }

    return result.empty() ? "UNKNOWN EVENT" : result;
}

std::unique_ptr<Server> serverInstance;
void log_global_fds(const GlobalFDS& fds) {
    Logger::file("GlobalFDS epoll_fd: " + std::to_string(fds.epoll_fd));

    Logger::file("GlobalFDS clFD_to_svFD_map:");
    for (const auto& pair : fds.clFD_to_svFD_map) {
        Logger::file("GlobalFDS=  client_fd: " + std::to_string(pair.first) + " -> server_fd: " + std::to_string(pair.second));
    }
}
void handle_sigint(int sig)
{
	(void)sig;
	if (serverInstance)
	{
		Logger::cyan("\nCTRL+C received, shutting down gracefully...");
		serverInstance->cleanup();
		serverInstance.reset();  // Ensure cleanup
	}
	exit(EXIT_SUCCESS);
}

int main(int argc, char **argv, char **envp)
{
	ConfigHandler utils;
	GlobalFDS globalFDS;
	serverInstance = std::make_unique<Server>(globalFDS);
	//ClientHandler clientHandler(*serverInstance);
	//CgiHandler cgiHandler(*serverInstance);

	// Set up signal handling
	struct sigaction action;
	action.sa_handler = handle_sigint;  // Register the function
	sigemptyset(&action.sa_mask);
	action.sa_flags = 0;  // No special flags needed
	Logger::yellow("Server Starting...");
	if (sigaction(SIGINT, &action, NULL) == -1)
	{
		Logger::red() << "Failed to set signal handler!";
		return EXIT_FAILURE;
	}

	try
	{
		utils.parseArgs(argc, argv, envp);
		if (!utils.getconfigFileValid())
		{
			Logger::red() << "Invalid configuration file!";
			return EXIT_FAILURE;
		}

		std::vector<ServerBlock> configs = utils.get_registeredServerConfs();
		if (configs.empty())
		{
			Logger::red() << "No configurations found!";
			return EXIT_FAILURE;
		}

		int epoll_fd = serverInstance->server_init(configs);
		if (epoll_fd == EXIT_FAILURE)
			return EXIT_FAILURE;

		if (serverInstance->runEventLoop(epoll_fd, configs) == EXIT_FAILURE)
			return EXIT_FAILURE;
	}
	catch (const std::exception &e)
	{
		std::string err_msg = "Error: " + std::string(e.what());
		Logger::red() << err_msg;
		return EXIT_FAILURE;
	}

	return EXIT_SUCCESS;
}
