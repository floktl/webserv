/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   CgiHandler.cpp                                     :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/11 14:42:00 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/13 12:33:55 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "CgiHandler.hpp"
#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <iostream>
#include <sys/wait.h>

enum RequestState {
    STATE_READING_REQUEST,
    STATE_PREPARE_CGI,
    STATE_CGI_RUNNING,
    STATE_SENDING_RESPONSE,
    STATE_DONE
};

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
    errorHandler(_client_fd, _config.errorPages),
    state(STATE_READING_REQUEST),
    epoll_fd(-1),
    cgi_pid(-1),
    input_written(false)
{
    for (const auto& pair : _serverBlockConfigs) {
        if (pair.second) {
            serverBlockConfigs.emplace(pair.first, *(pair.second));
        }
    }
}

bool CgiHandler::createPipes(int _pipefd_out[2], int _pipefd_in[2]) {
    if (pipe(_pipefd_out) == -1) {
        Logger::red("Pipe creation failed (out): " + std::string(strerror(errno)));
        return false;
    }
    if (pipe(_pipefd_in) == -1) {
        Logger::red("Pipe creation failed (in): " + std::string(strerror(errno)));
        close(_pipefd_out[0]);
        close(_pipefd_out[1]);
        return false;
    }
    return true;
}

void CgiHandler::runCgiChild(const std::string& cgiPath, const std::string& scriptPath,
                             const std::map<std::string, std::string>& cgiParams) {
    // Child process
    if (dup2(pipefd_out[1], STDOUT_FILENO) == -1) {
        std::cerr << "Failed to redirect stdout: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
    close(pipefd_out[0]);
    close(pipefd_out[1]);

    if (dup2(pipefd_in[0], STDIN_FILENO) == -1) {
        std::cerr << "Failed to redirect stdin: " << strerror(errno) << std::endl;
        exit(EXIT_FAILURE);
    }
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
    std::cerr << "execve failed: " << strerror(errno) << std::endl;
    exit(EXIT_FAILURE);
}

void CgiHandler::setupChildEnv(const std::map<std::string, std::string>& cgiParams, std::vector<char*>& env) {
    for (const auto& param : cgiParams) {
        std::string envVar = param.first + "=" + param.second;
        env.push_back(strdup(envVar.c_str()));
    }
    env.push_back(nullptr);
}

/*
    Statt parseCgiOutput() blockierend aufzurufen, werden wir die CGI-Ausgabe
    nun eventbasiert einlesen. Sobald EPOLLIN auftritt für pipefd_out[0],
    lesen wir Daten in cgi_output ein. Sobald wir ein komplettes Header-Ende
    "\r\n\r\n" gefunden haben, parsen wir die Header und Body heraus.
    Dieser Parsing-Schritt wird also in onEvent() passieren, wenn wir genug Daten haben.
*/

void CgiHandler::nonBlockingParseHeaders() {
    // Wenn wir die Header noch nicht geparst haben:
    if (cgi_headers.empty()) {
        size_t header_end = cgi_output.find("\r\n\r\n");
        if (header_end != std::string::npos) {
            cgi_headers = cgi_output.substr(0, header_end);
            cgi_body = cgi_output.substr(header_end + 4);
        }
    } else {
        // Headers schon geparst, der Rest ist body
        // cgi_body wird fortlaufend durch cgi_output ergänzt.
        // Im Grunde ist cgi_output = cgi_headers + "\r\n\r\n" + cgi_body
        // Wir hängen neue Daten direkt an cgi_output an und aktualisieren cgi_body
        size_t header_end = cgi_headers.size() + 4;
        cgi_body = cgi_output.substr(header_end);
    }
}

/*
    Wenn wir fertig sind (EOF von CGI oder Prozess ist beendet), senden wir die Antwort.
*/

void CgiHandler::sendCgiResponse() {
    if (cgi_headers.empty()) {
        // Keine Headers gefunden, wir nutzen ein default Content-Type
        cgi_headers = "Content-Type: text/html; charset=UTF-8";
    }
    std::string response = "HTTP/1.1 200 OK\r\n" + cgi_headers + "\r\n\r\n" + cgi_body;
    ssize_t sent = send(client_fd, response.c_str(), response.length(), 0);
    if (sent < 0) {
        Logger::red("Failed to send CGI response: " + std::string(strerror(errno)));
    }
    state = STATE_DONE;
}

/*
    Non-blocking wait for child:
    Wenn wir wissen, dass das CGI den Output geschlossen hat, versuchen wir non-blocking mit WNOHANG zu warten.
*/

void CgiHandler::checkChildExit() {
    if (cgi_pid > 0) {
        int status;
        pid_t res = waitpid(cgi_pid, &status, WNOHANG);
        if (res == cgi_pid) {
            // Kindprozess beendet
            cgi_pid = -1;
        }
    }
}

/*
    executeCGI() wird jetzt nur noch die Pipes aufsetzen, fork() ausführen,
    den epoll-FD erstellen, die FDs registrieren, den Status auf STATE_CGI_RUNNING setzen
    und dann zurückkehren. Keine blockierenden Loops mehr!
*/

void CgiHandler::executeCGI(const std::string& cgiPath, const std::string& scriptPath,
                            const std::map<std::string, std::string>& cgiParams) {
    if (!createPipes(pipefd_out, pipefd_in)) {
        errorHandler.sendErrorResponse(500, "Internal Server Error");
        return;
    }

    pid_t pid = fork();
    if (pid < 0) {
        Logger::red("Fork failed: " + std::string(strerror(errno)));
        closePipe(pipefd_out);
        closePipe(pipefd_in);
        errorHandler.sendErrorResponse(500, "Internal Server Error");
        return;
    }

    if (pid == 0) {
        // Child
        runCgiChild(cgiPath, scriptPath, cgiParams);
    } else {
        // Parent
        cgi_pid = pid;
        close(pipefd_out[1]);
        close(pipefd_in[0]);

        fcntl(pipefd_out[0], F_SETFL, O_NONBLOCK);
        fcntl(pipefd_in[1], F_SETFL, O_NONBLOCK);

        epoll_fd = epoll_create1(0);
        if (epoll_fd == -1) {
            Logger::red("Epoll creation failed: " + std::string(strerror(errno)));
            closePipe(pipefd_out);
            closePipe(pipefd_in);
            errorHandler.sendErrorResponse(500, "Internal Server Error");
            return;
        }

        struct epoll_event event = {};

        // pipefd_out[0] als EPOLLIN registrieren um CGI Output zu lesen
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLHUP;
        event.data.fd = pipefd_out[0];
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pipefd_out[0], &event) == -1) {
            Logger::red("Epoll add failed (out): " + std::string(strerror(errno)));
            cleanupEpoll();
            errorHandler.sendErrorResponse(500, "Internal Server Error");
            return;
        }

        // pipefd_in[1] als EPOLLOUT registrieren, um CGI Input zu schreiben (Request Body)
        event.events = EPOLLOUT | EPOLLHUP;
        event.data.fd = pipefd_in[1];
        if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, pipefd_in[1], &event) == -1) {
            Logger::red("Epoll add failed (in): " + std::string(strerror(errno)));
            cleanupEpoll();
            errorHandler.sendErrorResponse(500, "Internal Server Error");
            return;
        }

        state = STATE_CGI_RUNNING;
        // Jetzt kehren wir sofort zum Hauptloop zurück.
        // Die weitere Verarbeitung erfolgt in onEvent() bei neuen EPOLL-Events.
    }
}

