#include "./Sanitizer.hpp"

Sanitizer::Sanitizer() {}
Sanitizer::~Sanitizer() {}

bool Sanitizer::isValidPath(std::string& path, const std::string& context, const std::string& pwd) {
	if (path.empty()) {
		Logger::error("[" + context + "] Path is empty.");
		return false;
	}

// If the path does not start with '/', Prepend the Current Working Directory (PWD)
	std::string normalizedPath = path;
	if (normalizedPath[0] != '/') {
		normalizedPath = pwd + "/" + normalizedPath;
	}

// Validate the Path by Splitting and Checking for Directory Traversal Attempts
	std::stringstream ss(normalizedPath);
	std::string segment;
	std::vector<std::string> segments;

	while (std::getline(ss, segment, '/')) {
		if (segment == "..") {
			Logger::error("[" + context + "] Directory traversal attempt detected ('..') in path: " + normalizedPath);
			return false;
		}
		if (segment == "." || segment.empty()) continue;
		segments.push_back(segment);
	}

// Rebuild the Cleaned Absolute Path
	normalizedPath = "/";
	for (const auto& seg : segments) {
		normalizedPath += seg + "/";
	}
	if (!segments.empty()) {
		normalizedPath.pop_back();
	}

// Check for invalid characters in the Normalized Path
	for (char c : normalizedPath)
	{
		if (!std::isalnum(static_cast<unsigned char>(c)) && c != '/' && c != '_' && c != '-' && c != '.')
			return (Logger::error("[" + context + "] Invalid character '" + std::string(1, c) + "' found in path: " + normalizedPath));
	}

// For root or error page contexts, verify that the path exists and is of the correct type
	if (context == "Root" || context == "Error page")
	{
		struct stat path_stat;
		if (stat(normalizedPath.c_str(), &path_stat) == -1)
			return Logger::error("[" + context + "] Path does not exist: " + normalizedPath);

		if (context == "Root" && !S_ISDIR(path_stat.st_mode))
			return Logger::error("[" + context + "] Root path is not a directory: " + normalizedPath);
		if (context == "Error page" && !S_ISREG(path_stat.st_mode))
			return Logger::error("[" + context + "] Error page path is not a file: " + normalizedPath);
	}

// If all checks pass, update the original path to the normalized absolute path
	path = normalizedPath;
	return true;
}


long Sanitizer::parseSize(const std::string& sizeStr, const std::string& defaultUnit)
{
	std::string numberPart;
	std::string unitPart;
	size_t i = 0;

	while (i < sizeStr.size() && isdigit(sizeStr[i]))
		numberPart += sizeStr[i++];

	while (i < sizeStr.size() && isalpha(sizeStr[i]))
		unitPart += sizeStr[i++];

	if (numberPart.empty())
		return -1;

	long size;
	try
	{
		size = std::stol(numberPart);
	}
	catch (...)
	{
		return -1;
	}

	if (size <= 0) return -1;

	if (unitPart.empty())
	{
		if (defaultUnit == "K" || defaultUnit == "KB") return size * 1024L;
		if (defaultUnit == "M" || defaultUnit == "MB") return size * 1024L * 1024L;
		if (defaultUnit == "G" || defaultUnit == "GB") return size * 1024L * 1024L * 1024L;
		return size;
	}

	std::string unit = unitPart;
	std::transform(unit.begin(), unit.end(), unit.begin(), ::toupper);

	if (unit == "K" || unit == "KB") return size * 1024L;
	if (unit == "M" || unit == "MB") return size * 1024L * 1024L;
	if (unit == "G" || unit == "GB") return size * 1024L * 1024L * 1024L;

	return -1;
}

bool Sanitizer::isValidFilename(const std::string& filename, bool allowPath) {
	if (filename.empty()) return false;

	if (!allowPath && (filename.find('/') != std::string::npos || filename.find('\\') != std::string::npos))
		return false;

	for (char c : filename)
	{
		if (!isalnum(c) && c != '.' && c != '-' && c != '_' && !(allowPath && (c == '/' || c == '\\')))
			return false;
	}
	return true;
}

// Public Methods - Mandatory Checks
bool Sanitizer::sanitize_portNr(int portNr)
{
	return (portNr >= 1 && portNr <= 65535);
}

bool Sanitizer::sanitize_serverName(std::string& serverName)
{
	if (serverName.empty() || serverName.length() > 255) return false;

	std::string segment;
	std::istringstream segmentStream(serverName);

	while (std::getline(segmentStream, segment, '.'))
	{
		if (segment.empty() || segment.length() > 63
			|| !isalnum(segment.front()) || !isalnum(segment.back()))
		return false;

		for (char c : segment)
			if (!isalnum(c) && c != '-') return false;
	}
	return true;
}

