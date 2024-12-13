/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiHandler.hpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/11 14:36:37 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/13 12:33:15 by jeberle          ###   ########.fr       */
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
    int                             client_fd;
    const Location*                 location;
    const std::string&              filePath;
    const std::string&              method;
    const std::string&              requestedPath;
    const std::string&              requestBody;
    const std::map<std::string, std::string>& headersMap;
    std::set<int>                   activeFds;
    const ServerBlock&              config;
    std::map<int, const ServerBlock> serverBlockConfigs;
    ErrorHandler                    errorHandler;

    enum RequestState {
        STATE_READING_REQUEST,
        STATE_PREPARE_CGI,
        STATE_CGI_RUNNING,
        STATE_SENDING_RESPONSE,
        STATE_DONE
    };

    RequestState    state;
    int             pipefd_out[2];
    int             pipefd_in[2];
    int             epoll_fd;
    pid_t           cgi_pid;
    bool            input_written;

    std::string     cgi_output;
    std::string     cgi_headers;
    std::string     cgi_body;

    void closePipe(int fds[2]);
    bool createPipes(int pipefd_out[2], int pipefd_in[2]);
    void setupChildEnv(const std::map<std::string, std::string>& cgiParams, std::vector<char*>& env);
    void runCgiChild(const std::string& cgiPath, const std::string& scriptPath, const std::map<std::string, std::string>& cgiParams);
    void writeRequestBodyIfNeeded();
    void executeCGI(const std::string& cgiPath, const std::string& scriptPath, const std::map<std::string, std::string>& cgiParams);
    void nonBlockingParseHeaders();
    void sendCgiResponse();
    void checkChildExit();
    void cleanupEpoll();

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
    void onEvent(int fd, uint32_t events);
    void closeConnection();
};

#endif