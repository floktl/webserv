#include "Server.hpp"

// Adds a server name entry to the /etc/hosts file
bool Server::addServerNameToHosts(const std::string &server_name)
{
	const std::string hosts_file = "/etc/hosts";
	std::ifstream infile(hosts_file);
	std::string line;

	while (std::getline(infile, line))
	{
		if (line.find(server_name) != std::string::npos)
			return true;
	}

	std::ofstream outfile(hosts_file, std::ios::app);
	if (!outfile.is_open())
		throw std::runtime_error("Failed to open /etc/hosts");
	outfile << "127.0.0.1 " << server_name << "\n";
	outfile.close();
	Logger::yellow("Added " + server_name + " to /etc/hosts file");
	added_server_names.push_back(server_name);
	return true;
}

// Removes previously added server names from the /etc/hosts file
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

// Normalizes a given path, ensuring it starts with '/' and removing trailing slashes
std::string Server::normalizePath(const std::string& path) {
	if (path.empty()) {
		return "/";
	}

	std::string normalized = (path[0] != '/') ? "/" + path : path;
	std::string result;
	bool lastWasSlash = false;

	for (char c : normalized) {
		if (c == '/') {
			if (!lastWasSlash) {
				result += c;
			}
			lastWasSlash = true;
		} else {
			result += c;
			lastWasSlash = false;
		}
	}

	if (result.length() > 1 && result.back() == '/') {
		result.pop_back();
	}

	return result;
}

bool isPathMatch(const std::vector<std::string>& requestSegments, const std::vector<std::string>& locationSegments) {
	if (locationSegments.empty()) {
		return true;
	}

	if (locationSegments.size() > requestSegments.size()) {
		return false;
	}

	for (size_t i = 0; i < locationSegments.size(); ++i) {
		if (locationSegments[i] != requestSegments[i]) {
			return false;
		}
	}

	return true;
}

bool Server::matchLoc(const std::vector<Location>& locations, const std::string& rawPath, Location& bestMatch) {
	std::string normalizedPath = normalizePath(rawPath);

	std::vector<std::string> requestSegments = splitPathLoc(normalizedPath);

	size_t bestMatchLength = 0;
	bool foundMatch = false;
	const Location* rootLocation = nullptr;

	for (const Location& loc : locations) {
		std::vector<std::string> locationSegments = splitPathLoc(loc.path);

		if (loc.path == "/" || loc.path.empty()) {
			rootLocation = &loc;
		}

		if (isPathMatch(requestSegments, locationSegments)) {
			if (locationSegments.size() > bestMatchLength) {
				bestMatchLength = locationSegments.size();
				bestMatch = loc;
				foundMatch = true;
			}
		}
	}

	if (!foundMatch && rootLocation != nullptr) {
		bestMatch = *rootLocation;
		foundMatch = true;
	}

	if (!foundMatch) {
		Logger::errorLog("No matching location found, including root location!");
	}

	return foundMatch;
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

std::string Server::subtractLocationPath(const std::string& path, const Location& location) {
	if (location.root.empty()) {
		return path;
	}
	size_t pos = path.find(location.path);
	if (pos == std::string::npos) {
		return path;
	}
	std::string remainingPath = path.substr(pos + location.path.length());
	if (remainingPath.empty() || remainingPath[0] != '/') {
		remainingPath = "/" + remainingPath;
	}
	return remainingPath;
}

// Extracts the directory part of a given file path
std::string Server::getDirectory(const std::string& path)
{
	size_t lastSlash = path.find_last_of("/\\");
	if (lastSlash != std::string::npos)
		return path.substr(0, lastSlash);
	return "";
}

// Converts a request type enum to a string representation
std::string Server::requestTypeToString(RequestType type)
{
	switch(type)
	{
		case RequestType::INITIAL: return "INITIAL";
		case RequestType::STATIC: return "STATIC";
		case RequestType::CGI: return "CGI";
		case RequestType::REDIRECT: return "REDIRECT";
		case RequestType::ERROR: return "ERROR";
		default: return "UNKNOWN";
	}
}

// Checks if a file is readable
bool Server::fileReadable(const std::string& path)
{
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && (st.st_mode & S_IRUSR));
}

// Checks if a file is executable
bool Server::fileExecutable(const std::string& path)
{
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && (st.st_mode & S_IXUSR));
}

// Checks if a directory is readable
bool Server::dirReadable(const std::string& path)
{
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & S_IRUSR));
}

// Checks if a directory is writable
bool Server::dirWritable(const std::string& path)
{
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & S_IWUSR));
}

// Verifies access permissions for a given path based on the request context
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

// Determines whether the requested HTTP method is allowed in the location block
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
	else
		reason = ctx.method + " method not allowed for this location";
	return isAllowed;
}



