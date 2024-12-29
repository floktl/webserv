/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/26 12:58:21 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/29 11:29:54 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "main.hpp"

void printServerBlock(const ServerBlock& serverBlock)
{
    std::cout << "---- ServerBlock Information ----" << std::endl;

    // Print basic fields
    std::cout << "Port: " << serverBlock.port << std::endl;
    std::cout << "Server FD: " << serverBlock.server_fd << std::endl;
    std::cout << "Name: " << serverBlock.name << std::endl;
    std::cout << "Root: " << serverBlock.root << std::endl;
    std::cout << "Index: " << serverBlock.index << std::endl;
    std::cout << "Client Max Body Size: " << serverBlock.client_max_body_size << std::endl;

    // Print errorPages map
    std::cout << "Error Pages:" << std::endl;
    for (std::map<int, std::string>::const_iterator it = serverBlock.errorPages.begin();
		it != serverBlock.errorPages.end(); ++it)
    {
        std::cout << "  Error Code: " << it->first << ", Page: " << it->second << std::endl;
    }

    // Print locations vector
    std::cout << "Locations:" << std::endl;
    for (size_t i = 0; i < serverBlock.locations.size(); ++i)
    {
        std::cout << "  Location " << i + 1 << ":" << std::endl;
        // Assuming the Location struct has a `print` or similar method.
        // Replace with the actual implementation.
        // Example: serverBlock.locations[i].print();
        std::cout << "    (Add Location details here)" << std::endl;
    }

    std::cout << "---------------------------------" << std::endl;
}

void printRequestState(const RequestState& req)
{
    std::cout << "---- RequestState Information ----" << std::endl;

    // Print basic fields
    std::cout << "Client FD: " << req.client_fd << std::endl;
    std::cout << "CGI Input FD: " << req.cgi_in_fd << std::endl;
    std::cout << "CGI Output FD: " << req.cgi_out_fd << std::endl;
    std::cout << "CGI PID: " << req.cgi_pid << std::endl;
	std::cout << "CGI Done: " << (req.cgi_done ? "true" : "false") << std::endl;

    // Print State (enum as readable string)
    std::cout << "State: ";
    switch (req.state)
    {
    case RequestState::STATE_READING_REQUEST:
        std::cout << "STATE_READING_REQUEST";
        break;
    case RequestState::STATE_PREPARE_CGI:
        std::cout << "STATE_PREPARE_CGI";
        break;
    case RequestState::STATE_CGI_RUNNING:
        std::cout << "STATE_CGI_RUNNING";
        break;
    case RequestState::STATE_SENDING_RESPONSE:
        std::cout << "STATE_SENDING_RESPONSE";
        break;
    default:
        std::cout << "UNKNOWN";
        break;
    }
    std::cout << std::endl;

    // Print buffer sizes and first few characters (optional)
    std::cout << "Request Buffer Size: " << req.request_buffer.size() << std::endl;
    if (!req.request_buffer.empty())
    {
        std::cout << "Request Buffer Content (first 50 chars): "
			<< std::string(req.request_buffer.begin(),
			req.request_buffer.begin() + std::min(req.request_buffer.size(), size_t(50)))
			<< std::endl;
    }

    std::cout << "Response Buffer Size: " << req.response_buffer.size() << std::endl;
    if (!req.response_buffer.empty())
    {
        std::cout << "Response Buffer Content (first 50 chars): "
			<< std::string(req.response_buffer.begin(),
							req.response_buffer.begin() + std::min(req.response_buffer.size(), size_t(50)))
			<< std::endl;
    }

    std::cout << "CGI Output Buffer Size: " << req.cgi_output_buffer.size() << std::endl;
    if (!req.cgi_output_buffer.empty())
    {
        std::cout << "CGI Output Buffer Content (first 50 chars): "
			<< std::string(req.cgi_output_buffer.begin(),
							req.cgi_output_buffer.begin() + std::min(req.cgi_output_buffer.size(), size_t(50)))
			<< std::endl;
    }

	if (req.associated_conf)
    {
        std::cout << "Associated ServerBlock:" << std::endl;
        printServerBlock(*req.associated_conf);
    }
    else
    {
        std::cout << "Associated ServerBlock: NULL" << std::endl;
    }
    std::cout << "Requested Path: " << req.requested_path << std::endl;

    std::cout << "----------------------------------" << std::endl;
}

int main(int argc, char **argv, char **envp)
{
	ConfigHandler utils;
	GlobalFDS globalFDS;
	Server server(globalFDS);
	StaticHandler staticHandler(server);
	CgiHandler cgiHandler(server);

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
