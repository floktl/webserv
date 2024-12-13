/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiHandler.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/11 14:42:00 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/13 07:48:58 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "CgiHandler.hpp"
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <iostream>


CgiHandler::CgiHandler(
    int _client_fd,
    const Location* _location,
    const std::string& _filePath,
    const std::string& _method,
    const std::string& _requestedPath,
    const std::string& _requestBody,
    const std::map<std::string, std::string>& _headersMap,
    std::set<int> _activeFds,
    const ServerBlock& _config,
    const std::map<int, const ServerBlock*>& _serverBlockConfigs
) : client_fd(_client_fd),
    location(_location),
    filePath(_filePath),
    method(_method),
    requestedPath(_requestedPath),
    requestBody(_requestBody),
    headersMap(_headersMap),
    activeFds(std::move(_activeFds)),
    config(_config),
    errorHandler(_client_fd, _config.errorPages)
{
    for (const auto& pair : _serverBlockConfigs) {
        if (pair.second) {
            serverBlockConfigs.emplace(pair.first, *(pair.second));
        }
    }
}

bool CgiHandler::createPipes(int pipefd_out[2], int pipefd_in[2]) {
    if (pipe(pipefd_out) == -1) {
        Logger::red("Pipe creation failed (out): " + std::string(strerror(errno)));
        return false;
    }
    if (pipe(pipefd_in) == -1) {
        Logger::red("Pipe creation failed (in): " + std::string(strerror(errno)));
        close(pipefd_out[0]);
        close(pipefd_out[1]);
        return false;
    }
    return true;
}

void CgiHandler::runCgiChild(const std::string& cgiPath, const std::string& scriptPath,
                             int pipefd_out[2], int pipefd_in[2],
                             const std::map<std::string, std::string>& cgiParams) {
    dup2(pipefd_out[1], STDOUT_FILENO);
    close(pipefd_out[0]);
    close(pipefd_out[1]);

    dup2(pipefd_in[0], STDIN_FILENO);
    close(pipefd_in[0]);
    close(pipefd_in[1]);

    std::vector<char*> env;
    setupChildEnv(cgiParams, env);

    char* args[] = {
        const_cast<char*>(cgiPath.c_str()),
        const_cast<char*>(scriptPath.c_str()),
        nullptr
    };
    execve(cgiPath.c_str(), args, env.data());
    Logger::red("execve failed: " + std::string(strerror(errno)));
    exit(EXIT_FAILURE);
}

void CgiHandler::setupChildEnv(const std::map<std::string, std::string>& cgiParams, std::vector<char*>& env) {
    for (const auto& param : cgiParams) {
        std::string envVar = param.first + "=" + param.second;
        env.push_back(strdup(envVar.c_str()));
    }
    env.push_back(nullptr);
}

void CgiHandler::parseCgiOutput(std::string& headers, std::string& body, int pipefd_out[2], [[maybe_unused]] int pipefd_in[2], pid_t pid) {
    char buffer[4096];
    int bytes;
    struct timeval timeout;
    timeout.tv_sec = 5;
    timeout.tv_usec = 0;

    fd_set readfds;
    FD_ZERO(&readfds);
    FD_SET(pipefd_out[0], &readfds);

    std::string output;
    while (true) {
        fd_set tempfds = readfds;
        int activity = select(pipefd_out[0] + 1, &tempfds, NULL, NULL, &timeout);
        if (activity == -1) {
            Logger::red("Error in select while reading CGI output");
            break;
        } else if (activity == 0) {
            Logger::red("CGI script timed out");
            kill(pid, SIGKILL);
            close(pipefd_out[0]);
            return;
        }

        if (FD_ISSET(pipefd_out[0], &tempfds)) {
            bytes = read(pipefd_out[0], buffer, sizeof(buffer));
            if (bytes > 0) {
                output.append(buffer, static_cast<size_t>(bytes));
            } else {
                break;
            }
        }
    }

    size_t header_end = output.find("\r\n\r\n");
    if (header_end != std::string::npos) {
        headers = output.substr(0, header_end);
        body = output.substr(header_end + 4);
    } else {
        body = output;
        headers = "Content-Type: text/html; charset=UTF-8";
    }
}

void CgiHandler::sendCgiResponse(const std::string& headers, const std::string& body) {
    std::string response = "HTTP/1.1 200 OK\r\n" + headers + "\r\n\r\n" + body;
    int sent = send(client_fd, response.c_str(), response.length(), 0);
    if (sent < 0) {
        Logger::red("Failed to send CGI response: " + std::string(strerror(errno)));
    }
}

