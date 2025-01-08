#include "main.hpp"

int main(int argc, char **argv, char **envp)
{
	ConfigHandler utils;
	GlobalFDS globalFDS;
	Server server(globalFDS);
	ClientHandler ClientHandler(server);
	CgiHandler cgiHandler(server);

	(void)server;
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
		if (server.server_init(configs) == EXIT_FAILURE)
			return (EXIT_FAILURE);
	}
	catch (const std::exception &e)
	{
		std::string err_msg = "Error: " + std::string(e.what());
		Logger::red() << err_msg;
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
