/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/26 12:58:21 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/06 08:41:40 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./utils/ConfigHandler.hpp"
#include "./utils/Logger.hpp"

void closeServerSockets( std::vector<ServerBlock> configs)
{
	for (auto& conf : configs) // Use a reference to modify each ServerBlock
		{
			if (close(conf.server_fd) < 0)
			{
				std::perror("Error closing server socket");
			}
			else
			{
				std::cout << "Closed server socket: " << conf.server_fd << std::endl;
			}
		}
}

int main(int argc, char **argv, char **envp)
{
	ConfigHandler utils;
	Server server;

	try
	{
		// Parse command-line arguments and validate configuration file
		utils.parseArgs(argc, argv, envp);
		if (!utils.getconfigFileValid())
		{
			Logger::red() << "Invalid configuration file!";
			return EXIT_FAILURE;
		}

		// Retrieve and validate configurations
		std::vector<ServerBlock> configs = utils.get_registeredServerConfs();
		if (configs.empty())
		{
			Logger::red() << "No configurations found!";
			return EXIT_FAILURE;
		}

		// Create server sockets for each configuration
		for (auto& conf : configs) {
			conf.server_fd = server.create_server_socket(conf.port);
			if (conf.server_fd < 0)
			{
				Logger::red() << "Failed to create socket on port: " << conf.port;
				closeServerSockets(configs);
				return EXIT_FAILURE;
			}
			Logger::green() << "Server listening on port: " << conf.port;
		}

		// Start server
		server.start(configs);

		// Clean up server sockets
		closeServerSockets(configs);
	}
	catch (const std::exception &e)
	{
		Logger::red() << "Error: " << e.what();
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
