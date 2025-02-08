#include "Server.hpp"

void Server::modEpoll(int epoll_fd, int fd, uint32_t events)
{
	if (fd <= 0)
	{
        Logger::errorLog("WARNING: Attempt to modify epoll for invalid fd: " + std::to_string(fd));
        return;
    }
	struct epoll_event ev;
	ev.events = events;
	ev.data.fd = fd;

	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev) < 0)
	{
		if (errno != EEXIST)
		{
			return;
		}
		if (epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &ev) < 0)
			return;
	}
}

void Server::delFromEpoll(int epfd, int client_fd)
{
    if (epfd <= 0 || client_fd <= 0)
	{
        Logger::errorLog("WARNING: Attempt to delete invalid fd: " + std::to_string(client_fd));
        return;
    }
    if (findServerBlock(configData, client_fd))
	{
        Logger::errorLog("Prevented removal of server socket: " + std::to_string(client_fd));
        return;
    }
    auto ctx_it = globalFDS.context_map.find(client_fd);
    if (ctx_it != globalFDS.context_map.end())
    {
		epoll_ctl(epfd, EPOLL_CTL_DEL, client_fd, NULL);
		globalFDS.clFD_to_svFD_map.erase(client_fd);
		close(client_fd);
		client_fd =-1;
        globalFDS.context_map.erase(client_fd);
    }
    else
		globalFDS.clFD_to_svFD_map.erase(client_fd);

}

bool Server::findServerBlock(const std::vector<ServerBlock> &configs, int fd)
{
	for (const auto &conf : configs)
	{
		if (conf.server_fd == fd)
			return true;
	}
	return false;
}

int Server::setNonBlocking(int fd)
{
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags < 0)
		return -1;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

