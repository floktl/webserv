/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/16 12:38:49 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/16 13:54:41 by fkeitel          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include "../main.hpp"

struct GlobalFDS;
struct ServerBlock;

class Server {
	public:
		Server(GlobalFDS &_globalFDS);
		int server_init(std::vector<ServerBlock> configs);
	private:
		GlobalFDS& globalFDS;
		StaticHandler& staticHandler;
		CgiHandler& cgiHandler;
		void handleNewConnection(int epoll_FD, int fd, const ServerBlock& conf);
};

#endif
