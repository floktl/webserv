/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   RequestHandler.cpp                                 :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jonathaneberle <jonathaneberle@student.    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/29 12:41:17 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/19 16:56:10 by jonathanebe      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "RequestHandler.hpp"

RequestHandler::RequestHandler(Server& _server)
    : server(_server) {}

//#include <fstream>    // For file operations
//#include <sstream>    // For std::stringstream
//#include <iostream>   // For std::cout
//#include <string>     // For std::string


//		// Kombinierte Error Page Logik
//		if (conf.error_pages.empty()) {
//			// Füge alle gefundenen Error Pages hinzu
//			for (const auto& errorPage : errorPageDefaults) {
//				conf.error_pages[errorPage.first] = errorPage.second;
//			}

//			// Fallback für häufige Fehler-Codes, falls nicht gefunden
//			const int defaultErrorCodes[] = {400, 401, 403, 404, 500, 502, 503, 504};
//			for (int errorCode : defaultErrorCodes) {
//				if (conf.error_pages.find(errorCode) == conf.error_pages.end()) {
//					conf.error_pages[errorCode] = "/50x.html";
//				}
//			}
//		}

//		// error_pages - now handled as a map
//		for (size_t j = 0; j < registeredServerConfs.size(); ++j) {
//			ServerBlock& conf = registeredServerConfs[j];
//			std::map<int, std::string> errorPagesToValidate = conf.error_pages;

//			for (std::map<int, std::string>::const_iterator it = errorPagesToValidate.begin();
//				it != errorPagesToValidate.end(); ++it) {
//				int code = it->first;
//				std::string path = it->second;

//				// Validierung des Pfads und der Fehlercodes
//				std::string normalizedPath = path; // Kopie für Validierung
//				std::ostringstream errorPageDef;
//				errorPageDef << code << " " << normalizedPath;

//				std::string errorPageString = errorPageDef.str();
//				if (!Sanitizer::sanitize_errorPage(errorPageString, expandEnvironmentVariables("$PWD", env))) {
//					configFileValid = false;
//					return false;
//				}

//				// Pfad nach Validierung aktualisieren
//				conf.error_pages[code] = normalizedPath;
//			}
//		}

//void RequestHandler::sendErrorResponse(int statusCode,
//	const std::string& message, const std::map<int, std::string>& errorPages)
//{
//	auto it = errorPages.find(statusCode);
//	if (it != errorPages.end()) {
//		std::ifstream errorFile(it->second);
//		if (errorFile.is_open()) {
//			std::stringstream fileContent;
//			fileContent << errorFile.rdbuf();
//			std::string response = "HTTP/1.1 " + std::to_string(statusCode) + " " + message + "\r\n"
//								"Content-Type: text/html\r\n\r\n" + fileContent.str();
//			send(client_fd, response.c_str(), response.size(), 0);
//			return;
//		} else {
//			Logger::red("Error loading custom error page: " + it->second);
//		}
//	}

//	std::ostringstream response;
//	response << "HTTP/1.1 " << statusCode << " " << message << "\r\n"
//			<< "Content-Type: text/html\r\n\r\n"
//			<< "<html><body><h1>" << statusCode << " " << message << "</h1></body></html>";
//	send(client_fd, response.str().c_str(), response.str().size(), 0);
//}

void RequestHandler::buildErrorResponse(int statusCode, const std::string& message, std::stringstream *response, RequestState &req)
{
    // Access the ErrorHandler instance via the Server class
    ErrorHandler* errorHandler = server.getErrorHandler();
    if (errorHandler)
    {
        *response << errorHandler->generateErrorResponse(statusCode, message, req);
    }
    else
    {
        // Fallback if ErrorHandler is not available
        *response << "HTTP/1.1 " << statusCode << " " << message << "\r\n"
			<< "Content-Type: text/html\r\n\r\n"
			<< "<html><body><h1>" << statusCode << " " << message << "</h1></body></html>";
    }
}


#include <fstream>    // For file operations
#include <sstream>    // For std::stringstream
#include <iostream>   // For std::cout
#include <string>     // For std::string

void RequestHandler::buildResponse(RequestState &req)
{
    const ServerBlock* conf = req.associated_conf;

    if (!conf) return;

    // Ensure the root path ends with a '/'
    std::string root_path = conf->root;
    if (root_path.empty())
        root_path = conf->index;
    if (root_path.back() != '/')
        root_path += '/';

    // Extract the relative path from req.requested_path (e.g., "/team/florian")
    std::string relative_path = req.requested_path.substr(req.requested_path.find_first_of('/', 7));

    //if (relative_path == "/favicon.ico")
    //    relative_path = "/index.html";

    // Remove the leading slash from relative_path
    if (!relative_path.empty() && relative_path.front() == '/')
        relative_path.erase(0, 1);

	  // If relative_path is empty or "/", default to the index file
    if (relative_path.empty())
        relative_path = conf->index;

	// Final check: If file_path does not end with ".html", append it
    if (relative_path.size() < 5 || relative_path.substr(relative_path.size() - 5) != ".html")
        relative_path += ".html";

    // Combine root path with the relative path
    std::string file_path = root_path + relative_path;

    // Try to open the file at the constructed path
    std::string file_content;
    std::ifstream file(file_path.c_str());

	std::stringstream buffer;

	std::stringstream response;
    if (file.is_open())
    {
        // Read the file's content into file_content
        buffer << file.rdbuf();
        file_content = buffer.str();
        file.close();
		 // Construct HTTP response
		response << "HTTP/1.1 " << (file.is_open() ? "200 OK" : "404 Not Found") << "\r\n";
		response << "Content-Length: " << file_content.size() << "\r\n";
		response << "\r\n";
		response << file_content;
    }
    else
    {
		RequestHandler::buildErrorResponse(404, "error 404 file not found", &response, req);
    }


    std::string response_str = response.str();

    // Assign response to req.response_buffer
    req.response_buffer.assign(response_str.begin(), response_str.end());
}


