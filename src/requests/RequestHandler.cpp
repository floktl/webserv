/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:17 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/16 13:37:26 by fkeitel          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "RequestHandler.hpp"

RequestHandler::RequestHandler(GlobalFDS &_globalFDS) : globalFDS(_globalFDS) {}

void RequestHandler::buildResponse(RequestState &req)
{
	const ServerBlock* conf = req.associated_conf;
	if (!conf) return;

	std::string content;

	content = "HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\nHello, World!";
	req.response_buffer.assign(content.begin(), content.end());

	std::stringstream ss;
	ss.str("");
	ss << "Response\n" << content;
	Logger::file(ss.str());
}

void RequestHandler::parseRequest(RequestState &req)
{
	std::string request(req.request_buffer.begin(), req.request_buffer.end());

	size_t pos = request.find("\r\n");
	if (pos == std::string::npos)
		return;
	std::string requestLine = request.substr(0, pos);

	std::string method, path, version;
	{
		size_t firstSpace = requestLine.find(' ');
		if (firstSpace == std::string::npos) return;
		method = requestLine.substr(0, firstSpace);

		size_t secondSpace = requestLine.find(' ', firstSpace + 1);
		if (secondSpace == std::string::npos) return;
		path = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);

		version = requestLine.substr(secondSpace + 1);
	}

	std::string query;
	size_t qpos = path.find('?');
	if (qpos != std::string::npos)
	{
		query = path.substr(qpos + 1);
		path = path.substr(0, qpos);
	}

	req.requested_path = "http://localhost:" + std::to_string(req.associated_conf->port) + path;
	req.cgi_output_buffer.clear();

	if (needsCGI(req.associated_conf, req.requested_path))
	{
		req.state = RequestState::STATE_PREPARE_CGI;
		startCGI(req, method, query);
	}
	else
	{
		buildResponse(req);
		Logger::file("here we dont !");
		req.state = RequestState::STATE_SENDING_RESPONSE;
		Logger::file("here we dont go!");
		modEpoll(globalFDS.epoll_FD, req.client_fd, EPOLLOUT);
		Logger::file("here we dont go further!");
	}
}
