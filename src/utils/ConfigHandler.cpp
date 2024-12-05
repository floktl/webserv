/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigHandler.cpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/27 11:28:08 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/05 15:21:26 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./ConfigHandler.hpp"
#include "./Sanitizer.hpp"

// initialize handler with necessay flags
ConfigHandler::ConfigHandler() {
	configFileValid = false;
	inServerBlock = false;
	inLocationBlock = false;
	linecount = 1;
	parsingErr = false;
	locBlockTar = "";
	env = NULL;
};

ConfigHandler::~ConfigHandler() {};

// internal trimming
std::string trim(const std::string& str) {
	size_t first = str.find_first_not_of(" \t");
	if (first == std::string::npos) return "";
	size_t last = str.find_last_not_of(" \t");
	return str.substr(first, last - first + 1);
}

// parsing constants
std::vector<std::string> parseOptionsToVector(const std::string& opts) {
	std::vector<std::string> result;
	std::istringstream stream(opts);
	std::string opt;
	while (std::getline(stream, opt, ',')) {
		result.push_back(trim(opt));
	}
	return result;
}

// expand ENV varibales in bash style for config values
std::string expandEnvironmentVariables(const std::string& value, char** env) {
	if (!env) return value;

	std::string result = value;
	size_t pos = 0;

	while ((pos = result.find('$', pos)) != std::string::npos) {
		// Check if we have a valid variable name after $
		if (pos + 1 >= result.length()) break;

		// Find the end of the variable name
		size_t end = pos + 1;
		while (end < result.length() &&
			(std::isalnum(result[end]) || result[end] == '_')) {
			end++;
		}

		// Extract the variable name
		std::string varName = result.substr(pos + 1, end - (pos + 1));

		// Search for the variable in environment
		bool found = false;
		for (char** envVar = env; *envVar != nullptr; envVar++) {
			std::string envStr(*envVar);
			size_t equalPos = envStr.find('=');
			if (equalPos != std::string::npos) {
				std::string name = envStr.substr(0, equalPos);
				if (name == varName) {
					std::string replacer = envStr.substr(equalPos + 1);
					result.replace(pos, end - pos, replacer);
					pos += replacer.length();
					found = true;
					break;
				}
			}
		}

		// If variable not found, leave it unchanged and move past it
		if (!found) {
			pos = end;
		}
	}

	return result;
}

// Parses a single line of the configuration file
// Handles server blocks, location blocks, and their respective directives
void ConfigHandler::parseLine(std::string line) {
	static const std::vector<std::string> serverOpts =
		parseOptionsToVector(CONFIG_OPTS);
	static const std::vector<std::string> locationOpts =
		parseOptionsToVector(LOCATION_OPTS);

	std::istringstream stream(line);
	std::string keyword, value;

	stream >> keyword;
	keyword = trim(keyword);
	line = trim(line);

	// Skip comment lines
	if (line.find("#") == 0) {
		return;
	}

	// Handle block opening
	if (line.find("{") != std::string::npos) {
		// Prevent multiple scopes per line
		if (line.find("}") != std::string::npos) {
			Logger::error("Error: at line " + std::to_string(linecount)
				+ " Only one scope per line allowed");
			parsingErr = true;
			return;
		}

		// Handle server block opening
		if (keyword == "server") {
			if (inServerBlock) {
				Logger::error("Error: Nested server block at line "
					+ std::to_string(linecount));
				parsingErr = true;
				return;
			}
			inServerBlock = true;
			registeredServerConfs.push_back(ServerBlock());
			return;
		}
		// Handle location block opening
		else if (keyword == "location") {
			if (!inServerBlock || inLocationBlock) {
				Logger::error("Error: Invalid location block at line "
					+ std::to_string(linecount));
				parsingErr = true;
				return;
			}
			std::string location_path;
			stream >> location_path;

			std::string rest;
			std::getline(stream, rest);
			rest = trim(rest);

			if (rest != "{") {
				Logger::error("Error: Location definition must be followed by \
					its path and { on same line at line " + std::to_string(linecount));
				parsingErr = true;
				return;
			}

			inLocationBlock = true;
			registeredServerConfs.back().locations.push_back(Location());
			registeredServerConfs.back().locations.back().path = location_path;
			return;
		}
	}

	// Handle block closing
	if (keyword == "}") {
		if (!inServerBlock && !inLocationBlock) {
			Logger::error("Error: Unexpected } at line "
				+ std::to_string(linecount));
			parsingErr = true;
			return;
		}
		if (inLocationBlock)
			inLocationBlock = false;
		else
			inServerBlock = false;
		return;
	}

	// Ensure directives are within a server block
	if (!inServerBlock) {
		Logger::error("Error: Configuration outside server block at line "
			+ std::to_string(linecount));
		parsingErr = true;
		return;
	}

	// Parse directive value
	std::getline(stream, value);
	value = trim(value);
	if (value.empty() || value.back() != ';') {
		Logger::error("Error: Missing semicolon at line "
			+ std::to_string(linecount));
		parsingErr = true;
		return;
	}
	value.pop_back();  // Remove semicolon
	value = trim(value);
	value = expandEnvironmentVariables(value, env);

	// Handle server-level directives
	if (!inLocationBlock) {
		if (std::find(serverOpts.begin(), serverOpts.end(), keyword)
			== serverOpts.end() || keyword == "server") {
			Logger::error("Error: Invalid server directive '"
				+ keyword + "' at line " + std::to_string(linecount));
			parsingErr = true;
			return;
		}

		// Process different server directives
		if (keyword == "listen") {
			try {
				registeredServerConfs.back().port = std::stoi(value);
			} catch (const std::out_of_range& e) {
				Logger::error("Error: '" + keyword + "' value at line "
				+ std::to_string(linecount)
				+ " cannot be interpreted as a valid port");
				parsingErr = true;
				return;
			}
		}
		else if (keyword == "server_name")
			registeredServerConfs.back().name = value;
		else if (keyword == "root")
			registeredServerConfs.back().root = value;
		else if (keyword == "index")
			registeredServerConfs.back().index = value;
		else if (keyword == "error_page")
			registeredServerConfs.back().error_page = value;
	}
	// Handle location-level directives
	else {
		if (std::find(locationOpts.begin(), locationOpts.end(), keyword)
			== locationOpts.end()) {
			Logger::error("Error: Invalid location directive '" + keyword
				+ "' at line " + std::to_string(linecount));
			parsingErr = true;
			return;
		}
		Location& currentLocation = registeredServerConfs.back().locations.back();
		if (keyword == "methods")
			currentLocation.methods = value;
		else if (keyword == "cgi")
			currentLocation.cgi = value;
		else if (keyword == "cgi_param")
			currentLocation.cgi_param = value;
	}
}