#include <iostream>
#include <vector>
#include <string>
#include <iomanip> // For formatting
void printServerBlock(const ServerBlock& serverBlock)
{
    std::cout << "---- ServerBlock Information ----" << std::endl;

    // Print basic fields
    std::cout << "Port: " << serverBlock.port << std::endl;
    std::cout << "Server FD: " << serverBlock.server_fd << std::endl;
    std::cout << "Name: " << serverBlock.name << std::endl;
    std::cout << "Root: " << serverBlock.root << std::endl;
    std::cout << "Index: " << serverBlock.index << std::endl;
    std::cout << "Client Max Body Size: " << serverBlock.client_max_body_size << std::endl;

    // Print errorPages map
    std::cout << "Error Pages:" << std::endl;
    for (std::map<int, std::string>::const_iterator it = serverBlock.errorPages.begin();
         it != serverBlock.errorPages.end(); ++it)
    {
        std::cout << "  Error Code: " << it->first << ", Page: " << it->second << std::endl;
    }

    // Print locations vector
    std::cout << "Locations:" << std::endl;
    for (size_t i = 0; i < serverBlock.locations.size(); ++i)
    {
        std::cout << "  Location " << i + 1 << ":" << std::endl;
        // Assuming the Location struct has a `print` or similar method.
        // Replace with the actual implementation.
        // Example: serverBlock.locations[i].print();
        std::cout << "    (Add Location details here)" << std::endl;
    }

    std::cout << "---------------------------------" << std::endl;
}

void printRequestState(const RequestState& req)
{
    std::cout << "---- RequestState Information ----" << std::endl;

    // Print basic fields
    std::cout << "Client FD: " << req.client_fd << std::endl;
    std::cout << "CGI Input FD: " << req.cgi_in_fd << std::endl;
    std::cout << "CGI Output FD: " << req.cgi_out_fd << std::endl;
    std::cout << "CGI PID: " << req.cgi_pid << std::endl;
	std::cout << "CGI Done: " << (req.cgi_done ? "true" : "false") << std::endl;

    // Print State (enum as readable string)
    std::cout << "State: ";
    switch (req.state)
    {
    case RequestState::STATE_READING_REQUEST:
        std::cout << "STATE_READING_REQUEST";
        break;
    case RequestState::STATE_PREPARE_CGI:
        std::cout << "STATE_PREPARE_CGI";
        break;
    case RequestState::STATE_CGI_RUNNING:
        std::cout << "STATE_CGI_RUNNING";
        break;
    case RequestState::STATE_SENDING_RESPONSE:
        std::cout << "STATE_SENDING_RESPONSE";
        break;
    default:
        std::cout << "UNKNOWN";
        break;
    }
    std::cout << std::endl;

    // Print buffer sizes and first few characters (optional)
    std::cout << "Request Buffer Size: " << req.request_buffer.size() << std::endl;
    if (!req.request_buffer.empty())
    {
        std::cout << "Request Buffer Content (first 50 chars): "
                  << std::string(req.request_buffer.begin(),
                                 req.request_buffer.begin() + std::min(req.request_buffer.size(), size_t(50)))
                  << std::endl;
    }

    std::cout << "Response Buffer Size: " << req.response_buffer.size() << std::endl;
    if (!req.response_buffer.empty())
    {
        std::cout << "Response Buffer Content (first 50 chars): "
                  << std::string(req.response_buffer.begin(),
                                 req.response_buffer.begin() + std::min(req.response_buffer.size(), size_t(50)))
                  << std::endl;
    }

    std::cout << "CGI Output Buffer Size: " << req.cgi_output_buffer.size() << std::endl;
    if (!req.cgi_output_buffer.empty())
    {
        std::cout << "CGI Output Buffer Content (first 50 chars): "
                  << std::string(req.cgi_output_buffer.begin(),
                                 req.cgi_output_buffer.begin() + std::min(req.cgi_output_buffer.size(), size_t(50)))
                  << std::endl;
    }

	 if (req.associated_conf)
    {
        std::cout << "Associated ServerBlock:" << std::endl;
        printServerBlock(*req.associated_conf);
    }
    else
    {
        std::cout << "Associated ServerBlock: NULL" << std::endl;
    }
    std::cout << "Requested Path: " << req.requested_path << std::endl;

    std::cout << "----------------------------------" << std::endl;
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

	if (server.getCgiHandler()->needsCGI(req.associated_conf, req.requested_path))
	{
		req.state = RequestState::STATE_PREPARE_CGI;
		Logger::file("addCgiTunnel");
		server.getCgiHandler()->addCgiTunnel(req, method, query);
	}
	else // handle static pages
	{
		printRequestState(req);
		buildResponse(req);
		req.state = RequestState::STATE_SENDING_RESPONSE;
		server.modEpoll(server.getGlobalFds().epoll_fd, req.client_fd, EPOLLOUT);
	}
}