bool Server::addServerNameToHosts(const std::string &server_name)
{
	const std::string hosts_file = "/etc/hosts";
	std::ifstream infile(hosts_file);
	std::string line;

	// Check if the server_name already exists in /etc/hosts
	while (std::getline(infile, line))
	{
		if (line.find(server_name) != std::string::npos)
			return true;
	}

	// Add server_name to /etc/hosts
	std::ofstream outfile(hosts_file, std::ios::app);
	if (!outfile.is_open())
		throw std::runtime_error("Failed to open /etc/hosts");
	outfile << "127.0.0.1 " << server_name << "\n";
	outfile.close();
	Logger::yellow("Added " + server_name + " to /etc/hosts file");
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
	while (std::getline(infile, line))
	{
		bool shouldRemove = false;
		for (const auto &name : added_server_names)
		{
			if (line.find(name) != std::string::npos)
			{
				Logger::yellow("Remove " + name + " from /etc/host file");
				shouldRemove = true;
				break;
			}
		}
		if (!shouldRemove)
			lines.push_back(line);
	}
	infile.close();

	std::ofstream outfile(hosts_file, std::ios::trunc);
	if (!outfile.is_open())
		throw std::runtime_error("Failed to open /etc/hosts for writing");
	for (const auto &l : lines)
		outfile << l << "\n";
	outfile.close();
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

bool Server::matchLoc(Location& loc, std::string rawPath)
{
    if (rawPath.empty())
        rawPath = "/";

    if (rawPath[0] != '/')
        rawPath = "/" + rawPath;

    std::string normalizedPath;
    bool lastWasSlash = false;

    for (size_t i = 0; i < rawPath.length(); ++i)
	{
        if (rawPath[i] == '/')
		{
            if (!lastWasSlash)
                normalizedPath += '/';
            lastWasSlash = true;
        }
		else
		{
            normalizedPath += rawPath[i];
            lastWasSlash = false;
        }
    }

    if (normalizedPath == loc.path)
        return true;

    if (loc.path.length() <= normalizedPath.length() &&
        normalizedPath.substr(0, loc.path.length()) == loc.path)
	{
        if (loc.path[loc.path.length()-1] != '/')
            return normalizedPath[loc.path.length()] == '/';
        return true;
    }

    return false;
}

std::string Server::concatenatePath(const std::string& root, const std::string& path)
{
	if (root.empty())
		return path;
	if (path.empty())
		return root;

	if (root.back() == '/' && path.front() == '/')
		return root + path.substr(1);
	else if (root.back() != '/' && path.front() != '/')
		return root + '/' + path;
	else
		return root + path;
}

std::string Server::getDirectory(const std::string& path)
{
	size_t lastSlash = path.find_last_of("/\\");
	if (lastSlash != std::string::npos)
		return path.substr(0, lastSlash);
	return "";
}

void Server::checkAndCleanupTimeouts()
{
    auto now = std::chrono::steady_clock::now();

    auto it = globalFDS.context_map.begin();
    while (it != globalFDS.context_map.end())
	{
        Context& ctx = it->second;
        auto duration = std::chrono::duration_cast<std::chrono::seconds>(
            now - ctx.last_activity).count();

        if (duration > Context::TIMEOUT_DURATION.count())
		{
            delFromEpoll(ctx.epoll_fd, ctx.client_fd);
            it = globalFDS.context_map.erase(it);
        }
        else
            ++it;
    }
}

void Server::killTimeoutedCGI(RequestBody &req)
{
	if (req.cgi_pid > 0)
	{
		kill(req.cgi_pid, SIGTERM);
		std::this_thread::sleep_for(std::chrono::microseconds(req.associated_conf->timeout));
		int status;
		pid_t result = waitpid(req.cgi_pid, &status, WNOHANG);
		if (result == 0)
		{
			kill(req.cgi_pid, SIGKILL);
			waitpid(req.cgi_pid, &status, 0);
		}
		req.cgi_pid = -1;
	}
	if (req.cgi_in_fd != -1)
	{
		epoll_ctl(globalFDS.epoll_fd, EPOLL_CTL_DEL, req.cgi_in_fd, NULL);
		close(req.cgi_in_fd);
		globalFDS.clFD_to_svFD_map.erase(req.cgi_in_fd);
		req.cgi_in_fd = -1;
	}
	if (req.cgi_out_fd != -1)
	{
		epoll_ctl(globalFDS.epoll_fd, EPOLL_CTL_DEL, req.cgi_out_fd, NULL);
		close(req.cgi_out_fd);
		globalFDS.clFD_to_svFD_map.erase(req.cgi_out_fd);
		req.cgi_out_fd = -1;
	}
	if (req.pipe_fd != -1)
	{
		epoll_ctl(globalFDS.epoll_fd, EPOLL_CTL_DEL, req.pipe_fd, NULL);
		close(req.pipe_fd);
		req.pipe_fd = -1;
	}
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

bool Server::fileReadable(const std::string& path)
{
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && (st.st_mode & S_IRUSR));
}

bool Server::fileExecutable(const std::string& path)
{
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && (st.st_mode & S_IXUSR));
}

bool Server::dirReadable(const std::string& path)
{
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & S_IRUSR));
}

bool Server::dirWritable(const std::string& path)
{
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & S_IWUSR));
}

bool Server::checkAccessRights(Context &ctx, std::string path)
{
    struct stat path_stat;
    if (stat(path.c_str(), &path_stat) != 0)
        return updateErrorStatus(ctx, 404, "Not found");

    if (S_ISDIR(path_stat.st_mode))
	{
        if (ctx.location_inited && ctx.location.autoindex == "on")
		{
            std::string dirPath = getDirectory(path) + '/';
            if (!dirReadable(dirPath))
                return updateErrorStatus(ctx, 403, "Forbidden");
            ctx.doAutoIndex = dirPath;
            ctx.type = STATIC;
            return true;
        }
		else
            return updateErrorStatus(ctx, 403, "Forbidden");
    }

    if (!fileReadable(path))
        return updateErrorStatus(ctx, 403, "Forbidden");

    if (ctx.type == CGI && !fileExecutable(path))
        return updateErrorStatus(ctx, 500, "CGI script not executable");

    if (ctx.method == "POST")
	{
        std::string uploadDir = getDirectory(path);
        if (!dirWritable(uploadDir))
            return updateErrorStatus(ctx, 403, "Forbidden");
    }

    if (path.length() > 4096)
        return updateErrorStatus(ctx, 414, "URI Too Long");
    return true;
}

