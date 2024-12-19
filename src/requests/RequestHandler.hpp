/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jonathaneberle <jonathaneberle@student.    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:13 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/19 16:56:18 by jonathanebe      ###   ########.fr       */
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
		void buildErrorResponse(int statusCode, const std::string& message, std::stringstream *response, RequestState &req);
	private:
		Server& server;
};

#endif
