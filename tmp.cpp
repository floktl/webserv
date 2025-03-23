// Zur Verwaltung der CGI-Prozesse und Pipes in den globalFDS
struct GlobalFDS {
    // ... bestehende Felder

    // Mapping von CGI output pipe FD zu Client FD
    std::map<int, int> cgi_pipe_to_client_fd;

    // Mapping von PID zu Client FD für CGI-Prozesse
    std::map<pid_t, int> cgi_pid_to_client_fd;
};

bool Server::executeCgi(Context& ctx)
{
    Logger::green("executeCgi");

    // Validierung wie bisher...
    if (ctx.requested_path.empty() || !fileExists(ctx.requested_path) || !fileReadable(ctx.requested_path)) {
        Logger::error("CGI script validation failed: " + ctx.requested_path);
        return updateErrorStatus(ctx, 404, "Not Found - CGI Script");
    }

    int input_pipe[2];
    int output_pipe[2];

    if (pipe(input_pipe) < 0 || pipe(output_pipe) < 0) {
        Logger::error("Failed to create pipes for CGI");
        Logger::file("Failed to create pipes for CGI");
        return updateErrorStatus(ctx, 500, "Internal Server Error - Pipe Creation Failed");
    }

    if (setNonBlocking(input_pipe[0]) < 0 || setNonBlocking(input_pipe[1]) < 0 ||
        setNonBlocking(output_pipe[0]) < 0 || setNonBlocking(output_pipe[1]) < 0) {
        Logger::error("Failed to set pipes to non-blocking mode");
        Logger::file("Failed to set pipes to non-blocking mode");
        close(input_pipe[0]);
        close(input_pipe[1]);
        close(output_pipe[0]);
        close(output_pipe[1]);
        return updateErrorStatus(ctx, 500, "Internal Server Error - Non-blocking Pipe Failed");
    }

    pid_t pid = fork();

    if (pid < 0) {
        Logger::error("Failed to fork for CGI execution");
        Logger::file("Failed to fork for CGI execution");
        close(input_pipe[0]);
        close(input_pipe[1]);
        close(output_pipe[0]);
        close(output_pipe[1]);
        return updateErrorStatus(ctx, 500, "Internal Server Error - Fork Failed");
    }

    if (pid == 0) { // Child process
        // Child-Prozess-Code bleibt unverändert
        // Redirect stdin to input_pipe
        dup2(input_pipe[0], STDIN_FILENO);
        // Redirect stdout to output_pipe
        dup2(output_pipe[1], STDOUT_FILENO);
        // Also redirect stderr to stdout so we can capture PHP errors
        dup2(output_pipe[1], STDERR_FILENO);

        // Close unused pipe ends
        close(input_pipe[0]);
        close(input_pipe[1]);
        close(output_pipe[0]);
        close(output_pipe[1]);

        std::vector<std::string> env_vars = prepareCgiEnvironment(ctx);
        std::vector<char*> env_pointers;
        for (auto& var : env_vars) {
            env_pointers.push_back(const_cast<char*>(var.c_str()));
        }
        env_pointers.push_back(nullptr);

        logContext(ctx, "CGI");
        std::string interpreter;
        if (ctx.location.cgi_filetype == ".py") {
            interpreter = "/usr/bin/python";
        } else if (ctx.location.cgi_filetype == ".php") {
            interpreter = "/usr/bin/php-cgi"; // Make sure to use php-cgi, not php
        } else {
            // If no specific CGI filetype is matched, check if the file itself is executable
            if (fileExecutable(ctx.requested_path)) {
                interpreter = ctx.requested_path;
            }
        }

        char* args[3];
        args[0] = const_cast<char*>(interpreter.c_str());

        // For PHP-CGI, we need to pass the script path as an argument
        if (ctx.location.cgi_filetype == ".php") {
            args[1] = const_cast<char*>(ctx.requested_path.c_str());
            args[2] = nullptr;
        } else {
            // For other interpreters or executable scripts, we might not need an additional argument
            args[1] = nullptr;
        }

        execve(interpreter.c_str(), args, env_pointers.data());
        // Falls execve fehlschlägt, mit einem Fehler beenden
        exit(1);
    }

    // Parent process
    // Close unused pipe ends
    close(input_pipe[0]);
    close(output_pipe[1]);

    ctx.req.cgi_in_fd = input_pipe[1];
    ctx.req.cgi_out_fd = output_pipe[0];
    ctx.req.cgi_pid = pid;
    ctx.req.state = RequestBody::STATE_CGI_RUNNING;

    ctx.cgi_pipe_ready = false;
    ctx.cgi_read_attempts = 0;
    ctx.cgi_start_time = std::chrono::steady_clock::now();

    // Speichere Zuordnungen in den globalen Maps
    globalFDS.cgi_pipe_to_client_fd[ctx.req.cgi_out_fd] = ctx.client_fd;
    globalFDS.cgi_pid_to_client_fd[ctx.req.cgi_pid] = ctx.client_fd;

    // NEUES FEATURE: CGI output pipe zum epoll-Set hinzufügen
    struct epoll_event ev;
    std::memset(&ev, 0, sizeof(ev)); // Um Speicherprobleme zu vermeiden
    ev.events = EPOLLIN | EPOLLET;
    ev.data.fd = ctx.req.cgi_out_fd;
    if (epoll_ctl(ctx.epoll_fd, EPOLL_CTL_ADD, ctx.req.cgi_out_fd, &ev) < 0) {
        Logger::error("Failed to add CGI output pipe to epoll: " + std::string(strerror(errno)));
        // Wir machen hier weiter, auch wenn die epoll-Registrierung fehlgeschlagen ist
        // Der normale non-blocking Poll-Mechanismus wird als Fallback funktionieren
    } else {
        Logger::green("Added CGI output pipe " + std::to_string(ctx.req.cgi_out_fd) + " to epoll");
    }

    // Write POST data to the CGI input if it exists
    if (!ctx.body.empty()) {
        // For PHP, make sure we're writing properly formatted POST data
        write(ctx.req.cgi_in_fd, ctx.body.c_str(), ctx.body.length());
    }

    // Close the input pipe after writing all data
    close(ctx.req.cgi_in_fd);
    ctx.req.cgi_in_fd = -1;

    ctx.last_activity = std::chrono::steady_clock::now();

    ctx.cgi_executed = true;
    return (modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT | EPOLLET));
}

