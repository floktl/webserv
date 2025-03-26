#include "Server.hpp"
#include <filesystem>

// Retrieves the request root from the context, using the location's root if available
// otherwise falls back to the server's root.
std::string Server::retreiveReqRoot(Context &ctx)
{
	ctx.use_loc_root = false;
	std::string req_root = ctx.location.root;
	if (req_root.empty())
	{
		ctx.use_loc_root = true;
		req_root = ctx.root;
	}
	return req_root;
}

// Handles DELETE requests by verifying file permissions, preventing deletion of an active upload,
// and sending appropriate responses without killing the process.
bool Server::deleteHandler(Context &ctx)
{
	std::string req_root = retreiveReqRoot(ctx);
	std::string requestedPath;
		if (ctx.path.rfind(req_root, 0) == 0)
		requestedPath = ctx.path;
	else
		requestedPath = concatenatePath(req_root, ctx.path);

	if (ctx.index.empty() && ctx.method != "DELETE")
		ctx.index = DEFAULT_FILE;
	if (ctx.location.default_file.empty())
		ctx.location.default_file = ctx.index;

	if (ctx.method != "DELETE")
	{
		std::string adjustedPath = ctx.path;
		if (!ctx.use_loc_root)
			adjustedPath = subtractLocationPath(ctx.path, ctx.location);
		requestedPath = concatenatePath(req_root, adjustedPath);
	}
	if (!requestedPath.empty() && requestedPath.back() == '/')
		requestedPath = concatenatePath(requestedPath, ctx.location.default_file);
	if (ctx.location_inited && requestedPath == ctx.location.upload_store && dirWritable(requestedPath))
		return false;
	if (requestedPath.length() >= std::string(DEFAULT_FILE).length() &&
		requestedPath.substr(requestedPath.length() - std::string(DEFAULT_FILE).length()) == DEFAULT_FILE)
		return updateErrorStatus(ctx, 400, "Bad Request");
	for (auto it = globalFDS.context_map.begin(); it != globalFDS.context_map.end(); ++it)
	{
		if (it->second.multipart_file_path_up_down == requestedPath && it->second.client_fd != ctx.client_fd)
			return (updateErrorStatus(ctx, 409, "Conflict"));
	}
	std::filesystem::remove(requestedPath);
	if (std::filesystem::exists(requestedPath))
		return updateErrorStatus(ctx, 500, "Internal Server Error");
	std::stringstream response;
	response << "HTTP/1.1 303 See Other\r\n"
			<< "Location: /\r\n"
			<< "Content-Length: 0\r\n"
			<< "Connection: close\r\n\r\n";

	return (sendHandler(ctx, response.str()));
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

// Splits the given path string into segments based on the '/' delimiter
std::vector<std::string> Server::splitPathLoc(const std::string& path)
{
	std::vector<std::string> segments;
	std::string segment;

	for (size_t i = 0; i < path.length(); ++i)
	{
		if (path[i] == '/')
		{
			if (!segment.empty())
			{
				segments.push_back(segment);
				segment.clear();
			}
		}
		else
			segment += path[i];
	}

	if (!segment.empty())
		segments.push_back(segment);

	return segments;
}

void appendContentAfterFirstBoundary(std::string buf, std::vector<char>& output, size_t bound_Pos, size_t last_pos)
{
	if (last_pos != std::string::npos)
	{
		size_t end_pos = last_pos;

		if (end_pos >= 2 && buf[end_pos - 1] == '\n' && buf[end_pos - 2] == '\r')
			end_pos -= 2;

		if (end_pos > 0)
			output.insert(output.end(), buf.begin(), buf.begin() + end_pos);
	}
	else if (bound_Pos != std::string::npos)
	{
		if (bound_Pos > 0)
		{
			size_t end_pos = bound_Pos;

			if (end_pos >= 2 && buf[end_pos - 1] == '\n' && buf[end_pos - 2] == '\r')
				end_pos -= 2;
			output.insert(output.end(), buf.begin(), buf.begin() + end_pos);
		}

		size_t headers_end = buf.find("\r\n\r\n", bound_Pos);
		if (headers_end != std::string::npos)
		{
			size_t cont_start = headers_end + 4;

			if (cont_start < buf.size())
				output.insert(output.end(), buf.begin() + cont_start, buf.end());
		}
	}
	else
		output.insert(output.end(), buf.begin(), buf.end());
}

bool Server::extractFileContent(const std::string& boundary, const std::string& buf, std::vector<char>& output, Context& ctx)
{
	if (boundary.empty())
		return Logger::errorLog("Empty boundary");

	size_t boundaryPos = buf.find(boundary);
	size_t content_end = buf.find(boundary + "--");

	if (content_end != std::string::npos)
	{
		ctx.req.is_upload_complete = true;
		ctx.final_boundary_found = true;
	}

	if (!ctx.found_first_boundary)
	{
		if (boundaryPos != std::string::npos)
		{
			ctx.found_first_boundary = true;

			size_t headers_end = buf.find("\r\n\r\n", boundaryPos);
			if (headers_end != std::string::npos)
			{
				size_t content_start = headers_end + 4;

				if (content_end != std::string::npos)
				{
					if (content_end > content_start)
					{
						if (content_end >= 2 && buf[content_end - 1] == '\n' && buf[content_end - 2] == '\r')
							content_end -= 2;

						if (content_start < content_end)
							output.insert(output.end(), buf.begin() + content_start, buf.begin() + content_end);
					}
				}
				else if (content_start < buf.size())
					output.insert(output.end(), buf.begin() + content_start, buf.end());
			}
		}
	}
	else
		appendContentAfterFirstBoundary(buf, output, boundaryPos, content_end);
	return true;
}
