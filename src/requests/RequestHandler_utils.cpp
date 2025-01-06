#include "RequestHandler.hpp"

RequestHandler::RequestHandler(Server& _server)
	: server(_server) {}

bool RequestHandler::checkRedirect(RequestState &req, std::stringstream *response)
{
    const Location* loc = findMatchingLocation(req.associated_conf, req.location_path);
    if (!loc || loc->return_code.empty() || loc->return_url.empty())
        return false;

    // Generiere zuerst den kompletten Response-String
    std::stringstream redirect_response;
    redirect_response << "HTTP/1.1 " << loc->return_code << " ";

    if (loc->return_code == "301")
        redirect_response << "Moved Permanently";
    else if (loc->return_code == "302")
        redirect_response << "Found";

    redirect_response << "\r\n"
                     << "Location: " << loc->return_url << "\r\n"
                     << "Content-Length: 0\r\n";

    if (!req.cookie_header.empty())
        redirect_response << "Set-Cookie: " << req.cookie_header << "\r\n";

    redirect_response << "\r\n";

    std::string response_str = redirect_response.str();

    // Überprüfe die Größe vor dem Zuweisen
    if (response_str.length() > req.response_buffer.max_size())
    {
        // Wenn die Redirect-Response zu groß ist, sende eine kürzere Fehlerantwort
        std::stringstream error_response;
        buildErrorResponse(500, "Redirect response too large", &error_response, req);
        req.response_buffer.clear();
        req.response_buffer.assign(error_response.str().begin(), error_response.str().end());
    }
    else
    {
        // Sicheres Zuweisen der Response
        req.response_buffer.clear();
        req.response_buffer.insert(req.response_buffer.begin(),
                                 response_str.begin(),
                                 response_str.end());
    }

    // Kopiere die Response auch in den übergebenen stringstream
    *response = std::move(redirect_response);

    return true;
}

const Location* RequestHandler::findMatchingLocation(const ServerBlock* conf, const std::string& path)
{
	if (!conf) return nullptr;

	const Location* bestMatch = nullptr;
	size_t bestMatchLength = 0;
	bool hasExactMatch = false;

	for (const auto& loc : conf->locations)
	{
		if (path == loc.path)
		{
			bestMatch = &loc;
			hasExactMatch = true;
			break;
		}
	}

	if (hasExactMatch)
		return bestMatch;

	for (const auto& loc : conf->locations)
	{
		bool isPrefix = loc.path.back() == '/';

		if (isPrefix)
		{
			if (path.compare(0, loc.path.length(), loc.path) == 0
				&& loc.path.length() > bestMatchLength)
			{
					bestMatch = &loc;
					bestMatchLength = loc.path.length();
			}
		}
		else
		{
			if (path.length() >= loc.path.length() &&
				path.compare(0, loc.path.length(), loc.path) == 0 &&
				(path.length() == loc.path.length() || path[loc.path.length()] == '/'))
			{
					if (loc.path.length() > bestMatchLength)
					{
						bestMatch = &loc;
						bestMatchLength = loc.path.length();
					}
			}
		}
	}

	return bestMatch;
}

std::string RequestHandler::buildRequestedPath(RequestState &req, const std::string &rawPath)
{
    const Location* loc = findMatchingLocation(req.associated_conf, rawPath);

    std::string usedRoot = extractUsedRoot(loc, req.associated_conf);
    std::string pathAfterLocation = processPathAfterLocation(rawPath, loc);

    std::string fullPath = usedRoot + "/" + pathAfterLocation;

    std::string defaultFilePath = handleDefaultFile(fullPath, loc, req);
    if (!defaultFilePath.empty())
        return defaultFilePath;

    std::string directoryPath = handleDirectoryRequest(fullPath, loc, req.associated_conf, req);
    if (!directoryPath.empty())
        return directoryPath;

    req.is_directory = false;
    if (!fullPath.empty() && fullPath.back() == '/')
        fullPath.pop_back();
    return fullPath;
}

std::string RequestHandler::extractUsedRoot(const Location* loc, const ServerBlock* conf)
{
    if (loc && !loc->root.empty())
        return loc->root.back() == '/' ? loc->root.substr(0, loc->root.size() - 1) : loc->root;
    return conf->root.back() == '/' ? conf->root.substr(0, conf->root.size() - 1) : conf->root;
}

std::string RequestHandler::processPathAfterLocation(const std::string& rawPath, const Location* loc)
{
    if (!loc) return rawPath;

    std::string pathAfterLocation = rawPath;
    std::string locPath = loc->path;

    if (locPath.size() > 1 && locPath.back() == '/')
        locPath.pop_back();

    if (pathAfterLocation.rfind(locPath, 0) == 0)
        pathAfterLocation.erase(0, locPath.size());

    if (!pathAfterLocation.empty() && pathAfterLocation.front() == '/')
        pathAfterLocation.erase(0, 1);

    return pathAfterLocation;
}

std::string RequestHandler::handleDefaultFile(const std::string& fullPath, const Location* loc, RequestState& req)
{
    if (!loc || loc->default_file.empty())
        return "";

    bool pathPointsToDirectory = false;
    struct stat pathStat;
    if (stat(fullPath.c_str(), &pathStat) == 0 && S_ISDIR(pathStat.st_mode))
        pathPointsToDirectory = true;

    if (pathPointsToDirectory || fullPath.empty() || fullPath.back() == '/')
	{
        std::string defaultPath = fullPath;
        if (!pathPointsToDirectory && !defaultPath.empty() && defaultPath.back() != '/')
            defaultPath += "/";
        defaultPath += loc->default_file;

        struct stat defaultStat;
        if (stat(defaultPath.c_str(), &defaultStat) == 0 && S_ISREG(defaultStat.st_mode))
		{
            req.is_directory = false;
            return defaultPath;
        }
    }
    return "";
}

std::string RequestHandler::handleDirectoryRequest(const std::string& fullPath, const Location* loc, const ServerBlock* conf, RequestState& req) {
    bool isDirectoryRequest = fullPath.empty() || fullPath.back() == '/' || (loc && loc->doAutoindex);

    if (!isDirectoryRequest)
        return "";

    std::string usedIndex = conf->index;
    std::string fullDirPath = fullPath;
    if (!fullDirPath.empty() && fullDirPath.back() == '/')
        fullDirPath.pop_back();

    struct stat dirStat;
    if (stat(fullDirPath.c_str(), &dirStat) == 0 && S_ISDIR(dirStat.st_mode))
	{
        if (!usedIndex.empty())
		{
            std::string indexPath = fullDirPath + "/" + usedIndex;

            struct stat indexStat;
            if (stat(indexPath.c_str(), &indexStat) == 0 && S_ISREG(indexStat.st_mode))
			{
                req.is_directory = false;
                return indexPath;
            }
        }

        if (loc && loc->doAutoindex)
		{
            req.is_directory = true;
            return fullDirPath;
        }
    }
    return "";
}

std::string RequestHandler::getMethod(const std::vector<char>& request_buffer)
{
	if (request_buffer.empty())
		return "";

	std::string request(request_buffer.begin(), request_buffer.end());
	std::istringstream iss(request);
	std::string method;

	iss >> method;

	return method;
}
