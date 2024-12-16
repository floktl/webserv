/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiHandler.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/11 14:36:37 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/16 14:38:48 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include "../main.hpp"

struct ServerBlock;
struct RequestState;
struct Location;

class Server;

class CgiHandler
{
	public:
		CgiHandler(GlobalFDS &_globalFDS, Server& server);

		void cleanupCGI(RequestState &req);
		void startCGI(RequestState &req, const std::string &method, const std::string &query);
		const Location* findMatchingLocation(const ServerBlock* conf, const std::string& path);
		bool needsCGI(const ServerBlock* conf, const std::string &path);
		void handleCGIWrite(int epfd, int fd);
		void handleCGIRead(int epfd, int fd);
	private:
		GlobalFDS& globalFDS;
		Server& server;
};


#endif