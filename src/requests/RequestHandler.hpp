/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:13 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/11 16:18:53 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include "../utils/ConfigHandler.hpp"
#include <string>
#include <set>
#include <map>

struct ServerBlock;
struct Location;

class RequestHandler
{
private:
	int client_fd; // File descriptor for the client connection
	const ServerBlock& config; // Reference to the server configuration
	std::set<int>& activeFds; // Reference to the set of active file descriptors
	std::map<int, const ServerBlock*>& serverBlockConfigs; // Reference to server block configurations

	// Helper functions
	bool parseRequestLine(const std::string& request, std::string& method, std::string& requestedPath, std::string& version);
	void parseHeaders(std::istringstream& requestStream, std::map<std::string, std::string>& headers);
	std::string extractRequestBody(std::istringstream& requestStream);
	const Location* findLocation( const std::string& requestedPath);
	bool handleDeleteMethod( const std::string& filePath);
	bool handleDirectoryIndex( std::string& filePath);
	void handleStaticFile( const std::string& filePath);
	void closeConnection();
	void sendErrorResponse(int statusCode, const std::string& message, const std::map<int, std::string>& errorPages);
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
	bool handleCGIIfNeeded(const Location* location, const std::string& filePath, const std::string& method, const std::string& requestedPath, const std::string& requestBody, const std::map<std::string, std::string>& headers);

public:
	RequestHandler(int client_fd, const ServerBlock& config, std::set<int>& activeFds, std::map<int, const ServerBlock*>& serverBlockConfigs);
	void handle_request();
};

#endif