// Approves a file extension based on CGI configuration or static file handling rules
std::string Server::approveExtention(Context& ctx, std::string path_to_check)
{
	size_t dot_pos = path_to_check.find_last_of('.');
	bool starts_with_upload_store = false;
	// bool ends_with_std_file = false;

	Logger::blue(path_to_check);
	Logger::blue(ctx.location.upload_store);

	if (path_to_check.length() >= ctx.location.upload_store.length()) {
		starts_with_upload_store = path_to_check.substr(0, ctx.location.upload_store.length()) == ctx.location.upload_store;
	}
	// if (path_to_check.length() >= std::string(DEFAULT_FILE).length()) {
	// 	ends_with_std_file = path_to_check.substr(path_to_check.length() - std::string(DEFAULT_FILE).length()) == DEFAULT_FILE;
	// }
	Logger::yellow("start with upload store: " + std::to_string(starts_with_upload_store));
	//Logger::yellow("ends with std file: " + std::to_string(ends_with_std_file));

	if (dot_pos != std::string::npos)
	{
		std::string extension = path_to_check.substr(dot_pos + 1);
		Logger::yellow("extension: " + extension);
		Logger::yellow("cgi_filetype: " + ctx.location.cgi_filetype);
		if (("." + extension) == ctx.location.cgi_filetype && ctx.type == CGI)
			return path_to_check;
		Logger::yellow(" -- no cgi");
		if (ctx.method == "GET" && ctx.location.return_url.empty() && extension == "html")
			return path_to_check;
		Logger::yellow(" -- no simple get");
		if (!ctx.location.return_url.empty() && ctx.method == "GET")
		{
			Logger::yellow(" -- evaluating redirect");
			auto it = std::find(ctx.blocks_location_paths.begin(),
							ctx.blocks_location_paths.end(),
							ctx.location.return_url);
			if (it != ctx.blocks_location_paths.end()) {
			Logger::yellow(" -- set redirect");
				ctx.type = REDIRECT;
			}
			return path_to_check;
		}
		if (starts_with_upload_store && ctx.method == "GET" && ("." + extension) != ctx.location.cgi_filetype)
		{
			Logger::yellow(" -- set download");
			ctx.is_download = true;
			return path_to_check;
		}
		if (starts_with_upload_store && ctx.method == "DELETE")
		{
			Logger::yellow(" -- set post bad req");
			updateErrorStatus(ctx, 400, "Bad Request");
			return "";
		}
		// if (ends_with_std_file && ctx.method == "POST")
		// {
		// 	Logger::yellow(" -- set post bad req");
		// 	updateErrorStatus(ctx, 400, "Bad Request");
		// 	return "";
		// }
		if (starts_with_upload_store && ctx.method == "POST")
		{
			Logger::yellow(" -- set post");
			return path_to_check;
		}
		Logger::yellow(" -- approveExtention");
		updateErrorStatus(ctx, 404, "Not found");
		return "";
	}
	if (ctx.method == "GET" && starts_with_upload_store)
	{
		Logger::yellow(" -- set download no ext");
		ctx.is_download = true;
	}
	return path_to_check;
}

// Resets the context, clearing request data and restoring initial values
bool Server::resetContext(Context& ctx)
{
	ctx.cookies.clear();
	ctx.setCookies.clear();
	ctx.read_buffer.clear();
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

std::vector<std::string> Server::getBlocksLocsPath(const std::vector<Location>& locations) {
	std::vector<std::string> locPaths;
	for (const Location& loc : locations) {
		locPaths.push_back(loc.path);
	}
	return (locPaths);
}

// Determines the request type based on the server configuration and updates the context
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
		return updateErrorStatus(ctx, 500, "Internal Server Error type");
	Location bestMatch;
	if (matchLoc(conf->locations, ctx.path, bestMatch)) {
		ctx.port = conf->port;
		ctx.name = conf->name;
		ctx.root = conf->root;
		ctx.index = conf->index;
		ctx.errorPages = conf->errorPages;
		ctx.timeout = conf->timeout;
		ctx.blocks_location_paths = getBlocksLocsPath(conf->locations);
		ctx.location = bestMatch;
		ctx.location_inited = true;
		if (!isMethodAllowed(ctx))
			return updateErrorStatus(ctx, 405, "Method (" + ctx.method + ") not allowed");
		if (bestMatch.cgi != "")
			ctx.type = CGI;
		else
			ctx.type = STATIC;
		parseAccessRights(ctx);
		return true;
	}
	return (updateErrorStatus(ctx, 500, "Internal Server Error"));
}

// Retrieves the maximum allowed body size for a request from the server configuration
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
		Location bestMatch;
		if (matchLoc(conf->locations, ctx.path, bestMatch))
		{
			ctx.client_max_body_size = loc.client_max_body_size;
			if (ctx.client_max_body_size != -1 && bestMatch.client_max_body_size == -1)
				ctx.location.client_max_body_size = ctx.client_max_body_size;
			return;
		}
	}
}