bool Server::sendCgiResponse(Context& ctx) {
    Logger::green("sendCgiResponse");

    if (ctx.cgi_pipe_ready) {
        if (ctx.cgi_output_phase) {
            return readCgiOutput(ctx);
        } else {
            return sendCgiBuffer(ctx);
        }
    }

    // Prüfe auf Timeout
    auto now = std::chrono::steady_clock::now();
    auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - ctx.cgi_start_time).count();

    // Timeout-Check (30 Sekunden als Beispiel)
    if (elapsed > 30) {
        Logger::error("CGI process timeout: " + std::to_string(ctx.req.cgi_pid));
        // Kill the process
        kill(ctx.req.cgi_pid, SIGTERM);
        // Bereinige Ressourcen und gib Fehler zurück
        cleanupCgiResources(ctx);
        return updateErrorStatus(ctx, 504, "Gateway Timeout - CGI Process Timeout");
    }

    // Wir prüfen das einmalig nur noch, um zu sehen, ob der Prozess bereits beendet wurde
    // Da wir jetzt den Pipe-FD im epoll haben, sollten wir die meisten Daten darüber bekommen
    int status;
    pid_t result = waitpid(ctx.req.cgi_pid, &status, WNOHANG);

    if (result > 0) {
        // CGI-Prozess ist beendet
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
            Logger::error("CGI-Prozess beendet mit Fehlercode: " + std::to_string(WEXITSTATUS(status)));
            cleanupCgiResources(ctx);
            return updateErrorStatus(ctx, 500, "Internal Server Error - CGI Process Failed");
        }

        // Normalerweise würden wir hier ctx.cgi_pipe_ready = true setzen,
        // aber da wir Events vom Pipe direkt bekommen, ist das nicht nötig

        // Stattdessen prüfen wir, ob noch Daten im Pipe sind
        return checkAndReadCgiPipe(ctx);
    }

    // Wenn der Prozess nicht beendet ist und wir hier landen, warten wir
    // auf das nächste EPOLLIN-Event vom Pipe oder einen Timeout
    return true;
}

// Neue Hilfsfunktion zum Aufräumen der CGI-Ressourcen
void Server::cleanupCgiResources(Context& ctx) {
    // Pipe schließen, wenn noch offen
    if (ctx.req.cgi_out_fd > 0) {
        // Aus epoll entfernen
        epoll_ctl(ctx.epoll_fd, EPOLL_CTL_DEL, ctx.req.cgi_out_fd, nullptr);
        // Aus der globalFDS Map entfernen
        globalFDS.cgi_pipe_to_client_fd.erase(ctx.req.cgi_out_fd);
        // Pipe schließen
        close(ctx.req.cgi_out_fd);
        ctx.req.cgi_out_fd = -1;
    }

    // PID aus der Map entfernen
    globalFDS.cgi_pid_to_client_fd.erase(ctx.req.cgi_pid);

    // CGI-Zustand zurücksetzen
    ctx.cgi_terminate = true;
    ctx.cgi_terminated = true;
}

