/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/27 11:28:08 by jeberle           #+#    #+#             */
/*   Updated: 2024/11/27 17:52:37 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./Utils.hpp"

Utils::Utils() { configFileValid = false; };
Utils::~Utils() { configFileValid = false; };

std::string trim(const std::string& str) {
	size_t first = str.find_first_not_of(" \t");
	if (first == std::string::npos) return "";
	size_t last = str.find_last_not_of(" \t");
	return str.substr(first, last - first + 1);
}

bool Utils::parseConfigContent(std::string filename) {
	std::ifstream configFile(filename);
	if (!configFile.is_open()) {
		Logger::error("Could not open config file");
		return false;
	}

	std::string line;
	ConfLocations currentLocation;
	bool inLocationBlock = false;

	while (std::getline(configFile, line)) {
		line = trim(line);

		if (line.find("location") == 0) {
			inLocationBlock = true;
			currentLocation = ConfLocations();
			size_t pos = line.find(" ");
			if (pos != std::string::npos) {
				currentLocation.port = std::stoi(line.substr(pos + 1)); // Beispielhaft
			}
			continue;
		}

		if (inLocationBlock && line.find("redirect") == 0) {
			size_t pos = line.find(" ");
			if (pos != std::string::npos) {
				currentLocation.redirect = line.substr(pos + 1);
			}
			continue;
		}

		if (inLocationBlock && line == "}") {
			registeredConfs.back().locations.push_back(currentLocation);
			inLocationBlock = false;
			continue;
		}

		if (inLocationBlock && line.find("methods") == 0) {
			size_t pos = line.find(" ");
			if (pos != std::string::npos) {
				currentLocation.methods = line.substr(pos + 1);
			}
		}
	}

	configFile.close();
	return true;
}

bool Utils::isConfigFile(const std::string& filepath) {
	std::ifstream file(filepath.c_str());
	if (!file.good())
	{
		Logger::error("File does not exist");
		return false;
	}
	if (filepath.size() >= 5 && filepath.substr(filepath.size() - 5) != ".conf")
	{
		Logger::error("File has to be of type .conf");
		return false;
	}
	if (!this->parseConfigContent(filepath))
	{
		Logger::error("Config file contains incorrect configurations");
		return false;
	}
	return true;
}

void Utils::parseArgs(int argc, char **argv) {
	if (argc == 2) {
		std::string filepath(argv[1]);
		if (isConfigFile(filepath)) {
			Logger::green() << "Config " << filepath << " added successfully!";
		} else {
			Logger::error() << "Invalid config file: " << filepath;
		}
	} else {
		Logger::error() << "Usage: ./webserv [CONFIG_PATH]";
	}
}

bool Utils::getconfigFileValid(void) const {
	return configFileValid;
}

const char* Utils::InvalidFileNameException::what() const throw() {
	return "Invalid config file name";
}

const char* Utils::InvalidFileContentException::what() const throw() {
	return "Invalid file content";
}