void CgiHandler::closePipe(int fds[2]) {
    close(fds[0]);
    close(fds[1]);
}

void CgiHandler::writeRequestBodyIfNeeded() {
    if (method == "POST" && !requestBody.empty()) {
        ssize_t written = write(pipefd_in[1], requestBody.c_str(), requestBody.size());
        if (written < 0) {
            Logger::red("Error writing to CGI stdin: " + std::string(strerror(errno)));
        }
    }
    // Input ist geschrieben oder leer, wir schließen den Schreib-Endpoint:
    close(pipefd_in[1]);
}

/*
    Wird vom Hauptloop aufgerufen, wenn ein EPOLL-Event für diesen Request anliegt.
    fd und events geben an, auf welchem FD ein Ereignis eingetreten ist.

    Hier wird die State Machine umgesetzt:
    - STATE_CGI_RUNNING: Lesen vom CGI (EPOLLIN auf pipefd_out[0]) oder Schreiben von Request-Body (EPOLLOUT auf pipefd_in[1]).
    - Sobald EPOLLIN am pipefd_out[0] EOF liefert, haben wir alle Daten vom CGI.
      Dann parsen wir die Header und Body und gehen zu STATE_SENDING_RESPONSE.
    - In STATE_SENDING_RESPONSE: Senden wir die Antwort zurück an den Client.
      Ist das geschehen, STATE_DONE.
*/