// Neue Hilfsfunktion zum Prüfen und Lesen aus dem CGI-Pipe
bool Server::checkAndReadCgiPipe(Context& ctx) {
    // Diese Funktion simuliert ein EPOLLIN-Event für den CGI-Pipe
    // Wird aufgerufen, wenn der Prozess beendet ist oder wenn wir
    // ein echtes EPOLLIN-Event vom epoll bekommen

    ctx.cgi_pipe_ready = true;
    ctx.cgi_output_phase = true;

    if (!ctx.cgi_headers_send) {
        return processAndPrepareHeaders(ctx);
    } else {
        return readCgiOutput(ctx);
    }
}

bool Server::readCgiOutput(Context& ctx) {
    Logger::green("readCgiOutput");

    // Update activity timestamp
    ctx.last_activity = std::chrono::steady_clock::now();

    // If the CGI process has been terminated
    if (ctx.req.cgi_out_fd < 0 && ctx.cgi_terminated) {
        return true;
    }

    // Handle headers if not sent yet
    if (!ctx.cgi_headers_send) {
        return processAndPrepareHeaders(ctx);
    }

    // Read additional CGI output
    char buffer[DEFAULT_REQUESTBUFFER_SIZE];
    std::memset(buffer, 0, sizeof(buffer));

    Logger::magenta("read CGI output");
    ssize_t bytes = read(ctx.req.cgi_out_fd, buffer, sizeof(buffer));

    if (bytes < 0) {
        // Handle read error
        if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // Pipe ist nicht bereit, später erneut versuchen
            // Da wir jetzt ein epoll-Event für den Pipe haben,
            // sollte diese Situation seltener auftreten
            return true;
        }

        // Some other error occurred
        Logger::error("Failed to read from CGI output: " + std::string(strerror(errno)));
        cleanupCgiResources(ctx);
        return updateErrorStatus(ctx, 500, "Internal Server Error - CGI Read Failed");
    }

    if (bytes == 0) {
        // End of CGI output
        // Aus epoll entfernen
        epoll_ctl(ctx.epoll_fd, EPOLL_CTL_DEL, ctx.req.cgi_out_fd, nullptr);
        // Aus der globalFDS Map entfernen
        globalFDS.cgi_pipe_to_client_fd.erase(ctx.req.cgi_out_fd);
        // Pipe schließen
        close(ctx.req.cgi_out_fd);
        ctx.req.cgi_out_fd = -1;

        // If we haven't sent any data yet, add a marker to show we're done
        if (ctx.write_buffer.empty()) {
            const char* end_marker = "\r\n\r\n"; // Empty response
            ctx.write_buffer.assign(end_marker, end_marker + 4);
        }

        ctx.cgi_terminate = true;
    } else {
        // Append new data to existing buffer
        ctx.write_buffer.insert(ctx.write_buffer.end(), buffer, buffer + bytes);
    }

    // Switch to send phase
    ctx.cgi_output_phase = false;
    return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
}

// Die restlichen Funktionen (processAndPrepareHeaders, sendCgiBuffer) bleiben weitgehend unverändert
// Hier würden nur Anpassungen für die Ressourcenbereinigung mit cleanupCgiResources gemacht

// Zusätzliche Funktion für den Event-Loop, die prüft, ob ein eingehendes Event ein CGI-Pipe ist
bool Server::handleCgiPipeEvent(int epoll_fd, int incoming_fd, uint32_t events, std::vector<ServerBlock> &configs) {
    // Prüfe zuerst, ob das eingehende FD ein CGI-Pipe ist
    auto pipe_iter = globalFDS.cgi_pipe_to_client_fd.find(incoming_fd);
    if (pipe_iter != globalFDS.cgi_pipe_to_client_fd.end()) {
        int client_fd = pipe_iter->second;

        // Suche den zugehörigen Context
        auto ctx_iter = globalFDS.context_map.find(client_fd);
        if (ctx_iter != globalFDS.context_map.end()) {
            Context& ctx = ctx_iter->second;

            // CGI-Pipe ist bereit zum Lesen
            Logger::green("CGI Pipe " + std::to_string(incoming_fd) + " ready for client " + std::to_string(client_fd));

            ctx.cgi_pipe_ready = true;
            ctx.cgi_output_phase = true;

            // Verarbeite die CGI-Ausgabe
            checkAndReadCgiPipe(ctx);

            // Versuche direkt, die Daten zu senden
            return modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);
        }
    }

    // Falls es kein CGI-Pipe war, normalen Event-Loop-Code fortsetzen
    return false;
}

