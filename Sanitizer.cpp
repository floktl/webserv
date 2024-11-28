/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Sanitizer.cpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/28 15:33:15 by jeberle           #+#    #+#             */
/*   Updated: 2024/11/28 15:33:50 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./Sanitizer.hpp"
#include "./Logger.hpp"

Sanitizer::Sanitizer() {}
Sanitizer::~Sanitizer() {}

bool Sanitizer::sanitize_portNr(std::string portNr) {
	try {
		int port = std::stoi(portNr);
		if (port < 1 || port > 65535) {
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
	if (serverName.empty() || serverName.length() > 255) {
		Logger::error("Server name must be between 1 and 255 characters");
		return false;
	}
	for (char c : serverName) {
		if (!isalnum(c) && c != '.' && c != '-') {
			Logger::error("Server name contains invalid characters");
			return false;
		}
	}
	return true;
}

bool Sanitizer::sanitize_root(std::string root) {
	if (root.empty() || root[0] != '/') {
		Logger::error("Root path must be absolute");
		return false;
	}
	return true;
}

bool Sanitizer::sanitize_index(std::string index) {
	if (index.empty() || index.find('/') != std::string::npos) {
		Logger::error("Index file name cannot be empty or contain path separators");
		return false;
	}
	return true;
}

bool Sanitizer::sanitize_errorPage(std::string errorPage) {
	std::istringstream stream(errorPage);
	int code;
	std::string path;

	stream >> code;
	if (code < 400 || code > 599) {
		Logger::error("Error page code must be between 400 and 599");
		return false;
	}

	stream >> path;
	if (path.empty() || path[0] != '/') {
		Logger::error("Error page path must be absolute");
		return false;
	}
	return true;
}

bool Sanitizer::sanitize_locationPath(std::string locationPath) {
	if (locationPath.empty() || locationPath[0] != '/') {
		Logger::error("Location path must start with /");
		return false;
	}
	return true;
}

bool Sanitizer::sanitize_locationMethods(std::string locationMethods) {
	std::istringstream stream(locationMethods);
	std::string method;
	std::set<std::string> validMethods = {"GET", "POST", "DELETE"};

	while (stream >> method) {
		if (validMethods.find(method) == validMethods.end()) {
			Logger::error("Invalid HTTP method: " + method);
			return false;
		}
	}
	return true;
}

bool Sanitizer::sanitize_locationCgi(std::string locationCgi) {
	if (locationCgi.empty() || locationCgi[0] != '/') {
		Logger::error("CGI path must be absolute");
		return false;
	}
	return true;
}

bool Sanitizer::sanitize_locationCgiParam(std::string locationCgiParam) {
	std::istringstream stream(locationCgiParam);
	std::string key, value;

	stream >> key;
	if (key.empty()) {
		Logger::error("CGI parameter key cannot be empty");
		return false;
	}

	stream >> value;
	if (value.empty()) {
		Logger::error("CGI parameter value cannot be empty");
		return false;
	}
	return true;
}

bool Sanitizer::sanitize_locationRedirect(std::string locationRedirect) {
	if (locationRedirect.empty()) {
		Logger::error("Redirect URL cannot be empty");
		return false;
	}
	if (locationRedirect.substr(0, 7) != "http://" &&
		locationRedirect.substr(0, 8) != "https://") {
		Logger::error("Redirect URL must start with http:// or https://");
		return false;
	}
	return true;
}