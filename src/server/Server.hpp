/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Server.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/16 12:38:49 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/17 11:32:50 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef SERVER_HPP
#define SERVER_HPP

#include "../main.hpp"

struct GlobalFDS;
struct ServerBlock;

class StaticHandler;
class CgiHandler;
class RequestHandler;
class ErrorHandler;

class Server
{
	public:
		Server(GlobalFDS &_globalFDS);
		~Server();
		int server_init(std::vector<ServerBlock> configs);
		void delFromEpoll(int epfd, int fd);
		void modEpoll(int epfd, int fd, uint32_t events);
		GlobalFDS& getGlobalFds(void);
		StaticHandler* getStaticHandler(void);
		CgiHandler* getCgiHandler(void);
		RequestHandler* getRequestHandler(void);
		ErrorHandler* getErrorHandler(void);
		int setNonBlocking(int fd);

	private:
		GlobalFDS& globalFDS;
		StaticHandler* staticHandler;
		CgiHandler* cgiHandler;
		RequestHandler* requestHandler;
		ErrorHandler* errorHandler;
		void handleNewConnection(int epoll_fd, int fd, const ServerBlock& conf);
};

#endif
