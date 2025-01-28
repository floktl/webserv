#include "Server.hpp"

void Server::modEpoll(int epoll_fd, int fd, uint32_t events) {
	if (fd <= 0) {
        Logger::errorLog("WARNING: Attempt to modify epoll for invalid fd: " + std::to_string(fd));
        return;
    }
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = fd;

	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0) {
		if (errno != EEXIST) {
			Logger::errorLog("Epoll add failed: " + std::string(strerror(errno)));
			return;
		}
		if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0) {
			Logger::errorLog("Epoll mod failed: " + std::string(strerror(errno)));
		}
	}
}

void Server::delFromEpoll(int epfd, int fd)
{
	 if (epfd <= 0 || fd <= 0) {
        Logger::errorLog("WARNING: Attempt to delete invalid fd: " + std::to_string(fd));
        return;
    }
	if (findServerBlock(configData, fd)) {
		Logger::errorLog("Prevented removal of server socket: " + std::to_string(fd));
		return;
	}
	//Logger::file("Removing fd " + std::to_string(fd) + " from epoll");

	if (epoll_ctl(epfd, EPOLL_CTL_DEL, fd, NULL) < 0)
		Logger::errorLog("Error removing from epoll: " + std::string(strerror(errno)));

	auto ctx_it = globalFDS.request_state_map.find(fd);
	if (ctx_it != globalFDS.request_state_map.end()) {
		RequestBody &req = ctx_it->second.req;
		if (req.cgi_in_fd != -1) {
			close(req.cgi_in_fd);
			globalFDS.clFD_to_svFD_map.erase(req.cgi_in_fd);
		}
		if (req.cgi_out_fd != -1) {
			close(req.cgi_out_fd);
			globalFDS.clFD_to_svFD_map.erase(req.cgi_out_fd);
		}

		globalFDS.request_state_map.erase(fd);
	}
	Logger::file("removed from clFD_to_svFD_map[clFD:" + std::to_string(fd) + "] = svFD(" + std::to_string(globalFDS.clFD_to_svFD_map[fd]) + ")");
	close(fd);
	globalFDS.clFD_to_svFD_map.erase(fd);

	//Logger::file("Successfully removed fd " + std::to_string(fd));
}

bool Server::findServerBlock(const std::vector<ServerBlock> &configs, int fd)
{
	//Logger::file("Finding ServerBlock for fd: " + std::to_string(fd));
	for (const auto &conf : configs) {
		////Logger::file("Checking config with fd: " + std::to_string(conf.server_fd));
		if (conf.server_fd == fd) {
			//Logger::file("Found matching ServerBlock with fd: " + std::to_string(conf.server_fd) + " on Port: " + std::to_string(conf.port));
			return true;
		}
	}
	//Logger::file("No matching ServerBlock found");
	return false;
}

int Server::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}


// void Server::setTaskStatus(enum RequestBody::Task new_task, int client_fd)
// {
// 	if (client_fd < 0)
// 	{
// 		//Logger::file("Error: Invalid client_fd " + std::to_string(client_fd));
// 		return;
// 	}

// 	auto it = globalFDS.request_state_map.find(client_fd);

// 	if (it != globalFDS.request_state_map.end())
// 	{
// 		it->second.task = new_task;

// 		std::string taskName = (new_task == RequestBody::PENDING ? "PENDING" :
// 							new_task == RequestBody::IN_PROGRESS ? "IN_PROGRESS" :
// 							new_task == RequestBody::COMPLETED ? "COMPLETED" :
// 							"UNKNOWN");

// 	}
// 	else
// 	{
// 		//Logger::file("Error: client_fd " + std::to_string(client_fd) + " not found in request_state_map");
// 	}
// }


// enum RequestBody::Task Server::getTaskStatus(int client_fd)
// {
// 	auto it = globalFDS.request_state_map.find(client_fd);

