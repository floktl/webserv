/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:40:21 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/01 09:50:37 by fkeitel          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include <netinet/in.h>
#include <cstring>
#include <unistd.h>
#include <iostream>
#include <map>
#include <set>
#include "../client/ClientHandler.hpp"
#include "../utils/Utils.hpp"
#include <fcntl.h>         // For fcntl (to set non-blocking mode)
#include <sys/event.h>     // For kqueue (epoll not aviable for macOS)

# define MAX_EVENTS 100
struct FileConfData;

class Server
{
private:
    std::map<int, const FileConfData*> clientConfigMap;	// Map client FDs to configurations
    const std::vector<FileConfData>* configs = nullptr;	// Pointer to server configurations
    struct kevent	changes[MAX_EVENTS];	// Events to register with kqueue
    struct kevent	events[MAX_EVENTS];		// Array to store triggered events
    struct kevent	change;                 // Current kevent change structure
    int				kq = -1;				// Kqueue instance
    int				num_fds = 0;			// Number of triggered events
    int				client_fd = -1;			// Current client file descriptor
    std::set<int>	activeFds;				// Track active file descriptors

public:
    int		create_server_socket(int port);   // Create a server socket for the given port
    void	start(const std::vector<FileConfData>& configs); // Start the server
    int		handleServerEvent(int serv_fd, const FileConfData& serverConfig);
    void	process_events(int num_events); // Process triggered events
	void	close_everything(void);	//	close all fdss and kqueue
};

#endif
