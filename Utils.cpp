/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/27 11:28:08 by jeberle           #+#    #+#             */
/*   Updated: 2024/11/28 17:06:00 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./Utils.hpp"
#include "./Sanitizer.hpp"

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

std::vector<std::string> parseOptionsToVector(const std::string& opts) {
	std::vector<std::string> result;
	std::istringstream stream(opts);
	std::string opt;
	while (std::getline(stream, opt, ',')) {
		result.push_back(trim(opt));
	}
	return result;
}

void Utils::parseLine(std::string line) {
	static const std::vector<std::string> serverOpts = parseOptionsToVector(CONFIG_OPTS);
	static const std::vector<std::string> locationOpts = parseOptionsToVector(LOCATION_OPTS);

	std::istringstream stream(line);
	std::string keyword, value;

	stream >> keyword;
	keyword = trim(keyword);

	if (line.find("{") != std::string::npos) {
		if (line.find("}") != std::string::npos) {
			Logger::error("Error: at line " + std::to_string(linecount) + " Only one scope per line allowed");
			parsingErr = true;
			return;
		}
		if (keyword == "server") {
			if (inServerBlock) {
				Logger::error("Error: Nested server block at line " + std::to_string(linecount));
				parsingErr = true;
				return;
			}
			inServerBlock = true;
			registeredConfs.push_back(FileConfData());
			return;
		} else if (keyword == "location") {
			if (!inServerBlock || inLocationBlock) {
				Logger::error("Error: Invalid location block at line " + std::to_string(linecount));
				parsingErr = true;
				return;
			}
			std::string location_path;
			stream >> location_path;

			std::string rest;
			std::getline(stream, rest);
			rest = trim(rest);

			if (rest != "{") {
				Logger::error("Error: Location definition must be followed by its path and { on same line at line " + std::to_string(linecount));
				parsingErr = true;
				return;
			}

			inLocationBlock = true;
			registeredConfs.back().locations.push_back(ConfLocations());
			registeredConfs.back().locations.back().path = location_path;
			return;
		}
	}

	if (keyword == "}") {
		if (!inServerBlock && !inLocationBlock) {
			Logger::error("Error: Unexpected } at line " + std::to_string(linecount));
			parsingErr = true;
			return;
		}
		if (inLocationBlock)
			inLocationBlock = false;
		else
			inServerBlock = false;
		return;
	}

	if (!inServerBlock) {
		Logger::error("Error: Configuration outside server block at line " + std::to_string(linecount));
		parsingErr = true;
		return;
	}

	std::getline(stream, value);
	value = trim(value);
	if (value.empty() || value.back() != ';') {
		Logger::error("Error: Missing semicolon at line " + std::to_string(linecount));
		parsingErr = true;
		return;
	}
	value.pop_back();
	value = trim(value);

	if (!inLocationBlock) {
		if (std::find(serverOpts.begin(), serverOpts.end(), keyword) == serverOpts.end() || keyword == "server") {
			Logger::error("Error: Invalid server directive '" + keyword + "' at line " + std::to_string(linecount));
			parsingErr = true;
			return;
		}
		if (keyword == "listen")
		{
			try {
				registeredConfs.back().port = std::stoi(value);
			} catch (const std::out_of_range& e) {
				Logger::error("Error: '" + keyword + "' value at line " + std::to_string(linecount) + " cannot be interpreted as a valid port");
				parsingErr = true;
				return;
			}
		}
		else if (keyword == "server_name")
			registeredConfs.back().name = value;
		else if (keyword == "root")
			registeredConfs.back().root = value;
		else if (keyword == "index")
			registeredConfs.back().index = value;
		else if (keyword == "error_page")
			registeredConfs.back().error_page = value;
	} else {
		if (std::find(locationOpts.begin(), locationOpts.end(), keyword) == locationOpts.end()) {
			Logger::error("Error: Invalid location directive '" + keyword + "' at line " + std::to_string(linecount));
			parsingErr = true;
			return;
		}
		ConfLocations& currentLocation = registeredConfs.back().locations.back();
		if (keyword == "methods")
			currentLocation.methods = value;
		else if (keyword == "cgi")
			currentLocation.cgi = value;
		else if (keyword == "cgi_param")
			currentLocation.cgi_param = value;
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

// struct ConfLocations {
// 	int port;
// 	std::string path;
// 	std::string methods;
// 	std::string cgi;
// 	std::string cgi_param;
// 	std::string redirect;
// };

// struct FileConfData {
// 	int port;
// 	std::string name;
// 	std::string root;
// 	std::string index;
// 	std::string error_page;
// 	std::vector<ConfLocations> locations;
// };

bool Utils::sanitizeConfData(void) {
	if (TODO)
		Logger::white("TODO: add autoindex to locations");
	std::set<int> usedPorts;
	for (size_t i = 0; i < registeredConfs.size(); ++i)
	{
		// port
		if (Sanitizer::sanitize_portNr(registeredConfs[i].port))
		{
			if (usedPorts.find(registeredConfs[i].port) == usedPorts.end())
			{
				usedPorts.insert(registeredConfs[i].port);
			}
			else
			{
				Logger::magenta("Server Block " + std::to_string((i + 1)) + " ignored, because Port " + std::to_string(registeredConfs[i].port) + " already used.");
				registeredConfs.erase(registeredConfs.begin() + i);
				--i;
			}
		}
		else
			return false;

		// name
		if (!Sanitizer::sanitize_serverName(registeredConfs[i].name))
		{
			return false;
		}

		// root
		if (!Sanitizer::sanitize_root(registeredConfs[i].root))
		{
			return false;
		}

		// index
		if (!Sanitizer::sanitize_index(registeredConfs[i].index))
		{
			return false;
		}

		// error_page
		if (!Sanitizer::sanitize_errorPage(registeredConfs[i].error_page))
		{
			return false;
		}
	}
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
	if (!this->sanitizeConfData())
	{
		Logger::error("Config file contains logical errors");
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
				Logger::cyan("\n    Location [" + std::to_string(j + 1) + "]:   " + location.path + "");
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