// 	if (it != globalFDS.request_state_map.end())
// 	{
// 		return it->second.task;
// 	}
// 	else
// 	{
// 		//Logger::file("Error: client_fd " + std::to_string(client_fd) + " not found in request_state_map");
// 		return RequestBody::Task::PENDING;
// 	}
// }

bool Server::addServerNameToHosts(const std::string &server_name)
{
	const std::string hosts_file = "/etc/hosts";
	std::ifstream infile(hosts_file);
	std::string line;

	// Check if the server_name already exists in /etc/hosts
	while (std::getline(infile, line)) {
		if (line.find(server_name) != std::string::npos) {
			return true; // Already present, no need to add
		}
	}

	// Add server_name to /etc/hosts
	std::ofstream outfile(hosts_file, std::ios::app);
	if (!outfile.is_open()) {
		throw std::runtime_error("Failed to open /etc/hosts");
	}
	outfile << "127.0.0.1 " << server_name << "\n";
	outfile.close();
	Logger::yellow("Added " + server_name + " to /etc/hosts file");
	// Store the added server name
	added_server_names.push_back(server_name);
	return true;
}

void Server::removeAddedServerNamesFromHosts()
{
	const std::string hosts_file = "/etc/hosts";
	std::ifstream infile(hosts_file);
	if (!infile.is_open()) {
		throw std::runtime_error("Failed to open /etc/hosts");
	}

	std::vector<std::string> lines;
	std::string line;
	while (std::getline(infile, line)) {
		bool shouldRemove = false;
		for (const auto &name : added_server_names) {
			if (line.find(name) != std::string::npos) {
				Logger::yellow("Remove " + name + " from /etc/host file");
				shouldRemove = true;
				break;
			}
		}
		if (!shouldRemove) {
			lines.push_back(line);
		}
	}
	infile.close();

	std::ofstream outfile(hosts_file, std::ios::trunc);
	if (!outfile.is_open()) {
		throw std::runtime_error("Failed to open /etc/hosts for writing");
	}
	for (const auto &l : lines) {
		outfile << l << "\n";
	}
	outfile.close();

	// Clear the added server names
	added_server_names.clear();
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
    if (rawPath.empty()) {
        rawPath = "/";
    }

    if (rawPath[0] != '/') {
        rawPath = "/" + rawPath;
    }

    std::string normalizedPath;
    bool lastWasSlash = false;

    for (size_t i = 0; i < rawPath.length(); ++i) {
        if (rawPath[i] == '/') {
            if (!lastWasSlash) {
                normalizedPath += '/';
            }
            lastWasSlash = true;
        } else {
            normalizedPath += rawPath[i];
            lastWasSlash = false;
        }
    }

    if (normalizedPath == loc.path) {
        return true;
    }

    if (loc.path.length() <= normalizedPath.length() &&
        normalizedPath.substr(0, loc.path.length()) == loc.path) {
        if (loc.path[loc.path.length()-1] != '/') {
            return normalizedPath[loc.path.length()] == '/';
        }
        return true;
    }

    return false;
}

void Server::logRequestBodyMapFDs()
{
	//Logger::file("Logging all FDs in request_state_map:");

	if (globalFDS.request_state_map.empty())
	{
		//Logger::file("[empty]");
		return;
	}

	std::string log = "Active FDs: ";
	for (const auto& pair : globalFDS.request_state_map)
	{
		log += std::to_string(pair.first) + " ";
	}

	//Logger::file(log);
}

std::string Server::concatenatePath(const std::string& root, const std::string& path)
{
	if (root.empty())
		return path;
	if (path.empty())
		return root;

	if (root.back() == '/' && path.front() == '/')
	{
		return root + path.substr(1);  // Entferne das führende '/'
	}
	else if (root.back() != '/' && path.front() != '/')
	{
		return root + '/' + path;  // Füge einen '/' hinzu
	}
	else
	{
		return root + path;  // Einfach zusammenfügen
	}
}