// Modifizierte runEventLoop-Funktion (nur relevante Teile gezeigt)
int Server::runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs)
{
    struct epoll_event events[MAX_EVENTS];
    int incoming_fd = -1;
    int server_fd = -1;
    int client_fd = -1;
    int eventNum;

    while (!g_shutdown_requested)
    {
        eventNum = epoll_wait(epoll_fd, events, MAX_EVENTS, TIMEOUT_MS);
        if (eventNum < 0)
        {
            if (errno == EINTR)
                break;
            Logger::errorLog("Epoll error: " + std::string(strerror(errno)));
            break;
        }
        else if (eventNum == 0)
        {
            checkAndCleanupTimeouts();
            continue;
        }

        for (int eventIter = 0; eventIter < eventNum; eventIter++)
        {
            incoming_fd = events[eventIter].data.fd;

            // Prüfe, ob es ein CGI-Pipe ist
            if (globalFDS.cgi_pipe_to_client_fd.find(incoming_fd) != globalFDS.cgi_pipe_to_client_fd.end()) {
                handleCgiPipeEvent(epoll_fd, incoming_fd, events[eventIter].events, configs);
                continue;
            }

            // Restlicher Event-Loop-Code bleibt wie vorher
            auto ctx_iter = globalFDS.context_map.find(incoming_fd);
            bool is_active_upload = false;

            if (ctx_iter != globalFDS.context_map.end())
            {
                Context& ctx = ctx_iter->second;
                if (isMultipart(ctx) && ctx.req.parsing_phase == RequestBody::PARSING_BODY &&
                        !ctx.req.is_upload_complete)
                    is_active_upload = true;
            }

            if (!is_active_upload && findServerBlock(configs, incoming_fd))
            {
                server_fd = incoming_fd;
                acceptNewConnection(epoll_fd, server_fd, configs);
            }
            else
            {
                client_fd = incoming_fd;
                handleAcceptedConnection(epoll_fd, client_fd, events[eventIter].events, configs);
            }
        }
    }
    close(epoll_fd);
    epoll_fd = -1;
    return EXIT_SUCCESS;
}

// Funktion zum Aufräumen von Timeouts, die CGI-Prozesse prüft und beendet
void Server::checkAndCleanupTimeouts() {
    auto now = std::chrono::steady_clock::now();

    // Überprüfe alle aktiven CGI-Kontexte
    for (auto it = globalFDS.cgi_pid_to_client_fd.begin(); it != globalFDS.cgi_pid_to_client_fd.end(); /* increment in body */) {
        pid_t pid = it->first;
        int client_fd = it->second;

        auto ctx_iter = globalFDS.context_map.find(client_fd);
        if (ctx_iter != globalFDS.context_map.end()) {
            Context& ctx = ctx_iter->second;

            // CGI-Timeout prüfen (30 Sekunden als Beispiel)
            auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(now - ctx.cgi_start_time).count();
            if (elapsed > 30) {
                Logger::error("Timeout for CGI process " + std::to_string(pid));

                // Beende den Prozess
                kill(pid, SIGTERM);

                // Warte kurz und beende dann mit SIGKILL, falls nötig
                usleep(100000); // 100ms
                int status;
                if (waitpid(pid, &status, WNOHANG) == 0) {
                    // Prozess läuft noch, töte ihn brutal
                    kill(pid, SIGKILL);
                }

                // Ressourcen aufräumen
                cleanupCgiResources(ctx);

                // Setze Error-Status im Context
                updateErrorStatus(ctx, 504, "Gateway Timeout - CGI Process Timeout");

                // Sorge dafür, dass die Antwort gesendet wird
                modEpoll(ctx.epoll_fd, ctx.client_fd, EPOLLOUT);

                // Entferne aus der Map und gehe zum nächsten Element
                auto currentIt = it++;
                globalFDS.cgi_pid_to_client_fd.erase(currentIt);
            } else {
                ++it;
            }
        } else {
            // Context nicht mehr vorhanden, entferne aus der Map
            auto currentIt = it++;
            globalFDS.cgi_pid_to_client_fd.erase(currentIt);
        }
    }

    // Überprüfe andere Timeouts wie bisher
    // ...
}