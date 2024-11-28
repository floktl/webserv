/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/27 11:28:08 by jeberle           #+#    #+#             */
/*   Updated: 2024/11/28 13:18:09 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./Utils.hpp"


Utils::Utils() {
	configFileValid = false;
	inServerBlock = false;
	inLocationBlock = false;
	linecount = 1;
	parsingErr = false;
	locBlockTar = "";
};

Utils::~Utils() {};

std::string trim(const std::string& str) {
	size_t first = str.find_first_not_of(" \t");
	if (first == std::string::npos) return "";
	size_t last = str.find_last_not_of(" \t");
	return str.substr(first, last - first + 1);
}

void Utils::parseLine(std::string line) {
	std::istringstream stream(line);
	std::string keyword, value;

	stream >> keyword;
	keyword = trim(keyword);

	if (keyword == "server" && line.find("{") != std::string::npos) {
		inServerBlock = true;
		registeredConfs.push_back(FileConfData());
		return;
	}

	if (keyword == "location") {
		if (!inServerBlock) {
			Logger::error("Error: Location block outside server block at line " + std::to_string(linecount));
			parsingErr = true;
			return;
		}
		inLocationBlock = true;
		std::string location_path;
		stream >> location_path;
		if (line.find("{") == std::string::npos) {
			Logger::error("Error: Missing { in location block at line " + std::to_string(linecount));
			parsingErr = true;
			return;
		}
		registeredConfs.back().locations.push_back(ConfLocations());
		locBlockTar = location_path;
		return;
	}

	if (keyword == "}") {
		if (inLocationBlock) {
			inLocationBlock = false;
			locBlockTar = "";
		} else if (inServerBlock) {
			inServerBlock = false;
		}
		return;
	}

	std::getline(stream, value);
	value = trim(value);
	if (!value.empty() && value.back() == ';') {
		value.pop_back();
		value = trim(value);
	} else {
		Logger::error("Error: Missing semicolon at line " + std::to_string(linecount));
		parsingErr = true;
		return;
	}

	if (inServerBlock && !inLocationBlock) {
		if (keyword == "listen") {
			registeredConfs.back().port = std::stoi(value);
		} else if (keyword == "server_name") {
			registeredConfs.back().name = value;
		} else if (keyword == "root") {
			registeredConfs.back().root = value;
		} else if (keyword == "index") {
			registeredConfs.back().index = value;
		} else if (keyword == "error_page") {
			registeredConfs.back().error_page = value;
		}
	} else if (inLocationBlock) {
		ConfLocations& currentLocation = registeredConfs.back().locations.back();
		if (keyword == "methods") {
			currentLocation.methods = value;
		} else if (keyword == "cgi") {
			currentLocation.cgi = value;
		} else if (keyword == "cgi_param") {
			currentLocation.cgi_param = value;
		} else if (keyword == "redirect") {
			currentLocation.redirect = value;
		}
	}
}

bool Utils::parseConfigContent(std::string filename) {
	std::ifstream configFile(filename);
	if (!configFile.is_open()) {
		Logger::error("Could not open config file");
		return false;
	}

	std::string line;
	while (std::getline(configFile, line)) {
		line = trim(line);
		if (!line.empty()) {
			parseLine(line);
			if (parsingErr) {
				configFile.close();
				return false;
			}
		}
		linecount++;
	}

	if (inServerBlock || inLocationBlock) {
		Logger::error("Error: Unclosed block at end of file");
		configFile.close();
		return false;
	}

	configFile.close();
	configFileValid = true;
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
			Logger::green("\nConfig " + filepath + " registered successfully!\n");
			printRegisteredConfs(filepath);
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

void Utils::printRegisteredConfs(std::string filename) {
	if (registeredConfs.empty()) {
		Logger::yellow("No configurations registered.");
		return;
	}

	Logger::green("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
	Logger::yellow(" Configurations by " + filename);
	Logger::green("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
	for (size_t i = 0; i < registeredConfs.size(); ++i) {
		const FileConfData& conf = registeredConfs[i];
		Logger::blue("\n  Server Block [" + std::to_string(i + 1) + "]:\n");
		Logger::white("    Port: ", false, 20);
		Logger::yellow(std::to_string(conf.port));
		Logger::white("    Server Name: ", false, 20);
		Logger::yellow(conf.name);
		Logger::white("    Root: ", false, 20);
		Logger::yellow(conf.root);
		Logger::white("    Index: ", false, 20);
		Logger::yellow(conf.index);
		Logger::white("    Error Page: ", false, 20);
		Logger::yellow(conf.error_page);

		if (conf.locations.empty()) {
			Logger::yellow("  No Location Blocks.");
		} else {
			for (size_t j = 0; j < conf.locations.size(); ++j) {
				const ConfLocations& location = conf.locations[j];
				Logger::cyan("\n    Location [" + std::to_string(j + 1) + "]:\n");
				Logger::white("      Methods: ", false, 20);
				Logger::yellow(location.methods);
				Logger::white("      CGI: ", false, 20);
				Logger::yellow(location.cgi);
				Logger::white("      CGI Param: ", false, 20);
				Logger::yellow(location.cgi_param);
				Logger::white("      Redirect: ", false, 20);
				Logger::yellow(location.redirect);
			}
		}
	}
	Logger::green("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
}
