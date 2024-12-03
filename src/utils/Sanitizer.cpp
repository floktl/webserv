/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Sanitizer.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/28 15:33:15 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/03 09:31:48 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./Sanitizer.hpp"

Sanitizer::Sanitizer() {}
Sanitizer::~Sanitizer() {}

// Private helper methods
bool Sanitizer::isValidPath(const std::string& path, const std::string& context) {
    if (path.empty() || path[0] != '/') {
        Logger::error(context + " path must be absolute");
        return false;
    }

    for (char c : path) {
        if (!isalnum(c) && c != '/' && c != '_' && c != '-' && c != '.') {
            Logger::error("Invalid character in " + context + " path");
            return false;
        }
    }

    if (path.find("..") != std::string::npos) {
        Logger::error("Directory traversal not allowed in " + context);
        return false;
    }

    // Check if path exists
    if (access(path.c_str(), F_OK) == -1) {
        Logger::error(context + " path does not exist: " + path);
        return false;
    }

    // Check read permissions
    if (access(path.c_str(), R_OK) == -1) {
        Logger::error("No read permission for " + context + " path: " + path);
        return false;
    }

    // Additional checks based on context
    struct stat path_stat;
    if (stat(path.c_str(), &path_stat) == -1) {
        Logger::error("Unable to get status for " + context + " path: " + path);
        return false;
    }

    // Specific checks for different contexts
    if (context == "CGI") {
        // For CGI, check if it's a file and is executable
        if (!S_ISREG(path_stat.st_mode)) {
            Logger::error("CGI path must be a regular file: " + path);
            return false;
        }
        if (access(path.c_str(), X_OK) == -1) {
            Logger::error("CGI file is not executable: " + path);
            return false;
        }
    }
    else if (context == "Upload store") {
        // For upload directory, check if it's a directory and is writable
        if (!S_ISDIR(path_stat.st_mode)) {
            Logger::error("Upload store must be a directory: " + path);
            return false;
        }
        if (access(path.c_str(), W_OK) == -1) {
            Logger::error("Upload directory is not writable: " + path);
            return false;
        }
    }
    else {
        // For other paths (Root, Location root, etc.), check if it's a directory
        if (!S_ISDIR(path_stat.st_mode)) {
            // Special case for error pages which should be files
            if (context == "Error page") {
                if (!S_ISREG(path_stat.st_mode)) {
                    Logger::error("Error page must be a regular file: " + path);
                    return false;
                }
            } else {
                Logger::error(context + " path must be a directory: " + path);
                return false;
            }
        }
    }

    return true;
}

long Sanitizer::parseSize(const std::string& sizeStr, const std::string& unit) {
	long size;
	try {
		size = std::stol(sizeStr);
	} catch (...) {
		Logger::error("Invalid size format");
		return -1;
	}

	if (size <= 0) {
		Logger::error("Size must be positive");
		return -1;
	}

	if (unit == "K" || unit == "KB") return size * 1024L;
	if (unit == "M" || unit == "MB") return size * 1024L * 1024L;
	if (unit == "G" || unit == "GB") return size * 1024L * 1024L * 1024L;
	if (unit.empty()) return size;

	Logger::error("Invalid size unit. Use K/KB, M/MB, or G/GB");
	return -1;
}

bool Sanitizer::isValidFilename(const std::string& filename, bool allowPath) {
	if (filename.empty()) {
		Logger::error("Filename cannot be empty");
		return false;
	}

	if (!allowPath && (filename.find('/') != std::string::npos ||
					filename.find('\\') != std::string::npos)) {
		Logger::error("Filename cannot contain path separators");
		return false;
	}

	for (char c : filename) {
		if (!isalnum(c) && c != '.' && c != '-' && c != '_' &&
			!(allowPath && (c == '/' || c == '\\'))) {
			Logger::error("Invalid character in filename");
			return false;
		}
	}

	return true;
}

// Public methods
bool Sanitizer::sanitize_portNr(int portNr) {
	try {
		if (portNr < 1 || portNr > 65535) {
			Logger::error("Port number must be between 1 and 65535");
			return false;
		}
		return true;
	} catch (std::exception& e) {
		Logger::error("Invalid port number format");
		return false;
	}
}

bool Sanitizer::sanitize_serverName(std::string serverName) {
	std::string lowerName = serverName;
	std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);

	if (serverName.empty() || serverName.length() > 255) {
		Logger::error("Server name must be between 1 and 255 characters");
		return false;
	}

	std::vector<std::string> segments;
	std::string segment;
	std::istringstream segmentStream(serverName);

	while (std::getline(segmentStream, segment, '.')) {
		if (segment.empty() || segment.length() > 63) {
			Logger::error("Invalid hostname segment length");
			return false;
		}

		if (!isalnum(segment.front()) || !isalnum(segment.back())) {
			Logger::error("Segments must start and end with alphanumeric characters");
			return false;
		}

		for (char c : segment) {
			if (!isalnum(c) && c != '-') {
				Logger::error("Invalid character in hostname");
				return false;
			}
		}

		segments.push_back(segment);
	}

	if (segments.empty()) {
		Logger::error("No valid hostname segments found");
		return false;
	}

	return true;
}

bool Sanitizer::sanitize_root(std::string root) {
	return isValidPath(root, "Root");
}

bool Sanitizer::sanitize_index(std::string index) {
	return isValidFilename(index, false);
}

