#include "Server.hpp"
void Server::logContext(const Context& ctx, const std::string& event) {
   std::string log = "Context [" + event + "]:\n";
   log += "FDs: epoll=" + std::to_string(ctx.epoll_fd);
   log += " server=" + std::to_string(ctx.server_fd);
   log += " client=" + std::to_string(ctx.client_fd) + "\n";

   log += "Type: " + requestTypeToString(ctx.type) + "\n";

   if (!ctx.method.empty())
       log += "Method: " + ctx.method + "\n";
   if (!ctx.path.empty())
       log += "Path: " + ctx.path + "\n";

   Logger::file(log);
}

std::string Server::requestTypeToString(RequestType type) {
	switch(type) {
	    case RequestType::INITIAL: return "INITIAL";
	    case RequestType::STATIC: return "STATIC";
	    case RequestType::CGI: return "CGI";
	    case RequestType::ERROR: return "ERROR";
	    default: return "UNKNOWN";
	}
}

int Server::runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs) {
   const int max_events = 64;
   struct epoll_event events[max_events];
   const int timeout_ms = 500;
   while (true) {
       int n = epoll_wait(epoll_fd, events, max_events, timeout_ms);
       Logger::file("Events count: " + std::to_string(n));

       if (n < 0) {
           Logger::file("Epoll error: " + std::string(strerror(errno)));
           if (errno == EINTR) continue;
           break;
       }
       else if (n == 0) {
           checkAndCleanupTimeouts();
           continue;
       }

       for (int i = 0; i < n; i++) {
           int fd = events[i].data.fd;
           uint32_t ev = events[i].events;
           Logger::file("Event: fd=" + std::to_string(fd) +
                       " events=" + std::to_string(ev));
           dispatchEvent(epoll_fd, fd, ev, configs);
       }
       checkAndCleanupTimeouts();
   }
   close(epoll_fd);
   return EXIT_SUCCESS;
}

std::string Server::normalizePath(const std::string& raw)
{
	if (raw.empty()) return "/";
	std::string path = (raw[0] == '/') ? raw : "/" + raw;
	if (path.size() > 1 && path.back() == '/')
		path.pop_back();
	return path;
}

bool Server::matchLoc(const Location& loc, std::string rawPath) {
	std::string path = normalizePath(rawPath);
	if (path == loc.path)  // Use . instead of ->
		return true;
	return false;
}

// Bestimmt Request-Typ aus Konfiguration
void Server::determineType(Context& ctx, const std::vector<ServerBlock>& configs) {
	const ServerBlock* conf = nullptr;

	// Finde passende Konfiguration
	for (const auto& config : configs) {
		if (config.server_fd == ctx.server_fd) {
			conf = &config;
			break;
		}
	}

	if (!conf) {
		ctx.type = ERROR;
		return;
	}

	// Prüfe Location Blocks
	for (const auto& loc : conf->locations) {
		if (matchLoc(loc, ctx.path)) {
			ctx.location = &loc;
			if (loc.cgi != "") {
				ctx.type = CGI;
			} else {
				ctx.type = STATIC;
			}
			return;
		}
	}

	ctx.type = ERROR;
}


bool Server::dispatchEvent(int epoll_fd, int fd, uint32_t ev, std::vector<ServerBlock> &configs) {
    Logger::file("dispatchEvent");

    // 1. Check if this is an existing client connection
    if (globalFDS.request_state_map.count(fd)) {
        Context& ctx = globalFDS.request_state_map[fd];
        ctx.last_activity = std::chrono::steady_clock::now();

        // Handle errors
        if (ev & (EPOLLHUP | EPOLLERR)) {
            Logger::file("Connection Error");
            // TODO: Implement cleanup
            return true;
        }

        // Handle incoming data for INITIAL state
        if (ev & EPOLLIN && ctx.type == RequestType::INITIAL) {
            parseRequest(ctx);
            determineType(ctx, configs);
            logContext(ctx, "After Parse");
            return true;
        }

        // Handle based on request type
        switch(ctx.type) {
            case STATIC:
                Logger::file("Handling STATIC request");
                // TODO: Implement static file handling
                return true;

            case CGI:
                Logger::file("Handling CGI request");
                // TODO: Implement CGI handling
                return true;

            case ERROR:
                Logger::file("Handling ERROR request");
                // TODO: Implement error handling
                return true;

            default:
                return true;
        }
    }

    // 2. Check if this is a server socket (new connection)
    if (findServerBlock(configs, fd)) {
        Logger::file("New connection on server socket: " + std::to_string(fd));
        return handleNewConnection(epoll_fd, fd);
    }

    // 3. Unknown fd - this shouldn't happen
    Logger::file("ERROR: Unknown fd: " + std::to_string(fd));
    return false;
}


