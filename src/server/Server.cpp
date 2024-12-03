/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:40:26 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/03 10:21:16 by jeberle          ###   ########.fr       */
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
		perror("socket");
		exit(EXIT_FAILURE);
	}

	// Set socket options to reuse address
	opt = 1; // Enable option
	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
	{
		perror("setsockopt");
		exit(EXIT_FAILURE);
	}

	// Initialize address structure to zero
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;           // Use IPv4
	addr.sin_addr.s_addr = INADDR_ANY;   // Accept connections on any IP address
	addr.sin_port = htons(port);         // Set port number (network byte order)

	// Bind the server socket to the address and port
	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
	{
		perror("bind");
		exit(EXIT_FAILURE);
	}

	// Set up the server socket to listen for incoming connections
	if (listen(server_fd, 128) < 0)
	{
		perror("listen");
		exit(EXIT_FAILURE);
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
		perror("accept");
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
		perror("epoll_ctl (EPOLL_CTL_ADD)");
		close(this->client_fd);
		return -1;
	}

	// Add client FD to the active file descriptors set
	this->activeFds.insert(this->client_fd);

	// Map the client FD to its configuration
	this->clientConfigMap[this->client_fd] = &servConfig;

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
			perror("epoll_ctl");
			exit(EXIT_FAILURE);
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
				ClientHandler::handle_client(fd,
					*(clientConfigMap[fd]),
					this->epoll_fd, activeFds, clientConfigMap);
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
		perror("epoll_create");
		return;
	}

	register_for_monitoring(*this->configs, events, activeFds, epoll_fd);

	while (true)
	{
		int num_events = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
		if (num_events < 0)
		{
			perror("epoll_wait");
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

////	function to create th eserver socket with the given port
//int Server::create_server_socket(int port)
//{
//	int					server_fd;
//	int					opt; 	// Option value used for socket options
//	struct sockaddr_in	addr;	// Struct to define server address and port

//	//	AF_INET: Specifies the IPv4 address family.
//	//	SOCK_STREAM: Specifies that this is a TCP socket.
//	//	0: The protocol is chosen automatically (default for TCP is IP).
//	server_fd = socket(AF_INET, SOCK_STREAM, 0); // Create a TCP socket
//	if (server_fd < 0)
//	{
//		perror("socket");
//		exit(EXIT_FAILURE);
//	}

//	//	SOL_SOCKET: Specifies that we are setting an option for socket itself.
//	//	SO_REUSEADDR: Allows server to reuse same address and port even -
//	//		if socket is in the TIME_WAIT state (e.g., when restarting server)

//	//	Analogy: Reserving a Table at a Restaurant
//	//	Imagine you're reserving a table at a restaurant:
//	//	Normally, when someone leaves, the table is cleaned and "locked" for -
//	//		a short time before new guests can use it.
//	//	By enabling SO_REUSEADDR, you're telling the manager: "I’m okay with -
//	//		taking this table immediately, even if it hasn’t been cleaned yet."
//	//	opt is used to set socket options, allowing the reuse of -
//	//	- the same address/port without waiting for a timeout.
//	opt = 1; // 1 means enable
//	if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
//	{
//		perror("setsockopt");
//		exit(EXIT_FAILURE);
//	}

//	memset(&addr, 0, sizeof(addr)); 	// Initialize address structure to zero
//	addr.sin_family = AF_INET;     		// Use IPv4
//	addr.sin_addr.s_addr = INADDR_ANY;	// Accept connections on any IP address
//	// Conv. port num to network byte order (host-to-network short)
//	addr.sin_port = htons(port);

//	//	bind associates the server socket (server_fd) with the address (addr) -
//	//	- and port specified in the sockaddr_in structure:
//	if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0)
//	{
//		perror("bind");
//		exit(EXIT_FAILURE);
//	}

//	//	128: The backlog (maximum number of pending connections allowed in the -
//	//	 	- queue before the server starts rejecting new connections).
//	if (listen(server_fd, 128) < 0)
//	{
//		perror("listen");
//	}
//	setNonBlocking(server_fd);	// Set the server socket to non-blocking mode

//	return server_fd;			// Return the server file descriptor
//}

////	helper function to handle the event on the server
//int Server::handleServerEvent(int serv_fd, const ServerBlock& servConfig)
//{
//    // Accept the client connection
//    this->client_fd = accept(serv_fd, nullptr, nullptr);
//    if (this->client_fd < 0)
//    {
//        perror("accept");
//        return -1; // Signal failure
//    }
//    setNonBlocking(this->client_fd);

//    // Add client FD to kqueue
//    EV_SET(&this->change, this->client_fd, EVFILT_READ,
//		EV_ADD | EV_ENABLE, 0, 0, nullptr);
//    if (kevent(this->kq, &this->change, 1, nullptr, 0, nullptr) < 0)
//    {
//        perror("kevent (EV_ADD)");
//        close(this->client_fd);
//        return -1;
//    }
//    this->activeFds.insert(this->client_fd);
//	// Map client FD to its config
//    this->clientConfigMap[this->client_fd] = &servConfig;
//    return 0;
//}

//// Registers server sockets for monitoring read events in kqueue.
//// Parameters:
//// - @param configs: A vector of server configuration data.
//// - @param changes: An array of kevent structures to register events.
//// - @param activeFds: A set to track active file descriptors.
//// - @param numFds: A pointer to the number of file descriptors currently registered.
//void register_for_monitoring(const std::vector<ServerBlock>& configs,
//    struct kevent* changes, std::set<int>& activeFds, int* numFds)
//{
//    // Iterate over each server configuration in the configs vector.

//    for (const auto& conf : configs)
//    {
//        //	Register the server socket for monitoring read events
//		//	EVFILT_READ: Monitor the server_fd for read events.
//		//	EV_ADD: Add the event to the kqueue.
//		//	EV_ENABLE: Enable the event for monitoring.
//        EV_SET(&changes[(*numFds)++], conf.server_fd, EVFILT_READ, EV_ADD | EV_ENABLE, 0, 0, nullptr);

//        // Add the server file descriptor to the active file descriptors set.
//        activeFds.insert(conf.server_fd);
//    }
//}

//// Identifies the server configuration associated with a given event.
//// Parameters:
//// - @param events: A kevent structure representing the triggered event.
//// - @param configs: A vector of server configuration data.
//// Returns:
//// - An iterator pointing to the corresponding server configuration or configs.end() if not found.
//std::vector<ServerBlock>::const_iterator define_config(struct kevent event,
//    const std::vector<ServerBlock>& configs)
//{
//    // Check if the event contains an error flag.
//    if (event.flags & EV_ERROR)
//    {
//        return configs.end(); // Return configs.end() to indicate an error.
//    }

//    // Find the configuration that matches the file descriptor in the event.
//    auto confIt = std::find_if(configs.begin(), configs.end(),
//        [&](const ServerBlock& conf) {
//            return conf.server_fd == (int)event.ident; // Compare file descriptors.
//        });

//    return confIt; // Return the iterator to the matching configuration.
//}



// Processes triggered events returned by kqueue.
// Parameters:
// - @param num_events: The number of events triggered.
//void Server::process_events(int num_events)
//{
//    // Loop through each triggered event.
//    for (int i = 0; i < num_events; ++i)
//    {
//        // Identify the server configuration associated with the event.
//        auto confIt = define_config(events[i], *configs);

//        // If the event is associated with a server socket, handle it.
//        if (confIt != configs->end() &&
//            handleServerEvent(events[i].ident, *confIt) == -1)
//            continue; // Skip further processing if handling fails.

//        // If event is not associated withserver socket, handle it as client request.
//        if (confIt == configs->end())
//            ClientHandler::handle_client(events[i].ident,
//                *(clientConfigMap[events[i].ident]),
//                this->kq, activeFds, clientConfigMap);
//    }
//}


//// Starts the server by initializing kqueue, registering sockets, and handling events in a loop.
//// Parameters:
//// - @param configs_files: A vector of server configuration data.
//void Server::start(const std::vector<ServerBlock>& configs_files)
//{
//    this->configs = &configs_files; // Store a pointer to the server configurations.

//    // Initialize the kqueue instance.
//    kq = kqueue();
//    if (kq < 0)
//    {
//        perror("kqueue"); // Print an error message if kqueue initialization fails.
//        return;
//    }

//    // Register all server sockets for monitoring read events.
//    register_for_monitoring(*this->configs, changes, activeFds, &num_fds);

//   	//	The program enters an event loop with kevent:
//	//		The program waits for one or more events to be triggered.
//	//		When an event occurs (e.g., a new client connects to server_fd), -
//	//			the details of the event are stored in the events array.
//	//	changelist: Pointer to kevent struct defining changes (add server socket).
//	//	nchanges: Number of entries in the changelist array.
//	//	eventlist: No triggered events are being retrieved in this call.
//	//	nevents: No space is alloc. for triggered events, as eventlist = nullptr.
//	//	timeout: No timeout is set, as the call is non-blocking for event regist.
//    while (true)
//    {
//        // Wait for events to be triggered and store them in the events array.
//        num_fds = kevent(kq, changes, num_fds, events, MAX_EVENTS, nullptr);

//        // If kevent fails, print an error message and break the loop.
//        if (num_fds < 0)
//        {
//            perror("kevent");
//            break;
//        }

//        // Process the triggered events.
//        process_events(num_fds);
//    }

//    // Cleanup all resources before exiting.
//    close_everything();
//}
