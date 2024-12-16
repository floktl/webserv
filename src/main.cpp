/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/26 12:58:21 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/16 16:07:31 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "main.hpp"

int main(int argc, char **argv, char **envp)
{
	ConfigHandler utils;
	GlobalFDS globalFDS;
	Server server(globalFDS);
	StaticHandler staticHandler(server);
	CgiHandler cgiHandler(server);
	RequestHandler requestHandler(server);
	ErrorHandler errorHandler(server);

	(void)server;
	try
	{
		utils.parseArgs(argc, argv, envp);
		if (!utils.getconfigFileValid())
		{
			Logger::red() << "Invalid configuration file!";
			Logger::file("Invalid configuration file");
			return EXIT_FAILURE;
		}

		std::vector<ServerBlock> configs = utils.get_registeredServerConfs();
		if (configs.empty())
		{
			Logger::red() << "No configurations found!";
			Logger::file("No configurations found");
			return EXIT_FAILURE;
		}
		if (server.server_init(configs) == EXIT_FAILURE)
			return (EXIT_FAILURE);
	}
	catch (const std::exception &e)
	{
		std::string err_msg = "Error: " + std::string(e.what());
		Logger::red() << err_msg;
		Logger::file(err_msg);
		return EXIT_FAILURE;
	}
	return EXIT_SUCCESS;
}
