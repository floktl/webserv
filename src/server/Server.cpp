/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:40:26 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/13 06:52:46 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "Server.hpp"
#include "../helpers/helper.hpp"

#include <poll.h> // For poll
#include <sys/socket.h> // For socket functions
#include <unistd.h> // For close
#include <cstring> // For memset
#include <cstdlib> // For exit

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

    // Add client FD to the active file descriptors set
    this->activeFds.insert(this->client_fd);

    // Map the client FD to its configuration
    this->serverBlockConfigs[this->client_fd] = &servConfig;

    return 0; // Success
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

void Server::process_events()
{
    // Iterate over active file descriptors
    for (int fd : activeFds)
    {
        if (fd == client_fd) // Check if it is a client connection
        {
            auto confIt = define_config(fd, *configs);
            if (confIt != configs->end())
            {
                handleServerEvent(fd, *confIt);
            }
            else
            {
                RequestHandler reqHan(fd, *(serverBlockConfigs[fd]), activeFds, serverBlockConfigs);
                reqHan.handle_request();
            }
        }
    }
}

void Server::start(const std::vector<ServerBlock>& configs_files)
{
    this->configs = &configs_files;

    for (const auto& conf : *this->configs)
    {
        this->activeFds.insert(conf.server_fd);
    }

    while (true)
    {
        process_events();
    }

    close_everything();
}

// Cleans up all resources by closing active file descriptors.
// This ensures proper resource deallocation at the end of the program.
void Server::close_everything(void)
{
    // Iterate over all active file descriptors.
    for (int fd : activeFds)
    {
        close(fd); // Close each file descriptor.
    }
}