bool ConfigHandler::parseConfigContent(std::string filename) {
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

bool ConfigHandler::sanitizeConfData(void) {
	std::set<int> usedPorts;
	for (size_t i = 0; i < registeredServerConfs.size(); ++i) {
		// Mandatory checks
		if (registeredServerConfs[i].port == 0) {  // Add this check
			Logger::error("Port number is mandatory for server block " + std::to_string(i + 1));
			configFileValid = false;
			return false;
		}
		// port - mandatory
		if (Sanitizer::sanitize_portNr(registeredServerConfs[i].port)) {
			if (usedPorts.find(registeredServerConfs[i].port) == usedPorts.end()) {
				usedPorts.insert(registeredServerConfs[i].port);
			} else {
				Logger::magenta("Server Block " + std::to_string((i + 1))
					+ " ignored, because Port " + std::to_string(registeredServerConfs[i].port) + " already used.");
				registeredServerConfs.erase(registeredServerConfs.begin() + i);
				--i;
				continue;
			}
		} else {
			configFileValid = false;
			return false;
		}

		// root - mandatory
		if (!Sanitizer::sanitize_root(registeredServerConfs[i].root, expandEnvironmentVariables("$PWD", env))) {
			configFileValid = false;
			return false;
		}

		// Optional server block checks
		// server_name - optional
		if (!registeredServerConfs[i].name.empty() &&
			!Sanitizer::sanitize_serverName(registeredServerConfs[i].name)) {
			configFileValid = false;
			return false;
		}

		// index - optional
		if (!registeredServerConfs[i].index.empty() &&
			!Sanitizer::sanitize_index(registeredServerConfs[i].index)) {
			configFileValid = false;
			return false;
		}

		// error_page - optional
		if (!registeredServerConfs[i].error_page.empty() &&
			!Sanitizer::sanitize_errorPage(registeredServerConfs[i].error_page, expandEnvironmentVariables("$PWD", env))) {
			configFileValid = false;
			return false;
		}

		// client_max_body_size - optional
		if (!registeredServerConfs[i].client_max_body_size.empty() &&
			!Sanitizer::sanitize_clMaxBodSize(registeredServerConfs[i].client_max_body_size)) {
			configFileValid = false;
			return false;
		}
		if (registeredServerConfs[i].locations.empty()) {
			Logger::error("Server block " + std::to_string(i + 1) + " must have at least one location block");
			configFileValid = false;
			return false;
		}
		// Location blocks validation
		for (size_t j = 0; j < registeredServerConfs[i].locations.size(); ++j) {
			Location& loc = registeredServerConfs[i].locations[j];

			// location path - mandatory for location block
			if (!Sanitizer::sanitize_locationPath(loc.path, expandEnvironmentVariables("$PWD", env))) {
				configFileValid = false;
				return false;
			}

			// Optional location block checks
			if (!loc.methods.empty() &&
				!Sanitizer::sanitize_locationMethods(loc.methods)) {
				configFileValid = false;
				return false;
			}

			if (!loc.return_directive.empty() &&
				!Sanitizer::sanitize_locationReturn(loc.return_directive)) {
				configFileValid = false;
				return false;
			}

			if (!loc.root.empty() &&
				!Sanitizer::sanitize_locationRoot(loc.root, expandEnvironmentVariables("$PWD", env))) {
				configFileValid = false;
				return false;
			}

			if (!loc.autoindex.empty() &&
				!Sanitizer::sanitize_locationAutoindex(loc.autoindex)) {
				configFileValid = false;
				return false;
			}

			if (!loc.default_file.empty() &&
				!Sanitizer::sanitize_locationDefaultFile(loc.default_file)) {
				configFileValid = false;
				return false;
			}

			if (!loc.upload_store.empty() &&
				!Sanitizer::sanitize_locationUploadStore(loc.upload_store, expandEnvironmentVariables("$PWD", env))) {
				configFileValid = false;
				return false;
			}

			if (!loc.client_max_body_size.empty() &&
				!Sanitizer::sanitize_locationClMaxBodSize(loc.client_max_body_size)) {
				configFileValid = false;
				return false;
			}

			if (!loc.cgi.empty() &&
				!Sanitizer::sanitize_locationCgi(loc.cgi, expandEnvironmentVariables("$PWD", env))) {
				configFileValid = false;
				return false;
			}

			if (!loc.cgi_param.empty() &&
				!Sanitizer::sanitize_locationCgiParam(loc.cgi_param)) {
				configFileValid = false;
				return false;
			}
		}
	}
	return true;
}

bool ConfigHandler::isConfigFile(const std::string& filepath) {
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

void ConfigHandler::parseArgs(int argc, char **argv, char **envp) {
	if (argc == 2) {
		env = envp;
		std::string filepath(argv[1]);
		if (isConfigFile(filepath)) {
			Logger::green("\nConfig " + filepath + " registered successfully!\n");
			printRegisteredConfs(filepath);
		}
	} else {
		Logger::error() << "Usage: ./webserv [CONFIG_PATH]";
	}
}

bool ConfigHandler::getconfigFileValid(void) const {
	return configFileValid;
}

void ConfigHandler::printRegisteredConfs(std::string filename) {
	if (registeredServerConfs.empty()) {
		Logger::yellow("No configurations registered.");
		return;
	}

	Logger::green("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
	Logger::yellow(" Configurations by " + filename);
	Logger::green("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

	// Since we're modifying values, we need to work with non-const references
	auto printValue = [](std::string& value, const std::string& label,
						const std::string& default_value = "", int padding = 20) {
		Logger::white(label, false, padding);
		if (value.empty()) {
			if (default_value.empty()) {
				Logger::black("[undefined]");
			} else {
				value = default_value;  // Set the default value
				Logger::black("[undefined (default: " + default_value + ")]");
			}
		} else {
			Logger::yellow(value);
		}
	};

	auto printIntValue = [](int& value, const std::string& label,
						int default_value = 0, int padding = 20) {
		Logger::white(label, false, padding);
		if (value == 0) {
			if (default_value == 0) {
				Logger::black("[undefined]");
			} else {
				value = default_value;  // Set the default value
				Logger::black("[undefined (default: " + std::to_string(default_value) + ")]");
			}
		} else {
			Logger::yellow(std::to_string(value));
		}
	};

	// Need to use non-const reference to modify the vector
	for (size_t i = 0; i < registeredServerConfs.size(); ++i) {
		ServerBlock& conf = registeredServerConfs[i];  // Note: non-const reference
		Logger::blue("\n  Server Block [" + std::to_string(i + 1) + "]:\n");

		// Server block defaults based on nginx
		printIntValue(conf.port, "    Port: ", 80);
		printValue(conf.name, "    Server Name: ", "localhost");
		printValue(conf.root, "    Root: ", "/usr/share/nginx/html");
		printValue(conf.index, "    Index: ", "index.html index.htm");
		printValue(conf.error_page, "    Error Page: ", "500 502 503 504 /50x.html");

		if (conf.locations.empty()) {
			Logger::yellow("  No Location Blocks.");
		} else {
			for (size_t j = 0; j < conf.locations.size(); ++j) {
				Location& location = conf.locations[j];  // Note: non-const reference
				Logger::cyan("\n    Location [" + std::to_string(j + 1) + "]:   " + location.path);

				// Location block defaults based on nginx
				printValue(location.methods, "      Methods: ", "GET HEAD POST");
				printValue(location.cgi, "      CGI: ");
				printValue(location.cgi_param, "      CGI Param: ");
				printValue(location.redirect, "      Redirect: ");
				printValue(location.autoindex, "      Autoindex: ", "off");
				printValue(location.return_directive, "      Return: ");
				printValue(location.default_file, "      Default File: ");
				printValue(location.upload_store, "      Upload Store: ");
				printValue(location.root, "      Root: ", conf.root);
				printValue(location.client_max_body_size, "      Client Max Body Size: ", "1m");
			}
		}
	}
	Logger::green("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
}

std::vector<ServerBlock> ConfigHandler::get_registeredServerConfs(void) const
{
	// Ensure there are registered configurations before returning
	if (registeredServerConfs.empty())
	{
		throw std::runtime_error("No registered configurations found!");
	}

	return registeredServerConfs; // Return all registered configurations
}

