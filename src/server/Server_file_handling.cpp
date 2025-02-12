#include "Server.hpp"
#include <filesystem>

// Handles file uploads by extracting filename, validating content boundaries, and saving the file
bool Server::handleStaticUpload(Context& ctx)
{
	std::regex filenameRegex(R"(Content-Disposition:.*filename=\"([^\"]+)\")");
	std::smatch matches;
	std::string filename;

	if (std::regex_search(ctx.req.received_body, matches, filenameRegex) && matches.size() > 1)
		filename = matches[1].str();
	else
		return updateErrorStatus(ctx, 422, "Unprocessable Entity");

	size_t contentStart = ctx.req.received_body.find("\r\n\r\n");
	if (contentStart == std::string::npos)
	{
		Logger::errorLog("[Upload] ERROR: Failed to find content start boundary");
		return updateErrorStatus(ctx, 422, "Unprocessable Entity");
	}
	contentStart += 4;

	size_t contentEnd = ctx.req.received_body.rfind("\r\n--");
	if (contentEnd == std::string::npos)
		return (Logger::errorLog("[Upload] ERROR: Failed to find content end boundary"));

	std::string fileContent = ctx.req.received_body.substr(contentStart, contentEnd - contentStart);
	std::string uploadPath = concatenatePath(ctx.location_path, ctx.location.upload_store);
	if (!dirWritable(uploadPath))
		return updateErrorStatus(ctx, 403, "Forbidden");
	std::string filePath = uploadPath + "/" + filename;

	std::ofstream outputFile(filePath, std::ios::binary);
	if (!outputFile)
		return (Logger::errorLog("[Upload] ERROR: Failed to open output file: " + filePath));
	outputFile.write(fileContent.c_str(), fileContent.size());
	outputFile.close();
	if (outputFile)
		return redirectAction(ctx);
	return false;
}

// Handles DELETE requests by verifying file permissions and removing the requested file
bool Server::deleteHandler(Context &ctx) {
		bool useLocRoot = false;
	std::string req_root = ctx.location.root;
	if (req_root.empty())
	{
		useLocRoot = true;
		req_root = ctx.root;
	}
	std::string requestedPath = concatenatePath(req_root, ctx.path);
	if (ctx.index.empty())
		ctx.index = "index.html";
	if (ctx.location.default_file.empty())
		ctx.location.default_file = ctx.index;

	if (ctx.method != "DELETE")
	{
		std::string adjustedPath = ctx.path;
		if (!useLocRoot) {
			adjustedPath = subtractLocationPath(ctx.path, ctx.location);
		}
		requestedPath = concatenatePath(req_root, adjustedPath);
	}
	if (!requestedPath.empty() && requestedPath.back() == '/')
		requestedPath = concatenatePath(requestedPath, ctx.location.default_file);
	if (ctx.location_inited && requestedPath == ctx.location.upload_store && dirWritable(requestedPath))
		return false;

	std::filesystem::remove(requestedPath);
	if (std::filesystem::exists(requestedPath)) {
		return updateErrorStatus(ctx, 500, "Internal Server Error");
	}
	return true;
}

// Splits a given path into components, removing empty segments
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