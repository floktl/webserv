#include "Server.hpp"

// Adds a server name Entry to the /etc /hosts file
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

// Removes Previously Added Server Names from the /etc /hosts file
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

// Normalizes A Given Path, Ensuring It Starts with '/' And Removing Trailing Slashes
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

// Extracts the Directory Part of a Given File Path
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

// Checks if a directory is written
bool Server::dirWritable(const std::string& path)
{
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & S_IWUSR));
}

// Verifies Access Permissions for a Given Path Based on the Request Context
bool Server::checkAccessRights(Context &ctx, std::string path)
{
	struct stat path_stat;
	if (stat(path.c_str(), &path_stat) != 0 && ctx.method == "GET")
		return updateErrorStatus(ctx, 404, "Not found in 1. checkaccessright()");
	if (!fileReadable(path) && ctx.method == "DELETE")
		return updateErrorStatus(ctx, 404, "Not found in 2. checkaccessright()");
	if (!fileReadable(path) && ctx.method != "POST")
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

// Determines Whether the Requested Http Method is allowed in the location block
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


std::string Server::approveExtention(Context& ctx, std::string path_to_check) {
	size_t dot_pos = path_to_check.find_last_of('.');
	bool starts_with_upload_store = false;

	if (path_to_check.length() >= ctx.location.upload_store.length()) {
		starts_with_upload_store = path_to_check.substr(0, ctx.location.upload_store.length()) == ctx.location.upload_store;
	}

	if (!ctx.location.return_url.empty() && ctx.method == "GET") {
		if (std::find(ctx.blocks_location_paths.begin(), ctx.blocks_location_paths.end(),
			ctx.location.return_url) != ctx.blocks_location_paths.end()) {
			updateErrorStatus(ctx, 508, "Infinite redirect loop detected: " + ctx.location.return_url);
			return "";
		}
		ctx.blocks_location_paths.push_back(ctx.location.return_url);
		ctx.type = REDIRECT;
		return path_to_check;
	}

	if (ctx.method == "GET" && starts_with_upload_store) {
		Logger::yellow(" -- Processing download request: " + path_to_check);
		ctx.is_download = true;

		if (!fileReadable(path_to_check)) {
			updateErrorStatus(ctx, 403, "Forbidden");
			return "";
		}

		if (ctx.multipart_fd_up_down >= 0) {
			close(ctx.multipart_fd_up_down);
			ctx.multipart_fd_up_down = -1;
		}

		ctx.multipart_fd_up_down = open(path_to_check.c_str(), O_RDONLY);
		if (ctx.multipart_fd_up_down < 0) {
			Logger::errorLog("Failed to open file for download: " + path_to_check);
			updateErrorStatus(ctx, 500, "Internal Server Error");
			return "";
		}

		ctx.multipart_file_path_up_down = path_to_check;

		ctx.type = STATIC;
		return path_to_check;
	}

	if (dot_pos != std::string::npos) {
		std::string extension = path_to_check.substr(dot_pos + 1);

		if (("." + extension) == ctx.location.cgi_filetype && ctx.type == CGI)
			return path_to_check;

		if (ctx.method == "GET" && ctx.location.return_url.empty() && extension == "html")
			return path_to_check;

		if (starts_with_upload_store && ctx.method == "DELETE") {
			updateErrorStatus(ctx, 400, "Bad Request");
			return "";
		}

		if (starts_with_upload_store && ctx.method == "POST") {
			return path_to_check;
		}

		updateErrorStatus(ctx, 404, "Not found");
		return "";
	}

	return path_to_check;
}

// Resets the Context, Clearing Request Data and Restoring Initial Values
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

// Determines the Request Type Based on the Server Configuration and Updates The Context
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

// Retrieves the Maximum Allowed Body Size for A Request From The Server Configuration
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
