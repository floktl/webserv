/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:40:21 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/13 06:57:32 by jeberle          ###   ########.fr       */
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
#include <vector>
#include <sys/epoll.h>     // For epoll
#include <fcntl.h>         // For fcntl (to set non-blocking mode)
#include "../requests/RequestHandler.hpp"
#include "../utils/ConfigHandler.hpp"
#include <fcntl.h>         // For fcntl (to set non-blocking mode)

#define MAX_EVENTS 100

struct ServerBlock;

class Server
{
private:
	std::map<int,const ServerBlock*> serverBlockConfigs;
	const std::vector<ServerBlock>* configs = nullptr;
	struct epoll_event changes[MAX_EVENTS];
	struct epoll_event events[MAX_EVENTS];
	int epoll_fd = -1;
	int num_fds = 0;
	int client_fd = -1;
	std::set<int> activeFds;

public:
	int create_server_socket(int port);
	void start(const std::vector<ServerBlock>& configs);
	int handleServerEvent(int serv_fd,const ServerBlock& serverConfig);
	void process_events();
	void close_everything(void);
	std::vector<ServerBlock>::const_iterator define_config(int fd, const std::vector<ServerBlock>& configList);

};

#endif
