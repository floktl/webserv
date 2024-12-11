/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiHandler.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/11 14:36:37 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/11 16:10:23 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include "CgiHandler.hpp"
#include "./../utils/Logger.hpp"

#include <sys/epoll.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/select.h>
#include <cstddef>
#include <vector> // Hinzugefügt für std::vector
#include <cstring> // Für strerror
#include <set>
#include <map> // Für strerror

struct ServerBlock;
struct Location;

class CgiHandler {
private:
	int client_fd;
	Location* location;
	std::string& filePath;
	std::string& method;
	std::string& requestedPath;
	std::string& requestBody;
	std::string& headers;
	std::set<int>& activeFds;
	std::map<int, const ServerBlock*>& serverBlockConfigs;

	void closePipe(int fds[2]);
	bool createPipes(int pipefd_out[2], int pipefd_in[2]);
	void setupChildEnv(const std::map<std::string, std::string>& cgiParams, std::vector<char*>& env);
	void runCgiChild(const std::string& cgiPath, const std::string& scriptPath, int pipefd_out[2], int pipefd_in[2], const std::map<std::string, std::string>& cgiParams);
	void writeRequestBodyIfNeeded(int pipe_in);
	std::string readCgiOutput(int pipe_out);
	void parseCgiOutput(std::string& headers, std::string& body, int pipefd_out[2], int pipefd_in[2], pid_t pid);
	bool checkForRedirect();
	void sendCgiResponse(const std::string& body);
	void waitForChild(pid_t pid);
	void executeCGI(const std::string& cgiPath, const std::map<std::string, std::string>& cgiParams);

    std::string getFileExtension();
public:
	CgiHandler(int client_fd, Location* _location, std::string& _filePath, std::string& _method, std::string& _requestedPath, std::string& _requestBody, std::string& _headers, std::set<int>& _activeFds, std::map<int, const ServerBlock*>& _serverBlockConfigs);
	bool handleCGI();
};

#endif