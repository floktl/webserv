#include "Server.hpp"

// Checks if a file is readable
bool Server::fileExists(const std::string& path)
{
	struct stat st;
	return (stat(path.c_str(), &st) == 0);
}

// Checks if a path is a directory
bool Server::isDirectory(const std::string& path)
{
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode));
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


// Checks if a directory is written
bool Server::dirWritable(const std::string& path)
{
	struct stat st;
	return (stat(path.c_str(), &st) == 0 && S_ISDIR(st.st_mode) && (st.st_mode & S_IWUSR));
}

// Verifies Access Permissions for a Given Path Based on the Request Context
bool Server::checkAccessRights(Context &ctx, std::string path)
{
	if (!fileReadable(path) && ctx.method == "GET")
		return updateErrorStatus(ctx, 404, "Not Found");
	if (!fileReadable(path) && ctx.method == "DELETE")
		return updateErrorStatus(ctx, 404, "Not Found");
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

size_t Server::getFileSize(const std::string& path)
{
	struct stat st;
	if (stat(path.c_str(), &st) == 0)
		return static_cast<size_t>(st.st_size);
	return 0;
}