void CgiHandler::waitForChild(pid_t pid) {
    int status;
    waitpid(pid, &status, 0);
}

void CgiHandler::executeCGI(const std::string& cgiPath, const std::string& scriptPath,
                            const std::map<std::string, std::string>& cgiParams) {
    int pipefd_out[2]; // For CGI stdout
    int pipefd_in[2];  // For CGI stdin

    if (!createPipes(pipefd_out, pipefd_in)) {
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        Logger::red("Fork failed: " + std::string(strerror(errno)));
        closePipe(pipefd_out);
        closePipe(pipefd_in);
        return;
    }

    if (pid == 0) {
        // Child process
        runCgiChild(cgiPath, scriptPath, pipefd_out, pipefd_in, cgiParams);
    } else {
        // Parent process
        close(pipefd_out[1]); // Close unused write end
        close(pipefd_in[0]);  // Close unused read end

        fcntl(pipefd_out[0], F_SETFL, O_NONBLOCK);
        fcntl(pipefd_in[1], F_SETFL, O_NONBLOCK);

        int epoll_fd = epoll_create1(0);
        if (epoll_fd == -1) {
            Logger::red("Epoll creation failed: " + std::string(strerror(errno)));
            closePipe(pipefd_out);
            closePipe(pipefd_in);
            return;
        }

        struct epoll_event event = {};
        struct epoll_event events[2];

        event.events = EPOLLIN;
        event.data.fd = pipefd_out[0];
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pipefd_out[0], &event) == -1) {
            Logger::red("Epoll add failed: " + std::string(strerror(errno)));
            close(epoll_fd);
            closePipe(pipefd_out);
            closePipe(pipefd_in);
            return;
        }

        event.events = EPOLLOUT;
        event.data.fd = pipefd_in[1];
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pipefd_in[1], &event) == -1) {
            Logger::red("Epoll add failed: " + std::string(strerror(errno)));
            close(epoll_fd);
            closePipe(pipefd_out);
            closePipe(pipefd_in);
            return;
        }

        bool input_written = false;
        std::string cgi_output;
        char buffer[4096];

        while (true) {
            int num_events = epoll_wait(epoll_fd, events, 2, 5000); // Timeout in milliseconds
            if (num_events == -1) {
                Logger::red("Epoll wait failed: " + std::string(strerror(errno)));
                break;
            }

            for (int i = 0; i < num_events; ++i) {
                if (events[i].data.fd == pipefd_out[0] && (events[i].events & EPOLLIN)) {
                    int bytes = read(pipefd_out[0], buffer, sizeof(buffer));
                    if (bytes > 0) {
                        cgi_output.append(buffer, bytes);
                    } else {
                        goto finish;
                    }
                }

                if (events[i].data.fd == pipefd_in[1] && (events[i].events & EPOLLOUT)) {
                    if (!input_written && method == "POST") {
                        writeRequestBodyIfNeeded(pipefd_in[1]);
                        input_written = true;
                    }
                }
            }
        }

    finish:
        close(pipefd_out[0]);
        close(pipefd_in[1]);
        close(epoll_fd);

        std::string headers, body;
        parseCgiOutput(headers, body, pipefd_out, pipefd_in, pid);

        sendCgiResponse(headers, body);
        waitForChild(pid);
    }
}

void CgiHandler::closePipe(int fds[2]) {
    close(fds[0]);
    close(fds[1]);
}

void CgiHandler::writeRequestBodyIfNeeded(int pipe_in) {
    if (method == "POST" && !requestBody.empty()) {
        ssize_t written = write(pipe_in, requestBody.c_str(), requestBody.size());
        if (written < 0) {
            Logger::red("Error writing to CGI stdin: " + std::string(strerror(errno)));
        }
    }
    close(pipe_in);
}

bool CgiHandler::handleCGIIfNeeded() {
    // Prüfen, ob der Request als CGI behandelt werden muss
    if (location->cgi.empty() || filePath.find(location->cgi) == std::string::npos) {
        return false; // Kein CGI-Request
    }

    try {
        std::string cgiPath = location->cgi;
        if (cgiPath.empty()) {
            Logger::red("No CGI path defined for this location.");
            return false;
        }

        executeCGI(cgiPath, filePath, {{"SCRIPT_FILENAME", filePath}, {"REQUEST_METHOD", method}});
        return true; // CGI-Request erfolgreich behandelt
    } catch (const std::exception& e) {
        Logger::red(std::string("CGI Error: ") + e.what());
		errorHandler.sendErrorResponse(500, "Internal Server Error");
    }
    return false;
}