// enum RequestType {
// 	STATIC,
// 	CGI,
// 	ERROR
// };

// struct Context
// {
// 	RequestType type;
// 	std::string method;
// 	std::string path;
// 	std::map<std::string, std::string> headers;
// 	std::string body;
// 	std::chrono::steady_clock::time_point last_activity;
// 	static constexpr std::chrono::seconds TIMEOUT_DURATION{5};

// 	std::string location_path;
// 	std::string requested_path;
// };


void Server::parseRequest(Context& ctx) {
	char buffer[8192];
	ssize_t bytes = read(ctx.client_fd, buffer, sizeof(buffer));

	if (bytes <= 0) return;

	std::string request(buffer, bytes);
	std::istringstream stream(request);
	std::string line;

	// Parse request line
	if (std::getline(stream, line)) {
	    std::istringstream request_line(line);
	    request_line >> ctx.method >> ctx.path >> ctx.version;
	}

	// Parse headers
	while (std::getline(stream, line) && !line.empty() && line != "\r") {
	    size_t colon = line.find(':');
	    if (colon != std::string::npos) {
	        std::string key = line.substr(0, colon);
	        std::string value = line.substr(colon + 2); // Skip ": "
	        ctx.headers[key] = value;
	    }
	}

	// Parse body if present
	if (ctx.headers.count("Content-Length")) {
	    size_t length = std::stoi(ctx.headers["Content-Length"]);
	    std::vector<char> body_buffer(length);
	    stream.read(body_buffer.data(), length);
	    ctx.body = std::string(body_buffer.data(), length);
	}

	ctx.last_activity = std::chrono::steady_clock::now();
	logContext(ctx, "Parsed");
}


void Server::checkAndCleanupTimeouts() {
   auto now = std::chrono::steady_clock::now();

   for (auto it = globalFDS.request_state_map.begin(); it != globalFDS.request_state_map.end();) {
       Context& ctx = it->second;

       auto duration = std::chrono::duration_cast<std::chrono::seconds>(
           now - ctx.last_activity
       ).count();

       if (duration > Context::TIMEOUT_DURATION.count()) {
           if (ctx.type == CGI) {
               //killTimeoutedCGI(ctx);
           }

           modEpoll(globalFDS.epoll_fd, ctx.client_fd, EPOLLOUT);
           // Cleanup context
           it = globalFDS.request_state_map.erase(it);
       } else {
           ++it;
       }
   }
}

void Server::killTimeoutedCGI(RequestState &req) {
	if (req.cgi_pid > 0) {
		kill(req.cgi_pid, SIGTERM);
		//Logger::file(std::to_string(req.associated_conf->timeout));
		std::this_thread::sleep_for(std::chrono::microseconds(req.associated_conf->timeout));
		int status;
		pid_t result = waitpid(req.cgi_pid, &status, WNOHANG);
		if (result == 0) {
			kill(req.cgi_pid, SIGKILL);
			waitpid(req.cgi_pid, &status, 0);
		}
		req.cgi_pid = -1;
	}
	if (req.cgi_in_fd != -1) {
		epoll_ctl(globalFDS.epoll_fd, EPOLL_CTL_DEL, req.cgi_in_fd, NULL);
		close(req.cgi_in_fd);
		globalFDS.svFD_to_clFD_map.erase(req.cgi_in_fd);
		req.cgi_in_fd = -1;
	}
	if (req.cgi_out_fd != -1) {
		epoll_ctl(globalFDS.epoll_fd, EPOLL_CTL_DEL, req.cgi_out_fd, NULL);
		close(req.cgi_out_fd);
		globalFDS.svFD_to_clFD_map.erase(req.cgi_out_fd);
		req.cgi_out_fd = -1;
	}
	if (req.pipe_fd != -1) {
		epoll_ctl(globalFDS.epoll_fd, EPOLL_CTL_DEL, req.pipe_fd, NULL);
		close(req.pipe_fd);
		req.pipe_fd = -1;
	}
}