/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ClientHandler.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:13 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/02 14:33:02 by fkeitel          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CLIENTHANDLER_HPP
#define CLIENTHANDLER_HPP

#include <vector>          // For std::vector
#include "../utils/ConfigHandler.hpp"       // Include the definition of FileConfData and ConfLocations
#include "../server/Server.hpp" // Include the Server class
#include "../helpers/helper.hpp"
#include <fstream>   // For std::ifstream
#include <sstream>   // For std::stringstream
#include <sys/socket.h> // For recv and send
#include <cstring>   // For memset
#include <unistd.h>  // For close
#include <iostream>  // For error output
#include <poll.h> // For poll
#include <sys/epoll.h> // For epoll

struct FileConfData;

class ClientHandler
{
public:
	static void handle_client(int client_fd, const FileConfData& config,
		int kq, std::set<int>& activeFds,
		std::map<int, const FileConfData*>& clientConfigMap);
};

#endif