bool Sanitizer::sanitize_root(std::string& root, const std::string& pwd)
{
	return isValidPath(root, "Root", pwd);
}

// Public Methods - Optional Checks
bool Sanitizer::sanitize_index(std::string& index)
{
	return index.empty() || isValidFilename(index, false);
}

bool Sanitizer::sanitize_errorPage(std::string &errorPage, const std::string &pwd) {

	std::istringstream stream(errorPage);
	std::vector<std::string> tokens;
	std::string token;

	while (stream >> token)
		tokens.push_back(token);

	if (tokens.size() < 2) {
		Logger::error("Error page definition must include at least one error code and a path.");
		return false;
	}

	std::string path = tokens.back();
	tokens.pop_back();

	if (!isValidPath(path, "Error page", pwd)) {
		Logger::error("Error page path invalid: " + path);
		return false;
	}

	for (const std::string &codeStr : tokens) {
		int code = 0;
		try {
			code = std::stoi(codeStr);
		} catch (...) {
			Logger::error("Invalid error code: " + codeStr);
			return false;
		}
		if (code < 400 || code > 599) {
			Logger::error("Error code out of range: " + std::to_string(code));
			return false;
		}
	}

	std::ostringstream oss;
	for (const std::string &codeStr : tokens) {
		oss << codeStr << " ";
	}
	oss << path;
	errorPage = oss.str();

	return true;
}


bool Sanitizer::sanitize_clMaxBodSize(long size) {
	return (size > 0 && size <= 2147483648L);
}

bool Sanitizer::sanitize_locationPath(std::string& locationPath, const std::string& pwd) {
	return locationPath.empty() || isValidPath(locationPath, "Location", pwd);
}

bool Sanitizer::sanitize_locationMethods(std::string& locationMethods) {
	if (locationMethods.empty()) return true;

	std::istringstream stream(locationMethods);
	std::string method;
	std::set<std::string> validMethods = {"GET", "POST", "DELETE"};
	std::set<std::string> seenMethods;

	while (stream >> method) {
		std::transform(method.begin(), method.end(), method.begin(), ::toupper);
		if (validMethods.find(method) == validMethods.end()) return false;
		if (seenMethods.find(method) != seenMethods.end()) return false;
		seenMethods.insert(method);
	}

	return !seenMethods.empty();
}

bool Sanitizer::sanitize_locationReturn(std::string& locationReturn) {
	if (locationReturn.empty()) return true;

	std::istringstream stream(locationReturn);
	int code;
	std::string url;

	if (!(stream >> code >> url)) return false;
	if (code < 300 || code > 308) return false;

	return sanitize_locationRedirect(url);
}

bool Sanitizer::sanitize_locationRoot(std::string& locationRoot, const std::string& pwd) {
	return locationRoot.empty() || isValidPath(locationRoot, "Location root", pwd);
}

bool Sanitizer::sanitize_locationAutoindex(std::string& locationAutoindex) {
	if (locationAutoindex.empty())
	{
		locationAutoindex = "off";
		return true;
	}

	std::string lowerCase = locationAutoindex;
	std::transform(lowerCase.begin(), lowerCase.end(), lowerCase.begin(), ::tolower);
	return (lowerCase == "on" || lowerCase == "off");
}

bool Sanitizer::sanitize_locationDefaultFile(std::string& locationDefaultFile) {
	if (locationDefaultFile.empty()) return true;
	return isValidFilename(locationDefaultFile, false);
}

bool Sanitizer::sanitize_locationUploadStore(std::string& locationUploadStore,
										const std::string& pwd,
										const std::string& serverRoot,
										const std::string& locationRoot) {

	std::string basePath;
	if (!locationRoot.empty()) {
		basePath = locationRoot;
	} else if (!serverRoot.empty()) {
		basePath = serverRoot;
	} else {
		basePath = pwd;
	}

	if (!basePath.empty() && basePath.back() == '/') {
		basePath.pop_back();
	}

	std::string fullPath;
	if (locationUploadStore[0] == '/') {
		fullPath = pwd + locationUploadStore;
	} else {
		fullPath = basePath + "/" + locationUploadStore;
	}

	std::vector<std::string> segments;
	std::stringstream ss(fullPath);
	std::string segment;

	while (std::getline(ss, segment, '/')) {
		if (segment.empty() || segment == ".") continue;
		if (segment == "..") {
			if (!segments.empty()) {
				segments.pop_back();
			}
			continue;
		}
		segments.push_back(segment);
	}

	fullPath = "/";
	for (const auto& seg : segments) {
		fullPath += seg + "/";
	}
	if (!segments.empty()) {
		fullPath.pop_back();
	}

	for (char c : fullPath) {
		if (!std::isalnum(static_cast<unsigned char>(c)) &&
			c != '/' && c != '_' && c != '-' && c != '.') {
			Logger::error("[Upload store] Invalid character in path: " + std::string(1, c));
			return false;
		}
	}

	if (!checkUploadStorePermissions(fullPath)) {
		return false;
	}

	locationUploadStore = fullPath;
	return true;
}

