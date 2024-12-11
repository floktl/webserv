/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiHandler.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/11 14:36:37 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/11 16:48:13 by jeberle          ###   ########.fr       */
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
#include <string> // Für strerror

struct ServerBlock;
struct Location;

class CgiHandler {
private:
	const Location* location;
	const std::string& filePath;
	const std::string& method;
	const std::string& requestedPath;
	const std::string& requestBody;
	const std::map<std::string, std::string>& headersMap;

	void closePipe(int fds[2]);
	bool createPipes(int pipefd_out[2], int pipefd_in[2]);
	void setupChildEnv(const std::map<std::string, std::string>& cgiParams, std::vector<char*>& env);
	void runCgiChild(const std::string& cgiPath, const std::string& scriptPath, int pipefd_out[2], int pipefd_in[2], const std::map<std::string, std::string>& cgiParams);
	void writeRequestBodyIfNeeded(int pipe_in, const std::string& method, const std::string& requestBody);
	std::string readCgiOutput(int);
	void parseCgiOutput(std::string&, std::string&, int*, int*, pid_t);
	bool checkForRedirect(const std::string&);
	void sendCgiResponse(const std::string&, const std::string&);
	void waitForChild(pid_t);
	void executeCGI(const std::string& cgiPath, const std::string& scriptPath, const std::map<std::string, std::string>& cgiParams, const std::string& requestBody, const std::string& method);

public:
	CgiHandler(const Location* location, const std::string& filePath, const std::string& method, const std::string& requestedPath, const std::string& requestBody, const std::map<std::string, std::string>& headersMap);
	bool handleCGIIfNeeded();
};

#endif