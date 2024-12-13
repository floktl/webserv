/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.hpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:13 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/13 12:48:37 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef REQUESTHANDLER_HPP
#define REQUESTHANDLER_HPP

#include <string>
#include <map>
#include <set>
#include <memory>
#include <vector>
#include "../utils/ConfigHandler.hpp"
#include "../error/ErrorHandler.hpp"
#include "../cgi/CgiHandler.hpp"

struct ServerBlock;

class RequestHandler {
public:
	enum State {
		STATE_READING_REQUEST,
		STATE_CGI_RUNNING,
		STATE_SENDING_RESPONSE,
		STATE_DONE
	};

	RequestHandler(
		int _client_fd,
		const ServerBlock& _config,
		std::set<int>& _activeFds,
		std::map<int,const ServerBlock*>& _serverBlockConfigs
	);

	void handle_request();

private:
	int                                 client_fd;
	const ServerBlock&                  config;
	std::set<int>&                      activeFds;
	std::map<int,const ServerBlock*>&   serverBlockConfigs;
	ErrorHandler                        errorHandler;

	State                               state;
	std::string                         requestBuffer;
	std::string                         responseBuffer;
	bool                                requestComplete;
	bool                                cgiNeeded;
	bool                                cgiStarted;
	bool                                cgiFinished;
	bool                                responseSent;

	std::unique_ptr<CgiHandler>         cgiHandler;

	bool parseRequestLine(const std::string& request, std::string& method,
						  std::string& requestedPath, std::string& version);

	void parseHeaders(std::istringstream& requestStream, std::map<std::string, std::string>& headersMap);
	std::string extractRequestBody(std::istringstream& requestStream);

	const Location* findLocation(const std::string& requestedPath);
	bool handleDeleteMethod(const std::string& filePath);
	bool handleDirectoryIndex(std::string& filePath);
	void handleStaticFile(const std::string& filePath);

	void handleStateReadingRequest();
	void handleStateSendingResponse();
	void parseFullRequest();
	void closeConnection();
};

#endif