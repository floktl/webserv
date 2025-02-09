#include "Server.hpp"

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
bool Server::deleteHandler(Context &ctx)
{

	std::string filename = mergePathsToFilename(ctx.path, ctx.location.upload_store);

	if (filename.empty())
		return updateErrorStatus(ctx, 404, "Not found");
	filename = concatenatePath(ctx.location.upload_store, filename);

	if (dirReadable(ctx.location.upload_store) && dirWritable(ctx.location.upload_store))
	{
		if (geteuid() == 0)
			Logger::blue("[WARNING] Running as root - access() will not reflect real permissions!");
		else if (access(filename.c_str(), W_OK) != 0)
			return updateErrorStatus(ctx, 403, "Forbidden");
	}
	if (unlink(filename.c_str()) != 0)
		return updateErrorStatus(ctx, 500, "Internal Server Error");
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

// Merges request and base paths, extracting the relative filename for further processing
std::string mergePathsToFilename(const std::string& requestPath, const std::string& basePath)
{
	auto requestComponents = splitPath(requestPath);
	auto baseComponents = splitPath(basePath);

	// Find common segment (e.g., "upload")
	size_t matchIndex = 0;
	bool found = false;
	for (size_t i = 0; i < requestComponents.size(); i++)
	{
		for (size_t j = 0; j < baseComponents.size(); j++)
		{
			if (requestComponents[i] == baseComponents[j])
			{
				matchIndex = i;
				found = true;
				break;
			}
		}
		if (found)
			break;
	}
	if (!found)
		return "";  // No common path found
	std::string result;
	for (size_t i = matchIndex + 1; i < requestComponents.size(); i++)
	{
		result += requestComponents[i];
		if (i < requestComponents.size() - 1)
			result += "/";
	}
	return result;
}
