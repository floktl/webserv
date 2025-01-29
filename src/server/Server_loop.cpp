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

	if (ctx.method == "GET" && ctx.location->allowGet) {
		isAllowed = true;
		reason = "GET method allowed for this location";
	}
	else if (ctx.method == "POST" && ctx.location->allowPost) {
		isAllowed = true;
		reason = "POST method allowed for this location";
	}
	else if (ctx.method == "DELETE" && ctx.location->allowDelete) {
		isAllowed = true;
		reason = "DELETE method allowed for this location";
	}
	else if (ctx.method == "COOKIE" && ctx.location->allowCookie) {
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

        if (ctx.type != CGI || (extension == ctx.location->cgi_filetype && ctx.type == CGI))
        {
            approvedIndex = path_to_check;
            //Logger::file("Default file approved: " + approvedIndex);
        }
        else
        {
            //Logger::file("Extension validation failed - CGI type: [" + ctx.location->cgi_filetype + "], Extension: [" + extension + "]");
            ctx.error_code = 500;
            ctx.type = ERROR;
            ctx.error_message = "Internal Server Error";
            return "";
        }
    }
    else if (ctx.type != CGI && ctx.method != "POST")
    {
        ctx.error_code = 400;
        ctx.type = ERROR;
        ctx.error_message = "Bad Request: No file extension found";
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
	if (ctx.location->default_file.empty()) {
		ctx.location->default_file = ctx.index;
	}
	//Logger::file("ctx.index: " + ctx.index);
	//Logger::file("ctx.location->default_file: " + ctx.location->default_file);
	ctx.access_flag = FILE_EXISTS;
	if (!requestedPath.empty() && requestedPath.back() == '/') {
		//Logger::file("apply default file config");
		requestedPath = concatenatePath(requestedPath, ctx.location->default_file);
	}
	requestedPath = approveExtention(ctx, requestedPath);
	//Logger::file("After Requested Path: " + requestedPath);
	if (ctx.type == ERROR)
		return;
	// File existence check
	ctx.access_flag = FILE_EXISTS;
	//checkAccessRights(ctx, rootAndLocationIndex);
	//Logger::file("File existence check completed for: " + requestedPath);

	// Read permission check
	ctx.access_flag = FILE_READ;
	//checkAccessRights(ctx, rootAndLocationIndex);
	//Logger::file("Read permission check completed for: " + requestedPath);

	// Execute permission check
	ctx.access_flag = FILE_EXISTS;
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

bool Server::checkAccessRights(Context &ctx, std::string path)
{
	//Logger::file("[INFO] Checking access rights for: " + path);
	if (access(path.c_str(), ctx.access_flag))
	{
		if (ctx.location->autoindex == "on")
		{
			std::string dirPath = getDirectory(path) + '/';
			//Logger::file("[INFO] Autoindex enabled, checking directory: " + dirPath);
			if (access(dirPath.c_str(), F_OK))
			{
				ctx.error_code = 403;
				ctx.type = ERROR;
				ctx.error_message = "Forbidden";
				//Logger::file("[ERROR] Autoindex forbidden for: " + dirPath);
				return false;
			}
			ctx.doAutoIndex = dirPath;
			ctx.type = STATIC;
			//Logger::file("[INFO] Autoindexing directory: " + dirPath);
			return false;
		}

		switch (errno)
		{
			case ENOENT:
				ctx.error_message = "File does not exist";
				ctx.error_code = 404;
				break;
			case EACCES:
				ctx.error_message = "Permission denied";
				ctx.error_code = 403;
				break;
			case EROFS:
				ctx.error_message = "Read-only file system";
				ctx.error_code = 500;
				break;
			case ENOTDIR:
				ctx.error_message = "A component of the path is not a directory";
				ctx.error_code = 500;
				break;
			default:
				ctx.error_message = "Unknown error: " + std::to_string(errno);
				ctx.error_code = 500;
				break;
		}
		ctx.type = ERROR;
		//Logger::file("[ERROR] Access denied for " + path + " Reason: " + ctx.error_message);
		return false;
	}
	else
	{
		//Logger::file("[INFO] Access granted for: " + path);
		return true;
	}
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
		ctx.type = ERROR;
		ctx.error_code = 500;
		ctx.error_message = "Internal Server Error";
		return;
	}

	// path und index file muss da sein. Pfad aufloesen..
	// autoindex
	// uploadstore

	for (auto& loc : conf->locations)
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
			ctx.location = &loc;
			if (!isMethodAllowed(ctx))
			{
				ctx.type = ERROR;
				ctx.error_code = 405;
				ctx.error_message = "Method (" + ctx.method + ") not allowed";
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

	ctx.type = ERROR;
	ctx.error_code = 500;
	ctx.error_message = "Internal Server Error";
}

