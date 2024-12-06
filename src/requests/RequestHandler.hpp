/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:13 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/03 10:08:25 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENTHANDLER_HPP
#define CLIENTHANDLER_HPP

#include <vector>          // For std::vector
#include "../utils/ConfigHandler.hpp"       // Include the definition of ServerBlock and Location
#include "../server/Server.hpp" // Include the Server class
#include "../helpers/helper.hpp"
#include <fstream>   // For std::ifstream
#include <sstream>   // For std::stringstream
#include <sys/socket.h> // For recv and send
#include <unistd.h>  // For close
#include <iostream>  // For error output
#include <poll.h> // For poll
#include <sys/epoll.h> // For epoll

struct ServerBlock;

class RequestHandler
{
public:
	static void handle_request(int client_fd, const ServerBlock& config,
		int kq, std::set<int>& activeFds,
		std::map<int, const ServerBlock*>& serverBlockConfigs);
};

#endif