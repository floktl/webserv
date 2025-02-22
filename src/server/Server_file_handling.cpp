#include "Server.hpp"
#include <filesystem>

std::string Server::retreiveReqRoot(Context &ctx)
{
	ctx.useLocRoot = false;
	std::string req_root = ctx.location.root;
	if (req_root.empty())
	{
		ctx.useLocRoot = true;
		req_root = ctx.root;
	}
	return req_root;
}

// Handles Delete Requests by Verifying File Permissions and Removing The Requested File
bool Server::deleteHandler(Context &ctx) {
	std::string req_root = retreiveReqRoot(ctx);
	std::string requestedPath = concatenatePath(req_root, ctx.path);
	if (ctx.index.empty() && ctx.method != "DELETE")
		ctx.index = DEFAULT_FILE;
	if (ctx.location.default_file.empty())
		ctx.location.default_file = ctx.index;

	if (ctx.method != "DELETE")
	{
		std::string adjustedPath = ctx.path;
		if (!ctx.useLocRoot) {
			adjustedPath = subtractLocationPath(ctx.path, ctx.location);
		}
		requestedPath = concatenatePath(req_root, adjustedPath);
	}
	if (!requestedPath.empty() && requestedPath.back() == '/')
		requestedPath = concatenatePath(requestedPath, ctx.location.default_file);
	if (ctx.location_inited && requestedPath == ctx.location.upload_store && dirWritable(requestedPath))
		return false;
	Logger::magenta(requestedPath);
	if (requestedPath.length() >= std::string(DEFAULT_FILE).length() &&
		requestedPath.substr(requestedPath.length() - std::string(DEFAULT_FILE).length()) == DEFAULT_FILE) {
		return updateErrorStatus(ctx, 400, "Bad Request");
	}
	std::filesystem::remove(requestedPath);
	if (std::filesystem::exists(requestedPath)) {
		return updateErrorStatus(ctx, 500, "Internal Server Error");
	}
	return true;
}

// Splits a given path into components, removing empty segment
std::vector<std::string> splitPath(const std::string& path)
{
	std::vector<std::string> components;
	std::stringstream ss(path);
	std::string item;
	while (std::getline(ss, item, '/'))
	{
		if (!item.empty())
			components.push_back(item);
	}
	return components;
}

std::vector<std::string> Server::splitPathLoc(const std::string& path) {
	std::vector<std::string> segments;
	std::string segment;

	for (size_t i = 0; i < path.length(); ++i) {
		if (path[i] == '/') {
			if (!segment.empty()) {
				segments.push_back(segment);
				segment.clear();
			}
		} else {
			segment += path[i];
		}
	}

	if (!segment.empty()) {
		segments.push_back(segment);
	}

	return segments;
}