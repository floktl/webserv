/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:13 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/16 16:01:35 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include "../main.hpp"

struct RequestState;

class CgiHandler;
class Server;

class RequestHandler {
	public:
		RequestHandler(Server& server);

		void buildResponse(RequestState &req);
		void parseRequest(RequestState &req);
	private:
		Server& server;
};

#endif