#include "Server.hpp"

int Server::runEventLoop(int epoll_fd, std::vector<ServerBlock> &configs)
{
	const int			max_events = 64;
	struct epoll_event	events[max_events];
	const int			timeout_ms = 1000;
	int					incoming_fd = -1;

	//Logger::file("\n");
	while (true)
	{
		int n = epoll_wait(epoll_fd, events, max_events, timeout_ms);
		////Logger::file("Events count: " + std::to_string(n));

		if (n < 0)
		{
			Logger::errorLog("Epoll error: " + std::string(strerror(errno)));
			if (errno == EINTR) continue;
			break;
		}
		else if (n == 0)
		{
			checkAndCleanupTimeouts();
			continue;
		}

		for (int i = 0; i < n; i++)
		{
			incoming_fd = events[i].data.fd;

			if (incoming_fd <= 0)
			{
				Logger::errorLog("WARNING: Invalid fd " + std::to_string(incoming_fd) + " in epoll event");
				continue;
			}
			uint32_t ev = events[i].events;
			if (dispatchEvent(epoll_fd, incoming_fd, ev, configs))
			{
				// handle successfull dispatch
			}
			else
			{
				Logger::errorLog("dispatch event error");
				log_global_fds(globalFDS);

				// differentiate between normal errorhandler and (bug erros)
			}
		}
	}
	close(epoll_fd);
	epoll_fd = -1;
	return EXIT_SUCCESS;
}

bool Server::dispatchEvent(int epoll_fd, int incoming_fd, uint32_t ev, std::vector<ServerBlock> &configs)
{
	int server_fd = -1;
	int client_fd = -1;

	Logger::file("");
	Logger::file(getEventDescription(ev));
	//log_server_configs(configs);
	// Determine if incoming_fd is a server or client fd
	if (findServerBlock(configs, incoming_fd))
	{
		server_fd = incoming_fd;
		Logger::file("dispatchEvent: Server fd identified: " + std::to_string(server_fd) + " events=" + std::to_string(ev));
		if (server_fd > 0)
		{
			return handleNewConnection(epoll_fd, server_fd);
		}
		else
		{
			Logger::errorLog("server_fd: " + std::to_string(server_fd) + "invalid in findserverblock");
			return false; // Error
		}
	}
	else
	{
		client_fd = incoming_fd;
		if (client_fd > 0)
		{
			Logger::file("handleAcceptedConnection client fd: " + std::to_string(client_fd) + " events=" + std::to_string(ev));
			return handleAcceptedConnection(epoll_fd, client_fd, ev, configs);
		}
		else
		{
			Logger::errorLog("client_fd: " + std::to_string(client_fd));
			return false; // Error
		}
	}
	//log_global_fds(globalFDS);
	Logger::errorLog("dispatch event not sucessful: Client_fd: " + std::to_string(client_fd) + " server_fd: " + std::to_string(server_fd));
	return false; // Error in Dispatch event
}

