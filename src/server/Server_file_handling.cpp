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

// Handles DELETE requests by verifying file permissions, preventing deletion of an active upload,
// and sending appropriate responses without killing the process.
bool Server::deleteHandler(Context &ctx)
{
	std::string req_root = retreiveReqRoot(ctx);
	std::string requestedPath = concatenatePath(req_root, ctx.path);

	if (ctx.index.empty() && ctx.method != "DELETE")
		ctx.index = DEFAULT_FILE;
	if (ctx.location.default_file.empty())
		ctx.location.default_file = ctx.index;

	if (ctx.method != "DELETE")
	{
		std::string adjustedPath = ctx.path;
		if (!ctx.useLocRoot)
			adjustedPath = subtractLocationPath(ctx.path, ctx.location);

		requestedPath = concatenatePath(req_root, adjustedPath);
	}

	if (!requestedPath.empty() && requestedPath.back() == '/')
		requestedPath = concatenatePath(requestedPath, ctx.location.default_file);

	if (ctx.location_inited && requestedPath == ctx.location.upload_store && dirWritable(requestedPath))
		return false;

	Logger::magenta(requestedPath);

	// Prevent DELETE of a file that matches DEFAULT_FILE
	if (requestedPath.length() >= std::string(DEFAULT_FILE).length() &&
		requestedPath.substr(requestedPath.length() - std::string(DEFAULT_FILE).length()) == DEFAULT_FILE)
	{
		return updateErrorStatus(ctx, 400, "Bad Request");
	}

	// Check if another context is currently uploading the file
	for (auto it = globalFDS.context_map.begin(); it != globalFDS.context_map.end(); ++it)
	{
		Context &other_ctx = it->second;
		if (other_ctx.multipart_file_path_up_down == requestedPath && other_ctx.client_fd != ctx.client_fd)
		{
			Logger::red("File is currently being uploaded by client_fd " + std::to_string(other_ctx.client_fd) + " - Deletion blocked.");

			// Send a 402 "File in Use" error to the DELETE requestor
			updateErrorStatus(ctx, 402, "File is currently in use and cannot be deleted.");
			return false;
		}
	}

	// Delete the requested file
	std::filesystem::remove(requestedPath);

	// Verify if deletion was successful
	if (std::filesystem::exists(requestedPath))
	{
		return updateErrorStatus(ctx, 500, "Internal Server Error");
	}

	// Redirect the original DELETE request client to a success page
	std::stringstream response;
	response << "HTTP/1.1 303 See Other\r\n"
			 << "Location: /success/delete-confirmation.html\r\n"
			 << "Content-Length: 0\r\n"
			 << "Connection: close\r\n\r\n";

	sendHandler(ctx, response.str());

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