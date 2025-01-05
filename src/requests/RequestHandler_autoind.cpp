#include "RequestHandler.hpp"

bool RequestHandler::checkDirectoryPermissions(const RequestState& req, std::stringstream* response)
{
    if (access(req.requested_path.c_str(), R_OK) != 0)
	{
        buildErrorResponse(403, "Permission denied", response, const_cast<RequestState&>(req));
        return false;
    }
    return true;
}

std::vector<RequestHandler::DirEntry> RequestHandler::getDirectoryEntries(RequestState& req,
			std::stringstream* response)
{
    DIR* dir = opendir(req.requested_path.c_str());
    if (!dir)
	{
        buildErrorResponse(404, "Directory not found", response, req);
        return {};
    }

    std::vector<DirEntry> entries;
    struct dirent* dir_entry;
    while ((dir_entry = readdir(dir)) != NULL)
        processDirectoryEntry(req, dir_entry, entries);
    closedir(dir);
    return entries;
}

void RequestHandler::processDirectoryEntry(const RequestState& req, struct dirent* dir_entry,
			std::vector<DirEntry>& entries)
{
    std::string name = dir_entry->d_name;
    if (name == "." || name == "..")
	{
        if (req.location_path != "/" && name == "..")
            entries.push_back({"..", true, 0, 0});
        return;
    }

    std::string fullPath = req.requested_path;
    if (fullPath.back() != '/')
        fullPath += '/';
    fullPath += name;

    struct stat statbuf;
    if (stat(fullPath.c_str(), &statbuf) != 0)
        return;

    entries.push_back({
        name,
        S_ISDIR(statbuf.st_mode),
        statbuf.st_mtime,
        statbuf.st_size
    });
}

void RequestHandler::sortDirectoryEntries(std::vector<DirEntry>& entries)
{
    std::sort(entries.begin(), entries.end(),
        [](const DirEntry& a, const DirEntry& b) -> bool {
            if (a.name == "..") return true;
            if (b.name == "..") return false;
            if (a.isDir != b.isDir) return a.isDir > b.isDir;
            return strcasecmp(a.name.c_str(), b.name.c_str()) < 0;
        });
}

void RequestHandler::generateAutoindexHtml(std::stringstream* response, const RequestState& req,
			const std::vector<DirEntry>& entries)
{
    *response << "HTTP/1.1 200 OK\r\n";
    *response << "Content-Type: text/html; charset=UTF-8\r\n\r\n";

    *response << "<!DOCTYPE html>\n<html>\n<head>\n"
			<< "    <meta charset=\"UTF-8\">\n"
			<< "    <title>Index of " << req.location_path << "</title>\n"
			<< "    <style>/* Add CSS styles here */</style>\n"
			<< "</head>\n<body>\n"
			<< "    <h1>Index of " << req.location_path << "</h1>\n"
			<< "    <div class=\"listing\">\n";

    for (const auto& entry : entries)
        generateEntryHtml(response, entry);

    *response << "    </div>\n</body>\n</html>\n";
}

void RequestHandler::generateEntryHtml(std::stringstream* response, const DirEntry& entry)
{
    char timeStr[50] = "-";
    if (entry.name != "..")
	{
        struct tm* tm = localtime(&entry.mtime);
        strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M", tm);
    }

    std::string sizeStr = (entry.isDir || entry.name == "..") ? "-" : formatFileSize(entry.size);

    *response << "        <div class=\"entry\">\n"
			<< "            <div class=\"name\">"
			<< "<a href=\"" << (entry.name == ".." ? "../" : entry.name + (entry.isDir ? "/" : "")) << "\">"
			<< entry.name << (entry.isDir ? "/" : "") << "</a></div>\n"
			<< "            <div class=\"modified\">" << timeStr << "</div>\n"
			<< "            <div class=\"size\">" << sizeStr << "</div>\n"
			<< "        </div>\n";
}

std::string RequestHandler::formatFileSize(off_t size)
{
    if (size < 1024)
        return std::to_string(size) + "B";
    else if (size < 1024 * 1024)
        return std::to_string(size / 1024) + "K";
    else if (size < 1024 * 1024 * 1024)
        return std::to_string(size / (1024 * 1024)) + "M";
    else
        return std::to_string(size / (1024 * 1024 * 1024)) + "G";
}