bool isMethodAllowed(Context& ctx)
{

	bool isAllowed = false;
	std::string reason;

	if (ctx.method == "GET" && ctx.location.allowGet)
	{
		isAllowed = true;
		reason = "GET method allowed for this location";
	}
	else if (ctx.method == "POST" && ctx.location.allowPost)
	{
		isAllowed = true;
		reason = "POST method allowed for this location";
	}
	else if (ctx.method == "DELETE" && ctx.location.allowDelete)
	{
		isAllowed = true;
		reason = "DELETE method allowed for this location";
	}
	else if (ctx.method == "COOKIE" && ctx.location.allowCookie)
	{
		isAllowed = true;
		reason = "COOKIE method allowed for this location";
	}
	else
		reason = ctx.method + " method not allowed for this location";
	return isAllowed;
}

// Bestimmt Request-Typ aus Konfiguration
bool Server::determineType(Context& ctx, std::vector<ServerBlock> configs)
{
	ServerBlock* conf = nullptr;
	for (auto& config : configs)
	{
		if (config.server_fd == ctx.server_fd)
		{
			conf = &config;
			break;
		}
	}

	if (!conf)
		return updateErrorStatus(ctx, 500, "Internal Server Error");

	for (auto loc : conf->locations)
	{
		if (matchLoc(loc, ctx.path))
		{
			ctx.port = conf->port;
			ctx.name = conf->name;
			ctx.root = conf->root;
			ctx.index = conf->index;
			ctx.errorPages = conf->errorPages;
			ctx.timeout = conf->timeout;
			ctx.location = loc;
			ctx.location_inited = true;
			logContext(ctx);
			Logger::file(ctx.location.methods);
			Logger::file(std::to_string(ctx.location.allowDelete));
			if (!isMethodAllowed(ctx))
				return updateErrorStatus(ctx, 405, "Method (" + ctx.method + ") not allowed");
			if (loc.cgi != "")
				ctx.type = CGI;
			else
				ctx.type = STATIC;
			parseAccessRights(ctx);
			return true;;
		}
	}
	return (updateErrorStatus(ctx, 500, "Internal Server Error"));
}

// Bestimmt Request-Typ aus Konfiguration
void Server::getMaxBodySizeFromConfig(Context& ctx, std::vector<ServerBlock> configs)
{
	ServerBlock* conf = nullptr;

	for (auto& config : configs)
	{

		if (config.server_fd == ctx.server_fd)
		{
			conf = &config;
			break;
		}
	}
	if (!conf)
		return;

	for (auto loc : conf->locations)
	{
		if (matchLoc(loc, ctx.path))
		{
			ctx.client_max_body_size = loc.client_max_body_size;
			if (ctx.client_max_body_size != -1 && ctx.location.client_max_body_size == -1)
				ctx.location.client_max_body_size = ctx.client_max_body_size;
			return;
		}
	}
}

bool detectMultipartFormData(Context &ctx)
{
	if (ctx.headers["Content-Type"] == "Content-Type: multipart/form-data")
	{
		ctx.is_multipart = true;
		return true;
	}
	ctx.is_multipart = false;
	return false;
}

std::string Server::approveExtention(Context& ctx, std::string path_to_check)
{
    std::string approvedIndex;

    size_t dot_pos = path_to_check.find_last_of('.');

    if (dot_pos != std::string::npos)
    {
        std::string extension = path_to_check.substr(dot_pos + 1);
        if (ctx.type != CGI || (extension == ctx.location.cgi_filetype && ctx.type == CGI))
        {
            approvedIndex = path_to_check;
        }
        else
        {
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

bool Server::resetContext(Context& ctx)
{
	Logger::file("resetContext");
	ctx.input_buffer.clear();
	ctx.headers.clear();
	ctx.method.clear();
	ctx.path.clear();
	ctx.version.clear();
	ctx.headers_complete = false;
	ctx.content_length = 0;
	ctx.error_code = 0;
	ctx.req.parsing_phase = RequestBody::PARSING_HEADER;
	ctx.req.current_body_length = 0;
	ctx.req.expected_body_length = 0;
	ctx.req.received_body.clear();
	ctx.req.chunked_state.processing = false;
	ctx.req.is_upload_complete = false;
	ctx.type = RequestType::INITIAL;
	return true;
}
