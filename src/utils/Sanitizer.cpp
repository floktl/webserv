/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Sanitizer.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/28 15:33:15 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/06 15:11:27 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./Sanitizer.hpp"

Sanitizer::Sanitizer() {}
Sanitizer::~Sanitizer() {}

bool Sanitizer::isValidPath(std::string& path, const std::string& context, const std::string& pwd) {
	if (path.empty()) {
		Logger::error("[" + context + "] Path is empty.");
		return false;
	}

	// If the path does not start with '/', prepend the current working directory (pwd)
	std::string normalizedPath = path;
	if (normalizedPath[0] != '/') {
		normalizedPath = pwd + "/" + normalizedPath;
	}

	// Validate the path by splitting and checking for directory traversal attempts
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

	// Rebuild the cleaned absolute path
	normalizedPath = "/";
	for (const auto& seg : segments) {
		normalizedPath += seg + "/";
	}
	if (!segments.empty()) {
		normalizedPath.pop_back(); // Remove the trailing slash
	}

	// Check for invalid characters in the normalized path
	for (char c : normalizedPath) {
		if (!std::isalnum(static_cast<unsigned char>(c)) && c != '/' && c != '_' && c != '-' && c != '.') {
			Logger::error("[" + context + "] Invalid character '" + std::string(1, c) + "' found in path: " + normalizedPath);
			return false;
		}
	}

	// For Root or Error page contexts, verify that the path exists and is of the correct type
	if (context == "Root" || context == "Error page") {
		struct stat path_stat;
		if (stat(normalizedPath.c_str(), &path_stat) == -1) {
			Logger::error("[" + context + "] Path does not exist: " + normalizedPath);
			return false;
		}

		if (context == "Root" && !S_ISDIR(path_stat.st_mode)) {
			Logger::error("[" + context + "] Root path is not a directory: " + normalizedPath);
			return false;
		}
		if (context == "Error page" && !S_ISREG(path_stat.st_mode)) {
			Logger::error("[" + context + "] Error page path is not a file: " + normalizedPath);
			return false;
		}
	}

	// If all checks pass, update the original path to the normalized absolute path
	path = normalizedPath;
	return true;
}


long Sanitizer::parseSize(const std::string& sizeStr, const std::string& unit) {
	long size;
	try {
		size = std::stol(sizeStr);
	} catch (...) {
		return -1;
	}

	if (size <= 0) return -1;

	if (unit == "K" || unit == "KB") return size * 1024L;
	if (unit == "M" || unit == "MB") return size * 1024L * 1024L;
	if (unit == "G" || unit == "GB") return size * 1024L * 1024L * 1024L;
	if (unit.empty()) return size;

	return -1;
}

bool Sanitizer::isValidFilename(const std::string& filename, bool allowPath) {
	if (filename.empty()) return false;

	if (!allowPath && (filename.find('/') != std::string::npos ||
					filename.find('\\') != std::string::npos)) {
		return false;
	}

	for (char c : filename) {
		if (!isalnum(c) && c != '.' && c != '-' && c != '_' &&
			!(allowPath && (c == '/' || c == '\\'))) {
			return false;
		}
	}

	return true;
}

// Public methods - Mandatory checks
bool Sanitizer::sanitize_portNr(int portNr) {
	return (portNr >= 1 && portNr <= 65535);
}

bool Sanitizer::sanitize_serverName(std::string& serverName) {
	if (serverName.empty() || serverName.length() > 255) return false;

	std::string segment;
	std::istringstream segmentStream(serverName);

	while (std::getline(segmentStream, segment, '.')) {
		if (segment.empty() || segment.length() > 63) return false;
		if (!isalnum(segment.front()) || !isalnum(segment.back())) return false;

		for (char c : segment) {
			if (!isalnum(c) && c != '-') return false;
		}
	}

	return true;
}

bool Sanitizer::sanitize_root(std::string& root, const std::string& pwd) {
	return isValidPath(root, "Root", pwd);
}

// Public methods - Optional checks
bool Sanitizer::sanitize_index(std::string& index) {
	return index.empty() || isValidFilename(index, false);
}

bool Sanitizer::sanitize_errorPage(std::string &errorPage, const std::string& pwd) {
	std::istringstream stream(errorPage);
	int code;
	std::string path;

	if (!(stream >> code >> path)) return false;
	if (code < 400 || code > 599) return false;

	if (!isValidPath(path, "Error page", pwd)) {
		return false;
	}

	std::ostringstream oss;
	oss << code << " " << path;
	errorPage = oss.str();
	return true;
}

bool Sanitizer::sanitize_clMaxBodSize(std::string& clMaxBodSize) {
	if (clMaxBodSize.empty()) return true;

	std::istringstream stream(clMaxBodSize);
	std::string sizeStr, unit;
	stream >> sizeStr >> unit;

	long size = parseSize(sizeStr, unit);
	return (size != -1 && size <= (1024L * 1024L * 1024L * 2L)); // Max 2GB
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
	if (locationAutoindex.empty()) return true;

	std::transform(locationAutoindex.begin(), locationAutoindex.end(),
				locationAutoindex.begin(), ::tolower);
	return (locationAutoindex == "on" || locationAutoindex == "off");
}

bool Sanitizer::sanitize_locationDefaultFile(std::string& locationDefaultFile) {
	if (locationDefaultFile.empty()) return true;
	return isValidFilename(locationDefaultFile, false);
}

bool Sanitizer::sanitize_locationUploadStore(std::string& locationUploadStore, const std::string& pwd) {
	return locationUploadStore.empty() || isValidPath(locationUploadStore, "Upload store", pwd);
}

bool Sanitizer::sanitize_locationClMaxBodSize(std::string& locationClMaxBodSize) {
	return sanitize_clMaxBodSize(locationClMaxBodSize);
}

bool Sanitizer::sanitize_locationCgi(std::string& locationCgi, const std::string& pwd) {
	return locationCgi.empty() || isValidPath(locationCgi, "CGI", pwd);
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