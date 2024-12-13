/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiHandler.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/11 14:36:37 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/13 07:46:02 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CGIHANDLER_HPP
#define CGIHANDLER_HPP

#include "../error/ErrorHandler.hpp"
#include "./../requests/RequestHandler.hpp"
#include "./../utils/Logger.hpp"
#include <sys/socket.h>
#include <sys/epoll.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/select.h>
#include <cstddef>
#include <vector>
#include <cstring>
#include <set>
#include <map>
#include <string>

struct ServerBlock;
struct Location;

class CgiHandler {
private:
	int client_fd;
	const Location* location;
	const std::string& filePath;
	const std::string& method;
	const std::string& requestedPath;
	const std::string& requestBody;
	const std::map<std::string, std::string>& headersMap;
	std::set<int> activeFds;
	const ServerBlock& config;
	std::map<int, const ServerBlock> serverBlockConfigs;
	ErrorHandler errorHandler;

	std::string getFileExtension();
	void closePipe(int fds[2]);
	bool createPipes(int pipefd_out[2], int pipefd_in[2]);
	void setupChildEnv(const std::map<std::string, std::string>& cgiParams, std::vector<char*>& env);
	void runCgiChild(const std::string& cgiPath, const std::string& scriptPath, int pipefd_out[2], int pipefd_in[2], const std::map<std::string, std::string>& cgiParams);
	void writeRequestBodyIfNeeded(int pipe_in);
	std::string readCgiOutput(int pipe_out);
	void parseCgiOutput(std::string& headers, std::string& body, int pipefd_out[2], [[maybe_unused]] int pipefd_in[2], pid_t pid);
	bool checkForRedirect(const std::string& headers);
	void sendCgiResponse(const std::string& headers, const std::string& body);
	void waitForChild(pid_t pid);
	void executeCGI(const std::string& cgiPath, const std::string& scriptPath, const std::map<std::string, std::string>& cgiParams);
public:
	CgiHandler(
		int client_fd,
		const Location* location,
		const std::string& filePath,
		const std::string& method,
		const std::string& requestedPath,
		const std::string& requestBody,
		const std::map<std::string, std::string>& headersMap,
		std::set<int> activeFds,
		const ServerBlock& config,
		const std::map<int, const ServerBlock*>& serverBlockConfigs
	);
		bool handleCGIIfNeeded();
		void closeConnection();
	};

#endif