bool Server::handleAcceptedConnection(int epoll_fd, int client_fd, uint32_t ev, std::vector<ServerBlock> &configs)
{
	std::map<int, Context>::iterator contextIter = globalFDS.context_map.find(client_fd);
	bool		status = false;

	if (contextIter != globalFDS.context_map.end())
	{
		Context		&ctx = contextIter->second;
		ctx.last_activity = std::chrono::steady_clock::now();
		// Handle reads
		if (ev & EPOLLIN)
		{
			status = handleRead(ctx, configs);
			//log_global_fds(globalFDS);
			//logContext(ctx);
		}

		// Handle writes
		if ((ev & EPOLLOUT))
		{
			//log_global_fds(globalFDS);
			status = handleWrite(ctx);

			if (status == false)
			{
				//Logger::errorLog("Connection error in handle write: client_fd: " + std::to_string(client_fd));
				//log_global_fds(globalFDS);
				//Logger::file("after handle write()");
				delFromEpoll(epoll_fd, client_fd);
				return status;
			}

			//logContext(ctx);
			return status;
		}

		// Handle errors/disconnects
		if (ev & (EPOLLHUP | EPOLLERR))
		{
			status = false;
			Logger::errorLog("Connection error on client_fd " + std::to_string(client_fd));
			delFromEpoll(epoll_fd, client_fd);
			//log_global_fds(globalFDS);
		}
		if (status == false)
		{
			Logger::errorLog("handleAcceptConnection Error: client_fd " + std::to_string(client_fd));
			if (ctx.error_code != 0)
					return (errorsHandler(ctx));
		}
		return status;
	}

	Logger::errorLog("Unknown fd: " + std::to_string(client_fd));
	//delFromEpoll(epoll_fd, client_fd);
	//log_global_fds(globalFDS);
	return true;
}
bool Server::isMethodAllowed(Context& ctx)
{

	bool isAllowed = false;
	std::string reason;

	if (ctx.method == "GET" && ctx.location.allowGet) {
		isAllowed = true;
		reason = "GET method allowed for this location";
	}
	else if (ctx.method == "POST" && ctx.location.allowPost) {
		isAllowed = true;
		reason = "POST method allowed for this location";
	}
	else if (ctx.method == "DELETE" && ctx.location.allowDelete) {
		isAllowed = true;
		reason = "DELETE method allowed for this location";
	}
	else if (ctx.method == "COOKIE" && ctx.location.allowCookie) {
		isAllowed = true;
		reason = "COOKIE method allowed for this location";
	}
	else {
		reason = ctx.method + " method not allowed for this location";
	}
	return isAllowed;
}

std::string Server::approveExtention(Context& ctx, std::string path_to_check)
{
    std::string approvedIndex;

    size_t dot_pos = path_to_check.find_last_of('.');

    if (dot_pos != std::string::npos)
    {
        std::string extension = path_to_check.substr(dot_pos + 1);
        //Logger::file("Checking default file extension (without dot): " + extension);

        if (ctx.type != CGI || (extension == ctx.location.cgi_filetype && ctx.type == CGI))
        {
            approvedIndex = path_to_check;
            //Logger::file("Default file approved: " + approvedIndex);
        }
        else
        {
            //Logger::file("Extension validation failed - CGI type: [" + ctx.location.cgi_filetype + "], Extension: [" + extension + "]");
			updateErrorStatus(ctx, 500, "Internal Server Error");
            return "";
        }
    }
    else if (ctx.type == CGI && ctx.method != "POST")
    {
        updateErrorStatus(ctx, 400, "Bad Request: No file extension found");
        return "";
    }
    else if (!ctx.location.return_url.empty()) {
        return path_to_check;
    }
    else {
        updateErrorStatus(ctx, 404, "Not found");
        return "";
    }
    return approvedIndex;
}


void Server::parseAccessRights(Context& ctx)
{
	//Logger::file("parseAccessRights: Starting access rights parsing");

	//Logger::file("Before Requested Path: " + ctx.path);
	std::string requestedPath = concatenatePath(ctx.root, ctx.path);
	//Logger::file("Before catted Requested Path: " + requestedPath);
	if (ctx.index.empty()) {
		ctx.index = "index.html";
	}
	if (ctx.location.default_file.empty()) {
		ctx.location.default_file = ctx.index;
	}
	if (!requestedPath.empty() && requestedPath.back() == '/') {
		//Logger::file("apply default file config");
		requestedPath = concatenatePath(requestedPath, ctx.location.default_file);
	}
	if (ctx.location_inited && requestedPath == ctx.location.upload_store) {
		if (dirWritable(requestedPath))
 			return;
    }
	requestedPath = approveExtention(ctx, requestedPath);
	//Logger::file("After Requested Path: " + requestedPath);
	if (ctx.type == ERROR)
		return;
	checkAccessRights(ctx, requestedPath);
	//Logger::file("Execute permission check completed for: " + requestedPath);
	if (ctx.type == ERROR)
		return;
	ctx.approved_req_path = requestedPath;
	//Logger::file("parseAccessRights: Completed all access rights checks");

	// Additional debug information about context state
	if (ctx.error_code != 0)
	{
		std::string contextState = "Context state - Error code: " +
								std::to_string(ctx.error_code) +
								", Type: " + std::to_string(ctx.type);
		//Logger::file(contextState);
	}
}