void CgiHandler::onEvent(int fd, uint32_t events) {
    if (state == STATE_CGI_RUNNING) {
        if (fd == pipefd_in[1]) {
            // Wir können jetzt schreiben
            if (events & EPOLLOUT) {
                // Request Body an CGI schreiben
                if (!input_written) {
                    writeRequestBodyIfNeeded();
                    input_written = true;
                    // Nach dem Schreiben wollen wir hier eigentlich nichts mehr schreiben.
                    // Also können wir diesen FD aus epoll entfernen.
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, pipefd_in[1], NULL);
                }
            }
            if (events & (EPOLLHUP | EPOLLERR)) {
                // CGI kann nicht geschrieben werden, Fehler
                Logger::red("Error writing to CGI");
                cleanupEpoll();
                errorHandler.sendErrorResponse(500, "Internal Server Error");
                state = STATE_DONE;
            }
        }

        if (fd == pipefd_out[0]) {
            if (events & EPOLLIN) {
                char buffer[4096];
                ssize_t bytes = read(pipefd_out[0], buffer, sizeof(buffer));
                if (bytes > 0) {
                    cgi_output.append(buffer, (size_t)bytes);
                    nonBlockingParseHeaders();
                } else if (bytes == 0) {
                    // EOF vom CGI
                    // Jetzt haben wir den kompletten Output
                    nonBlockingParseHeaders();
                    // Wir sind fertig mit dem CGI Output
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, pipefd_out[0], NULL);
                    close(pipefd_out[0]);
                    checkChildExit(); // Non-blocking waitpid
                    state = STATE_SENDING_RESPONSE;
                    sendCgiResponse();
                    cleanupEpoll();
                } else {
                    if (errno != EAGAIN && errno != EWOULDBLOCK) {
                        Logger::red("Error reading from CGI output: " + std::string(strerror(errno)));
                        cleanupEpoll();
                        errorHandler.sendErrorResponse(500, "Internal Server Error");
                        state = STATE_DONE;
                    }
                }
            }
            if (events & (EPOLLHUP | EPOLLERR | EPOLLRDHUP)) {
                // CGI hat geschlossen
                nonBlockingParseHeaders();
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, pipefd_out[0], NULL);
                close(pipefd_out[0]);
                checkChildExit(); // Non-blocking wait
                state = STATE_SENDING_RESPONSE;
                sendCgiResponse();
                cleanupEpoll();
            }
        }
    }
    // In STATE_SENDING_RESPONSE senden wir direkt in sendCgiResponse(), kein extra Event nötig.
    // STATE_DONE: Nichts mehr tun.
}

void CgiHandler::cleanupEpoll() {
    if (epoll_fd != -1) {
        close(epoll_fd);
        epoll_fd = -1;
    }
}

/*
    handleCGIIfNeeded() wird aufgerufen, wenn festgestellt wurde,
    dass für diesen Request ein CGI ausgeführt werden soll.

    Wir setzen den state auf STATE_PREPARE_CGI und rufen executeCGI().
    executeCGI setzt dann STATE_CGI_RUNNING, registriert epoll und kehrt zurück.
    Der Hauptloop ruft dann onEvent() bei neuen Events auf.
*/

bool CgiHandler::handleCGIIfNeeded() {
    std::cout << "Location CGI: '" << location->cgi << "'" << std::endl;
    std::cout << "File Path: '" << filePath << "'" << std::endl;

    if (location->cgi.empty()) {
        std::cout << "CGI path is empty" << std::endl;
        return false;
    }

    try {
        std::string cgiPath = location->cgi;
        std::cout << "CGI Path: '" << cgiPath << "'" << std::endl;

        if (cgiPath.empty()) {
            Logger::red("No CGI path defined for this location.");
            return false;
        }

        if (access(cgiPath.c_str(), X_OK) != 0) {
            std::cout << "CGI path is not executable: " << strerror(errno) << std::endl;
            return false;
        }

        // State ändern: wir bereiten jetzt CGI vor
        state = STATE_PREPARE_CGI;
        executeCGI(cgiPath, filePath, {{"SCRIPT_FILENAME", filePath}, {"REQUEST_METHOD", method}});
        // Keine Blockade - wir kehren einfach zurück
        return true;
    } catch (const std::exception& e) {
        Logger::red(std::string("CGI Error: ") + e.what());
        errorHandler.sendErrorResponse(500, "Internal Server Error");
    }
    return false;
}