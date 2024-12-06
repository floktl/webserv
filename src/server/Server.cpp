/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:40:26 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/06 10:55:13 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "../helpers/helper.hpp"

#include <poll.h> // For poll
#include <sys/epoll.h> // For epoll

int Server::create_server_socket(int port)
{
	int server_fd;
	int opt; // Option value used for socket options
	struct sockaddr_in addr; // Struct to define server address and port

	// Create a TCP socket
	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0)
	{
		std::perror("socket could not be created");
		std::exit(EXIT_FAILURE);
	}

	// Set socket options to reuse address
	opt = 1; // Enable option
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		std::perror("setsockopt");
		std::exit(EXIT_FAILURE);
	}

	// Initialize address structure to zero
	std::memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;           // Use IPv4
	addr.sin_addr.s_addr = INADDR_ANY;   // Accept connections on any IP address
	addr.sin_port = htons(port);         // Set port number (network byte order)

	// Bind the server socket to the address and port
	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		std::perror("bind");
		std::exit(EXIT_FAILURE);
	}

	// Set up the server socket to listen for incoming connections
	if (listen(server_fd, 128) < 0)
	{
		std::perror("listen");
		std::exit(EXIT_FAILURE);
	}

	// Set the server socket to non-blocking mode
	setNonBlocking(server_fd);

	return server_fd; // Return the server file descriptor
}

int Server::handleServerEvent(int serv_fd, const ServerBlock& servConfig)
{
	// Accept the client connection
	this->client_fd = accept(serv_fd, nullptr, nullptr);
	if (this->client_fd < 0)
	{
		std::perror("accept");
		return -1; // Signal failure
	}

	// Set the client socket to non-blocking mode
	setNonBlocking(this->client_fd);

	// Add the client file descriptor to the epoll instance
	struct epoll_event ev = {};
	ev.events = EPOLLIN | EPOLLET; // Monitor for read events, use edge-triggered mode
	ev.data.fd = this->client_fd;

	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, this->client_fd, &ev) < 0)
	{
		std::perror("epoll_ctl (EPOLL_CTL_ADD)");
		close(this->client_fd);
		return -1;
	}

	// Add client FD to the active file descriptors set
	this->activeFds.insert(this->client_fd);

	// Map the client FD to its configuration
	this->serverBlockConfigs[this->client_fd] = &servConfig;

	return 0; // Success
}

void register_for_monitoring(const std::vector<ServerBlock>& configs,
	struct epoll_event* changes, std::set<int>& activeFds, int epoll_fd)
{
	(void)changes;
	for (const auto& conf : configs)
	{
		struct epoll_event ev = {};
		ev.events = EPOLLIN; // Monitor for read events
		ev.data.fd = conf.server_fd;

		// Use the valid epoll_fd directly
		if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, conf.server_fd, &ev) < 0)
		{
			std::perror("epoll_ctl");
			std::exit(EXIT_FAILURE);
		}
		activeFds.insert(conf.server_fd);
	}
}



std::vector<ServerBlock>::const_iterator Server::define_config(int fd, const std::vector<ServerBlock>& configList)
{
	// Iterate through the configs to find the one matching the given file descriptor
	auto confIt = std::find_if(configList.begin(), configList.end(),
		[fd](const ServerBlock& conf) {
			return conf.server_fd == fd;
		});

	return confIt; // Return the iterator to the matching configuration or configList.end() if not found
}

void Server::process_events(int num_events)
{
	for (int i = 0; i < num_events; ++i)
	{
		int fd = events[i].data.fd;
		if (events[i].events & EPOLLIN) // Check if there's data to read
		{
			auto confIt = define_config(fd, *configs);
			if (confIt != configs->end())
			{
				handleServerEvent(fd, *confIt);
			}
			else
			{
				RequestHandler::handle_request(fd, *(serverBlockConfigs[fd]),
					activeFds, serverBlockConfigs);
			}
		}
	}
}

void Server::start(const std::vector<ServerBlock>& configs_files)
{
	this->configs = &configs_files;

	epoll_fd = epoll_create(1);
	if (epoll_fd < 0)
	{
		std::perror("epoll_create");
		return;
	}

	register_for_monitoring(*this->configs, events, activeFds, epoll_fd);

	while (true)
	{
		int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		if (num_events < 0)
		{
			std::perror("epoll_wait");
			break;
		}
		process_events(num_events);
	}

	close(epoll_fd);
	close_everything();
}

// Cleans up all resources by closing active file descriptors and the kqueue instance.
// This ensures proper resource deallocation at the end of the program.
void Server::close_everything(void)
{
	// Iterate over all active file descriptors.
	for (int fd : activeFds)
	{
		close(fd); // Close each file descriptor.
	}
	close(epoll_fd); // Close the kqueue instance.
}