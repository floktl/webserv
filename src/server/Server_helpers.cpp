#include "Server.hpp"

bool Server::matchLoc(const std::vector<Location>& locations,
		const std::string& rawPath, Location& bestMatch)
{
	std::string					normalizedPath = normalizePath(rawPath);
	std::vector<std::string>	requestSegments = splitPathLoc(normalizedPath);
	size_t						bestMatchLength = 0;
	bool						foundMatch = false;
	const Location*				rootLocation = nullptr;

	for (const Location& loc : locations)
	{
		std::vector<std::string> locationSegments = splitPathLoc(loc.path);

		if (loc.path == "/" || loc.path.empty())
			rootLocation = &loc;
		if (isPathMatch(requestSegments, locationSegments))
		{
			if (locationSegments.size() > bestMatchLength)
			{
				bestMatchLength = locationSegments.size();
				bestMatch = loc;
				foundMatch = true;
			}
		}
	}

	if (!foundMatch && rootLocation != nullptr)
	{
		bestMatch = *rootLocation;
		foundMatch = true;
	}

	return foundMatch;
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

std::string Server::approveExtention(Context& ctx, std::string path_to_check)
{
	size_t dot_pos = path_to_check.find_last_of('.');
	bool starts_with_upload_store = isPathInUploadStore(ctx, path_to_check);
	if (dot_pos == std::string::npos && isDirectory(path_to_check) && path_to_check.back() != '/' && ctx.location.autoindex != "on")
	{
		path_to_check = path_to_check + "/";
	}
	if (!path_to_check.empty() && path_to_check.back() == '/'&& ctx.location.autoindex != "on")
	{
		path_to_check = concatenatePath(path_to_check, ctx.location.default_file);
		starts_with_upload_store = false;
		return path_to_check;
	}
	if (!ctx.location.return_url.empty() && ctx.method == "GET") {
		if (std::find(ctx.blocks_location_paths.begin(), ctx.blocks_location_paths.end(),
			ctx.location.return_url) != ctx.blocks_location_paths.end()) {
			updateErrorStatus(ctx, 508, "Loop Detected");
			return "";
		}
		ctx.blocks_location_paths.push_back(ctx.location.return_url);
		ctx.type = REDIRECT;
		return path_to_check;
	}

	std::string extension = path_to_check.substr(dot_pos + 1);
	if (ctx.method == "GET" && starts_with_upload_store && ("." + extension != ctx.location.cgi_filetype && ctx.type == CGI)) {
		ctx.type = STATIC;
		ctx.is_download = true;
		if (!fileExists(path_to_check)) {
			updateErrorStatus(ctx, 404, "Not Found");
			return "";
		}
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
			updateErrorStatus(ctx, 500, "Internal Server Error");
			return "";
		}
		if (ctx.multipart_fd_up_down > 0) {
			if (setNonBlocking(ctx.multipart_fd_up_down) < 0) {
				updateErrorStatus(ctx, 500, "Internal Server Error");
			}
		}

		ctx.multipart_file_path_up_down = path_to_check;
		ctx.req.expected_body_length = getFileSize(ctx.multipart_file_path_up_down);
		ctx.type = STATIC;
		return path_to_check;
	}

	if (("." + extension) == ctx.location.cgi_filetype && ctx.type == CGI)
	{
		ctx.path = path_to_check;
		ctx.requested_path = path_to_check;
		return path_to_check;
	}

	if (ctx.method == "GET" && ctx.location.return_url.empty())
		return path_to_check;

	if (starts_with_upload_store && ctx.method == "DELETE") {
		updateErrorStatus(ctx, 400, "Bad Request");
		return "";
	}

	if (starts_with_upload_store && ctx.method == "POST") {
		return path_to_check;
	}
	updateErrorStatus(ctx, 404, "Not Found");
	return "";

	return path_to_check;
}

// Resets the Context, Clearing Request Data and Restoring Initial Values
bool Server::resetContext(Context& ctx)
{
	ctx.cookies.clear();
	ctx.set_cookies.clear();
	ctx.read_buffer.clear();
	ctx.headers.clear();
	ctx.method.clear();
	ctx.path.clear();
	ctx.version.clear();
	ctx.headers_complete = false;
	ctx.error_code = 0;
	ctx.req.current_body_length = 0;
	ctx.req.expected_body_length = 0;
	ctx.req.is_upload_complete = false;
	ctx.type = RequestType::INITIAL;
	ctx.ready_for_ping_pong = false;
	//ctx.cgi_run_to_timeout = false;
	return true;
}

// Extracts and returns the paths from a vector of Location objects.
std::vector<std::string> Server::getBlocksLocsPath(const std::vector<Location>& locations)
{
	std::vector<std::string> locPaths;

	for (const Location& loc : locations)
		locPaths.push_back(loc.path);
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
		return updateErrorStatus(ctx, 500, "Internal Server Error");
	Location bestMatch;
	if (matchLoc(conf->locations, ctx.path, bestMatch))
	{
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
			return updateErrorStatus(ctx, 405, "Method Not Allowed");
		if (bestMatch.cgi != "")
			ctx.type = CGI;
		else
			ctx.type = STATIC;
		parseAccessRights(ctx);
		if (ctx.error_code)
			return false;
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
