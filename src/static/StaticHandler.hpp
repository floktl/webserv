/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   StaticHandler.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:40:21 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/16 14:47:55 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef STATICHANDLER_HPP
#define STATICHANDLER_HPP

#include "../main.hpp"

class Server;
class CgiHandler;

class StaticHandler {
	public:
		StaticHandler(GlobalFDS &_globalFDS, Server& server);
		void handleClientRead(int epfd, int fd);
		void handleClientWrite(int epfd, int fd);
	private:
		GlobalFDS& globalFDS;
		CgiHandler* cgiHandler;
		Server& server;
};


#endif
