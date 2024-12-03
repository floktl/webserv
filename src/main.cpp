/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/26 12:58:21 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/03 10:08:50 by jeberle          ###   ########.fr       */
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
				perror("Error closing server socket");
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

		Logger::white() << "WEBSERV starting ...";
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



//int main(int argc, char **argv)
//{
//    ConfigHandler	utils;
//    Server	server;

//    try
//    {
//        utils.parseArgs(argc, argv); // Parse command-line arguments
//        if (utils.getconfigFileValid())
//        {
//            Logger::white() << "WEBSERV starting ...";
//        }

//        // Retrieve all registered configurations
//        std::vector<ServerBlock> configs = utils.get_registeredServerConfs();

//        // Check if there are any configurations
//        if (configs.empty())
//        {
//            std::cerr << "No configurations found!" << std::endl;
//            return EXIT_FAILURE;
//        }

//        // Use the port of the first configuration
//        int port = configs[0].port;

//        int server_fd = server.create_server_socket(port); // Set up server socket
//        std::cout << "Server listening on port " << port << std::endl;

//        // Start the server and begin handling connections
//        server.start(server_fd);

//        close(server_fd); // Close the server socket after exiting the loop
//    }
//    catch (const std::exception &e)
//    {
//        std::cerr << "Error: " << e.what() << std::endl;
//        return EXIT_FAILURE;
//    }

//    return EXIT_SUCCESS;
//}
