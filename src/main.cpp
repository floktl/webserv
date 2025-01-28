#include "main.hpp"
#include <csignal>
#include <iostream>
#include <memory>

std::unique_ptr<Server> serverInstance;

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