bool Sanitizer::sanitize_locationClMaxBodSize(long locationClMaxBodSize) {
	return sanitize_clMaxBodSize(locationClMaxBodSize);
}

bool Sanitizer::sanitize_timeout(int timeout) {
	return (timeout >= 10 && timeout <= 120);
}

bool Sanitizer::sanitize_locationCgi(std::string& locationCgi, std::string& locationCgiFileType, const std::string& pwd) {
	if (locationCgi.empty())
		return true;

	if (!isValidPath(locationCgi, "CGI", pwd))
		return false;

// if (access (locationcgi.c_str (), f_ok) == -1) {
// Logger :: Error ("[CGI] File does not exist:" + Locationcgi + "(" + Streror (Errno) + ")");
// return false;
// None

	{
		DIR* dirp = opendir(locationCgi.c_str());
		if (dirp != NULL) {
			closedir(dirp);
			Logger::error("[CGI] Path is a directory: " + locationCgi);
			return false;
		}
	}

	if (access(locationCgi.c_str(), X_OK) == -1) {
		Logger::error("[CGI] File is not executable: " + locationCgi);
		return false;
	}

	if (locationCgi.size() >= 7 && locationCgi.compare(locationCgi.size() - 7, 7, "php-cgi") == 0) {
		locationCgiFileType = ".php";
	} else if (locationCgi.size() >= 7 && locationCgi.compare(locationCgi.size() - 7, 7, "python3") == 0) {
		locationCgiFileType = ".py";
	}

	return true;
}

bool Sanitizer::sanitize_locationCgiParam(std::string& locationCgiParam) {
	if (locationCgiParam.empty()) return true;

	std::istringstream stream(locationCgiParam);
	std::string key, value;

	if (!(stream >> key >> value)) return false;
	if (key.empty() || value.empty()) return false;

	for (char c : key) {
		if (!isalnum(c) && c != '_') return false;
	}

	return true;
}

bool Sanitizer::sanitize_locationRedirect(std::string& locationRedirect) {
	if (locationRedirect.empty()) return true;

	if (locationRedirect.substr(0, 7) != "http://" &&
		locationRedirect.substr(0, 8) != "https://") {
		return false;
	}

	size_t schemeEnd = locationRedirect.find("://");
	return (schemeEnd != std::string::npos &&
			locationRedirect.length() > schemeEnd + 3);
}

bool Sanitizer::checkUploadStorePermissions(const std::string& path) {

	struct stat path_stat;
	if (stat(path.c_str(), &path_stat) != 0) {
		Logger::error("Cannot access path: " + path);
		return false;
	}

	if (!S_ISDIR(path_stat.st_mode)) {
		Logger::error("Path exists but is not a directory: " + path);
		return false;
	}

	if (access(path.c_str(), W_OK) != 0) {
		Logger::error("No write permission for directory: " + path);
		return false;
	}

	return true;
}

bool Sanitizer::isValidUploadPath(std::string& path, const std::string& context)
{
	if (path.empty()) {
		Logger::error("[" + context + "] Path is empty.");
		return false;
	}

// Normalize Path by Removing Consecutive Slashes and Resolving Relative Components
	std::vector<std::string> segments;
	std::stringstream ss(path);
	std::string segment;

	while (std::getline(ss, segment, '/')) {
		if (segment.empty() || segment == ".") continue;
		if (segment == "..") {
			if (!segments.empty()) segments.pop_back();
			continue;
		}
		segments.push_back(segment);
	}

// Rebuild Normalized Path
	path = "/";
	for (const auto& seg : segments) {
		path += seg + "/";
	}
	if (!segments.empty()) {
		path.pop_back();
	}

// Validate Characters
	for (char c : path) {
		if (!std::isalnum(static_cast<unsigned char>(c)) &&
			c != '/' && c != '_' && c != '-' && c != '.') {
			Logger::error("[" + context + "] Invalid character in path: " + std::string(1, c));
			return false;
		}
	}

// Create Directory with Proper Permissions
	return checkUploadStorePermissions(path);
}