bool Server::fileReadable(const std::string& path) {
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && (st.st_mode & S_IRUSR));
}

bool Server::fileExecutable(const std::string& path) {
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && (st.st_mode & S_IXUSR));
}

bool Server::dirReadable(const std::string& path) {
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & S_IRUSR));
}

bool Server::dirWritable(const std::string& path) {
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & S_IWUSR));
}

bool Server::checkAccessRights(Context &ctx, std::string path) {
    Logger::file("[INFO] Checking access rights for path: " + path);

    // Grundlegende Existenzprüfung
    struct stat path_stat;
    if (stat(path.c_str(), &path_stat) != 0) {
        Logger::file("[ERROR] Path not found: " + path);
		Logger::file("checkAccessRights");
        return updateErrorStatus(ctx, 404, "Not found");
    }

    // Verzeichnisbehandlung
    if (S_ISDIR(path_stat.st_mode)) {
        if (ctx.location_inited && ctx.location.autoindex == "on") {
            std::string dirPath = getDirectory(path) + '/';
            Logger::file("[INFO] Directory access with autoindex enabled: " + dirPath);

            // Verzeichnisberechtigungen prüfen
            if (!dirReadable(dirPath)) {
                Logger::file("[ERROR] Directory access forbidden: " + dirPath);
                return updateErrorStatus(ctx, 403, "Forbidden");
            }

            ctx.doAutoIndex = dirPath;
            ctx.type = STATIC;
            return true;
        } else {
            Logger::file("[ERROR] Directory listing forbidden (autoindex off): " + path);
            return updateErrorStatus(ctx, 403, "Forbidden");
        }
    }

    if (!fileReadable(path)) {
        Logger::file("[ERROR] Read permission denied: " + path);
        return updateErrorStatus(ctx, 403, "Forbidden");
    }

    if (ctx.type == CGI && !fileExecutable(path)) {
        Logger::file("[ERROR] Execute permission denied for CGI: " + path);
        return updateErrorStatus(ctx, 500, "CGI script not executable");
    }

    if (ctx.method == "POST") {
        std::string uploadDir = getDirectory(path);
        if (!dirWritable(uploadDir)) {
            Logger::file("[ERROR] Write permission denied to directory: " + uploadDir);
            return updateErrorStatus(ctx, 403, "Forbidden");
        }
    }

    if (path.length() > 4096) {
        Logger::file("[ERROR] Path exceeds maximum length: " + path);
        return updateErrorStatus(ctx, 414, "URI Too Long");
    }

    Logger::file("[INFO] Access rights check passed for: " + path);
    return true;
}


// Bestimmt Request-Typ aus Konfiguration
void Server::determineType(Context& ctx, std::vector<ServerBlock>& configs)
{
	ServerBlock* conf = nullptr;

	// Finde passende Konfiguration
	for (auto& config : configs) {
		if (config.server_fd == ctx.server_fd)
		{
			conf = &config;
			break;
		}
	}

	if (!conf)
	{
		updateErrorStatus(ctx, 500, "Internal Server Error");
		return;
	}

	// path und index file muss da sein. Pfad aufloesen..
	// autoindex
	// uploadstore

	for (auto loc : conf->locations)
	{
		if (matchLoc(loc, ctx.path))
		{
			ctx.port = conf->port;
			ctx.name = conf->name;
			ctx.root = conf->root;
			ctx.index = conf->index;
			ctx.errorPages = conf->errorPages;
			ctx.client_max_body_size = conf->client_max_body_size;
			ctx.timeout = conf->timeout;
			ctx.location = loc;
			ctx.location_inited = true;
			if (!isMethodAllowed(ctx))
			{
				updateErrorStatus(ctx, 405, "Method (" + ctx.method + ") not allowed");
				return;
			}
			if (loc.cgi != "")
				ctx.type = CGI;
			else
				ctx.type = STATIC;
			parseAccessRights(ctx);
			return;
		}
	}
	updateErrorStatus(ctx, 500, "Internal Server Error");
}

