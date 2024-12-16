/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiHandler.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/11 14:36:37 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/16 09:52:07 by fkeitel          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include "../main.hpp"

struct ServerBlock;
struct RequestState;
struct Location;

class CgiHandler {
	public:
	private:
};

void cleanupCGI(RequestState &req);
void startCGI(RequestState &req, const std::string &method, const std::string &query);
const Location* findMatchingLocation(const ServerBlock* conf, const std::string& path);
bool needsCGI(const ServerBlock* conf, const std::string &path);
void handleCGIWrite(int epfd, int fd);
void handleCGIRead(int epfd, int fd);

#endif