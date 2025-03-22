#include "./server/server.hpp"
#include <csignal>
#include <iostream>
#include <memory>

static sig_atomic_t g_shutdown_requested = 0;

static void handle_sigint(int sig)
{
	if (sig == SIGINT || sig == SIGTERM)
	{
		Logger::cyan("\n\nTerminated Server by signal - shutting down gracefully...");
		g_shutdown_requested = 1;
	}
}

int main(int argc, char **argv, char **envp)
{
	GlobalFDS globalFDS;
	Server serverInstance(globalFDS, g_shutdown_requested);
	ConfigHandler utils(&serverInstance);
	g_shutdown_requested = 0;

	Logger::yellow("Server Starting...");

	if (signal(SIGINT, handle_sigint) == SIG_ERR)
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
			serverInstance.removeAddedServerNamesFromHosts();
			return EXIT_FAILURE;
		}

		std::vector<ServerBlock> configs = utils.get_registeredServerConfs();
		if (configs.empty())
		{
			Logger::red() << "No configurations found!";
			serverInstance.removeAddedServerNamesFromHosts();
			return EXIT_FAILURE;
		}
		int epoll_fd = serverInstance.server_init(configs);
		serverInstance.environment = envp;
		if (epoll_fd == EXIT_FAILURE)
		{
			serverInstance.removeAddedServerNamesFromHosts();
			return EXIT_FAILURE;
		}

		if (serverInstance.runEventLoop(epoll_fd, configs) == EXIT_FAILURE)
		{
			serverInstance.removeAddedServerNamesFromHosts();
			return EXIT_FAILURE;
		}
	}
	catch (const std::exception &e)
	{
		std::string err_msg = "Error: " + std::string(e.what());
		Logger::red() << err_msg;
		serverInstance.removeAddedServerNamesFromHosts();
		return EXIT_FAILURE;
	}
	serverInstance.removeAddedServerNamesFromHosts();
	return EXIT_SUCCESS;
}