bool Sanitizer::sanitize_errorPage(std::string errorPage) {
	std::istringstream stream(errorPage);
	int code;
	std::string path;

	if (!(stream >> code)) {
		Logger::error("Invalid error page format: missing error code");
		return false;
	}

	if (code < 400 || code > 599) {
		Logger::error("Error page code must be between 400 and 599");
		return false;
	}

	if (!(stream >> path)) {
		Logger::error("Invalid error page format: missing path");
		return false;
	}

	return isValidPath(path, "Error page");
}

bool Sanitizer::sanitize_clMaxBodSize(std::string clMaxBodSize) {
	std::istringstream stream(clMaxBodSize);
	std::string sizeStr, unit;

	if (!(stream >> sizeStr)) {
		Logger::error("Invalid body size format");
		return false;
	}

	stream >> unit;

	long size = parseSize(sizeStr, unit);
	if (size == -1) return false;

	if (size > (1024L * 1024L * 1024L * 2L)) { // Max 2GB
		Logger::error("Client max body size too large (max 2GB)");
		return false;
	}

	return true;
}

bool Sanitizer::sanitize_locationPath(std::string locationPath) {
	return isValidPath(locationPath, "Location");
}

bool Sanitizer::sanitize_locationMethods(std::string locationMethods) {
	std::istringstream stream(locationMethods);
	std::string method;
	std::set<std::string> validMethods = {"GET", "POST", "DELETE"};
	std::set<std::string> seenMethods;

	while (stream >> method) {
		std::transform(method.begin(), method.end(), method.begin(), ::toupper);

		if (validMethods.find(method) == validMethods.end()) {
			Logger::error("Invalid HTTP method: " + method);
			return false;
		}

		if (seenMethods.find(method) != seenMethods.end()) {
			Logger::error("Duplicate HTTP method: " + method);
			return false;
		}

		seenMethods.insert(method);
	}

	if (seenMethods.empty()) {
		Logger::error("At least one HTTP method must be specified");
		return false;
	}

	return true;
}

bool Sanitizer::sanitize_locationReturn(std::string locationReturn) {
	std::istringstream stream(locationReturn);
	int code;
	std::string url;

	if (!(stream >> code)) {
		Logger::error("Invalid redirect format: missing status code");
		return false;
	}

	if (code < 300 || code > 308) {
		Logger::error("Redirect status code must be between 300 and 308");
		return false;
	}

	if (!(stream >> url)) {
		Logger::error("Invalid redirect format: missing URL");
		return false;
	}

	return sanitize_locationRedirect(url);
}

bool Sanitizer::sanitize_locationRoot(std::string locationRoot) {
	return isValidPath(locationRoot, "Location root");
}

bool Sanitizer::sanitize_locationAutoindex(std::string locationAutoindex) {
	std::transform(locationAutoindex.begin(), locationAutoindex.end(),
				locationAutoindex.begin(), ::tolower);

	if (locationAutoindex != "on" && locationAutoindex != "off") {
		Logger::error("Autoindex must be 'on' or 'off'");
		return false;
	}

	return true;
}

bool Sanitizer::sanitize_locationDefaultFile(std::string locationDefaultFile) {
	if (!isValidFilename(locationDefaultFile, false)) return false;

	size_t dotPos = locationDefaultFile.find_last_of(".");
	if (dotPos == std::string::npos || dotPos == 0 ||
		dotPos == locationDefaultFile.length() - 1) {
		Logger::error("Invalid file format: must have a valid name and extension");
		return false;
	}

	std::string filename = locationDefaultFile.substr(0, dotPos);
	std::string extension = locationDefaultFile.substr(dotPos + 1);

	if (filename.empty() || extension.empty()) {
		Logger::error("Invalid file format: must have both name and extension");
		return false;
	}

	return true;
}

bool Sanitizer::sanitize_locationUploadStore(std::string locationUploadStore) {
	return isValidPath(locationUploadStore, "Upload store");
}

bool Sanitizer::sanitize_locationClMaxBodSize(std::string locationClMaxBodSize) {
	return sanitize_clMaxBodSize(locationClMaxBodSize);
}

bool Sanitizer::sanitize_locationCgi(std::string locationCgi) {
	return isValidPath(locationCgi, "CGI");
}

bool Sanitizer::sanitize_locationCgiParam(std::string locationCgiParam) {
	std::istringstream stream(locationCgiParam);
	std::string key, value;

	if (!(stream >> key)) {
		Logger::error("CGI parameter key cannot be empty");
		return false;
	}

	if (!(stream >> value)) {
		Logger::error("CGI parameter value cannot be empty");
		return false;
	}

	// Additional key validation could be added here
	for (char c : key) {
		if (!isalnum(c) && c != '_') {
			Logger::error("Invalid character in CGI parameter key");
			return false;
		}
	}

	return true;
}

bool Sanitizer::sanitize_locationRedirect(std::string locationRedirect) {
	if (locationRedirect.empty()) {
		Logger::error("Redirect URL cannot be empty");
		return false;
	}

	std::vector<std::string> validSchemes = {"http://", "https://"};
	bool validScheme = false;
	for (const auto& scheme : validSchemes) {
		if (locationRedirect.substr(0, scheme.length()) == scheme) {
			validScheme = true;
			break;
		}
	}

	if (!validScheme) {
		Logger::error("Redirect URL must start with http:// or https://");
		return false;
	}

	size_t schemeEnd = locationRedirect.find("://");
	if (schemeEnd == std::string::npos ||
		locationRedirect.length() <= schemeEnd + 3) {
		Logger::error("Invalid URL format");
		return false;
	}

	return true;
}