/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:13 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/13 09:51:38 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include "../error/ErrorHandler.hpp"
#include "../utils/ConfigHandler.hpp"
#include "../cgi/CgiHandler.hpp"
#include <string>
#include <set>
#include <map>

struct ServerBlock;
struct Location;
class CgiHandler;

class RequestHandler
{
private:
	int client_fd; // File descriptor for the client connection
	const ServerBlock& config; // Reference to the server configuration
	std::set<int>& activeFds; // Reference to the set of active file descriptors
	std::map<int,const ServerBlock*>& serverBlockConfigs; // Reference to server block configurations
	ErrorHandler errorHandler;

	// Helper functions
	bool parseRequestLine(const std::string& request, std::string& method, std::string& requestedPath, std::string& version);
	void parseHeaders(std::istringstream& requestStream, std::map<std::string, std::string>& headersMap);
	std::string extractRequestBody(std::istringstream& requestStream);
	const Location* findLocation( const std::string& requestedPath);
	bool handleDeleteMethod( const std::string& filePath);
	bool handleDirectoryIndex( std::string& filePath);
	void handleStaticFile( const std::string& filePath);
	void closeConnection();

public:
	RequestHandler(int client_fd,const ServerBlock& config, std::set<int>& activeFds, std::map<int,const ServerBlock*>& serverBlockConfigs);
	void handle_request();
};

#endif