std::string Server::getDirectory(const std::string& path) {
	size_t lastSlash = path.find_last_of("/\\");
	if (lastSlash != std::string::npos) {
		return path.substr(0, lastSlash);
	}
	return "";
}

void Server::parseRequest(Context& ctx) {
		//Logger::file("parseRequest");

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

void Server::killTimeoutedCGI(RequestBody &req) {
	if (req.cgi_pid > 0) {
		kill(req.cgi_pid, SIGTERM);
		////Logger::file(std::to_string(req.associated_conf->timeout));
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
		globalFDS.clFD_to_svFD_map.erase(req.cgi_in_fd);
		req.cgi_in_fd = -1;
	}
	if (req.cgi_out_fd != -1) {
		epoll_ctl(globalFDS.epoll_fd, EPOLL_CTL_DEL, req.cgi_out_fd, NULL);
		close(req.cgi_out_fd);
		globalFDS.clFD_to_svFD_map.erase(req.cgi_out_fd);
		req.cgi_out_fd = -1;
	}
	if (req.pipe_fd != -1) {
		epoll_ctl(globalFDS.epoll_fd, EPOLL_CTL_DEL, req.pipe_fd, NULL);
		close(req.pipe_fd);
		req.pipe_fd = -1;
	}
}

void Server::logContext(const Context& ctx, const std::string& event)
{
	std::string log = "Context [" + event + "]:\n";

	log += "FDs: epoll=" + (ctx.epoll_fd != -1 ? std::to_string(ctx.epoll_fd) : "[empty]");
	log += " server=" + (ctx.server_fd != -1 ? std::to_string(ctx.server_fd) : "[empty]");
	log += " client=" + (ctx.client_fd != -1 ? std::to_string(ctx.client_fd) : "[empty]") + "\n";

	log += "Type: " + requestTypeToString(ctx.type) + "\n";

	log += "Method: " + (!ctx.method.empty() ? ctx.method : "[empty]") + "\n";
	log += "Path: " + (!ctx.path.empty() ? ctx.path : "[empty]") + "\n";
	log += "Version: " + (!ctx.version.empty() ? ctx.version : "[empty]") + "\n";

	log += "Headers:\n";
	if (ctx.headers.empty())
	{
		log += "[empty]\n";
	}
	else
	{
		for (const auto& header : ctx.headers)
		{
			log += header.first + " BDFLNDKSFJN: " + header.second + "\n";
		}
	}

	log += "Body: " + (!ctx.body.empty() ? ctx.body : "[empty]") + "\n";

	log += "Location Path: " + (!ctx.location_path.empty() ? ctx.location_path : "[empty]") + "\n";
	log += "Requested Path: " + (!ctx.requested_path.empty() ? ctx.requested_path : "[empty]") + "\n";

	log += std::string("Location: ") + (ctx.location ? "Defined" : "[empty]") + "\n";


	log += "Last Activity: " +
		std::to_string(std::chrono::duration_cast<std::chrono::seconds>(
							ctx.last_activity.time_since_epoch())
							.count()) +
		" seconds since epoch";

	//Logger::file(log);
}


std::string Server::requestTypeToString(RequestType type)
{
	switch(type) {
		case RequestType::INITIAL: return "INITIAL";
		case RequestType::STATIC: return "STATIC";
		case RequestType::CGI: return "CGI";
		case RequestType::ERROR: return "ERROR";
		default: return "UNKNOWN";
	}
}

bool Server::isRequestComplete(const Context& ctx)
{
	// First check if headers are complete
	if (!ctx.headers_complete) {
		return false;
	}

	// If no Content-Length header, request is complete after headers
	auto it = ctx.headers.find("Content-Length");
	if (it == ctx.headers.end()) {
		return true;
	}

	// Check if we've received the full body
	return ctx.body_received >= ctx.content_length;
}
