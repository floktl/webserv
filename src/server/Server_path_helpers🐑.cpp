#include "Server.hpp"

std::string determineContentType(const std::string& path)
{
	std::string	contentType = "text/plain";
	size_t		dot_pos = path.find_last_of('.');

	if (dot_pos != std::string::npos)
	{
		std::string ext = path.substr(dot_pos + 1);
		if (ext == "html" || ext == "htm")
			contentType = "text/html";
		else if (ext == "css")
			contentType = "text/css";
		else if (ext == "js")
			contentType = "application/javascript";
		else if (ext == "jpg" || ext == "jpeg")
			contentType = "image/jpeg";
		else if (ext == "png")
			contentType = "image/png";
		else if (ext == "gif")
			contentType = "image/gif";
		else if (ext == "pdf")
			contentType = "application/pdf";
		else if (ext == "json")
			contentType = "application/json";
	}
	return contentType;
}

// Normalizes A Given Path, Ensuring It Starts with '/' And Removing Trailing Slashes
std::string Server::normalizePath(const std::string& path)
{
	if (path.empty())
		return "/";

	std::string	normalized = (path[0] != '/') ? "/" + path : path;
	std::string	result;
	bool		lastWasSlash = false;

	for (char c : normalized)
	{
		if (c == '/')
		{
			if (!lastWasSlash)
				result += c;
			lastWasSlash = true;
		}
		else
		{
			result += c;
			lastWasSlash = false;
		}
	}

	if (result.length() > 1 && result.back() == '/')
		result.pop_back();

	return result;
}

// Checks if locationSegments match the beginning of requestSegments
bool isPathMatch(const std::vector<std::string>& requestSegments, const std::vector<std::string>& locationSegments)
{
	if (locationSegments.empty())
		return true;

	if (locationSegments.size() > requestSegments.size())
		return false;

	for (size_t i = 0; i < locationSegments.size(); ++i)
	{
		if (locationSegments[i] != requestSegments[i])
			return false;
	}
	return true;
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

std::string Server::subtractLocationPath(const std::string& path, const Location& location)
{
	if (location.root.empty())
		return path;
	size_t pos = path.find(location.path);
	if (pos == std::string::npos)
		return path;
	std::string remainingPath = path.substr(pos + location.path.length());

	if (remainingPath.empty() || remainingPath[0] != '/')
		remainingPath = "/" + remainingPath;
	return remainingPath;
}

bool Server::isPathInUploadStore(Context& ctx, const std::string& path_to_check)
{
    std::string full_upload_store;

    if (ctx.location.upload_store.find(ctx.root) != std::string::npos)
        full_upload_store = ctx.location.upload_store;
    else
        full_upload_store = concatenatePath(ctx.root, ctx.location.upload_store);

    std::string normalized_upload_store = full_upload_store;
    std::string normalized_path = path_to_check;

    if (!normalized_upload_store.empty() && normalized_upload_store.back() == '/')
        normalized_upload_store.pop_back();
    if (!normalized_path.empty() && normalized_path.back() == '/')
        normalized_path.pop_back();

    bool starts_with_upload_store = false;

    if (normalized_path.length() >= normalized_upload_store.length())
	{
        starts_with_upload_store = (normalized_path.substr(0, normalized_upload_store.length()) == normalized_upload_store);
        if (starts_with_upload_store &&
            normalized_path.length() > normalized_upload_store.length() &&
            normalized_path[normalized_upload_store.length()] != '/') {
            starts_with_upload_store = false;
        }
    }

    return starts_with_upload_store;
}
