#include "ConfigHandler.hpp"

void ConfigHandler::parseLine(std::string line)
{
	static const std::vector<std::string> serverOpts =
		parseOptionsToVector(CONFIG_OPTS);
	static const std::vector<std::string> locationOpts =
		parseOptionsToVector(LOCATION_OPTS);

	std::istringstream stream(line);
	std::string keyword, value;

	stream >> keyword;
	keyword = trim(keyword);
	line = trim(line);

	if (line.find("#") == 0)
	{
		return;
	}

	if (line.find("{") != std::string::npos)
	{
		if (line.find("}") != std::string::npos)
		{
			Logger::error("Error: at line " + std::to_string(linecount)
				+ " Only one scope per line allowed");
			parsingErr = true;
			return;
		}

		if (keyword == "server")
		{
			if (inServerBlock)
			{
				Logger::error("Error: Nested server block at line "
					+ std::to_string(linecount));
				parsingErr = true;
				return;
			}
			inServerBlock = true;
			registeredServerConfs.push_back(ServerBlock());
			return;
		}
		else if (keyword == "location")
		{
			if (!inServerBlock || inLocationBlock)
			{
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

			if (rest != "{")
			{
				Logger::error("Error: Location definition must be followed by \
					its path and { on same line at line " + std::to_string(linecount));
				parsingErr = true;
				return;
			}

			inLocationBlock = true;
			registeredServerConfs.back().locations.push_back(Location());
			Location& newLocation = registeredServerConfs.back().locations.back();
			newLocation.path = location_path;
			newLocation.root = registeredServerConfs.back().root;
			return;
		}
	}

	if (keyword == "}")
	{
		if (!inServerBlock && !inLocationBlock)
		{
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

	if (!inServerBlock)
	{
		Logger::error("Error: Configuration outside server block at line "
			+ std::to_string(linecount));
		parsingErr = true;
		return;
	}

	std::getline(stream, value);
	value = trim(value);
	if (value.empty() || value.back() != ';')
	{
		Logger::error("Error: Missing semicolon at line "
			+ std::to_string(linecount));
		parsingErr = true;
		return;
	}
	value.pop_back();
	value = trim(value);
	value = expandEnvironmentVariables(value, env);

	if (!inLocationBlock)
	{
		if (std::find(serverOpts.begin(), serverOpts.end(), keyword)
			== serverOpts.end() || keyword == "server")
		{
			Logger::error("Error: Invalid server directive '"
				+ keyword + "' at line " + std::to_string(linecount));
			parsingErr = true;
			return;
		}

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
		else if (keyword == "error_page") {
			std::istringstream iss(value);
			std::vector<std::string> tokens;
			std::string token;

			while (iss >> token) {
				tokens.push_back(token);
			}

			if (tokens.size() < 2) {
				Logger::error("Error: Invalid error_page format at line " + std::to_string(linecount));
				parsingErr = true;
				return;
			}

			std::string path = tokens.back();
			tokens.pop_back();

			for (const std::string &codeStr : tokens) {
				try {
					int code = std::stoi(codeStr);
					if (code < 400 || code > 599) {
						Logger::error("Error: Invalid error code '" + codeStr + "' at line " + std::to_string(linecount));
						parsingErr = true;
						return;
					}
					registeredServerConfs.back().errorPages[code] = path;
				} catch (...) {
					Logger::error("Error: Invalid error code '" + codeStr + "' at line " + std::to_string(linecount));
					parsingErr = true;
					return;
				}
			}
		} else if (keyword == "client_max_body_size") {
		    long size = Sanitizer::parseSize(value, "M");
		    if (size == -1) {
		        Logger::error("Error: Invalid client_max_body_size at line " + std::to_string(linecount));
		        parsingErr = true;
		        return;
		    }
		    Logger::file("Server Parsed size: " + std::to_string(size));
		    registeredServerConfs.back().client_max_body_size = size;
		} else if (keyword == "timeout") {
			try {
				int timeout = std::stoi(value);
				if (!Sanitizer::sanitize_timeout(timeout)) {
					Logger::error("Error: Invalid timeout value at line " + std::to_string(linecount)
						+ " (must be between 10 and 120 seconds)");
					parsingErr = true;
					return;
				}
				registeredServerConfs.back().timeout = timeout;
			} catch (...) {
				Logger::error("Error: Invalid timeout value at line "
					+ std::to_string(linecount));
				parsingErr = true;
				return;
			}
		}
	}
	else {
		if (std::find(locationOpts.begin(), locationOpts.end(), keyword) == locationOpts.end()) {
			Logger::error("Error: Invalid location directive '" + keyword + "' at line " + std::to_string(linecount));
			parsingErr = true;
			return;
		}

		Location& currentLocation = registeredServerConfs.back().locations.back();
		if (keyword == "methods") {
			currentLocation.allowGet = false;
			currentLocation.allowPost = false;
			currentLocation.allowDelete = false;
			currentLocation.allowCookie = false;

			currentLocation.methods = value;

			std::istringstream iss(value);
			std::string method;
			while (iss >> method) {
				if (method == "GET") currentLocation.allowGet = true;
				if (method == "POST") currentLocation.allowPost = true;
				if (method == "DELETE") {currentLocation.allowDelete = true;
				;}
				if (method == "COOKIE") currentLocation.allowCookie = true;
			}
		}
		else if (keyword == "cgi")
			currentLocation.cgi = value;
		else if (keyword == "autoindex")
			currentLocation.autoindex = value;
		else if (keyword == "default_file")
			currentLocation.default_file = value;
		else if (keyword == "root")
			currentLocation.root = value;
		else if (keyword == "upload_store")
			currentLocation.upload_store = value;
		else if (keyword == "return") {
			std::istringstream iss(value);
			std::string code, url;

			iss >> code;

			std::getline(iss >> std::ws, url);

			try {
				int status = std::stoi(code);
				if ((status != 301 && status != 302) || url.empty()) {
					Logger::error("Error: Invalid return directive at line "
						+ std::to_string(linecount) + ". Format: return 301|302 URL;");
					parsingErr = true;
					return;
				}
				currentLocation.return_code = code;
				currentLocation.return_url = url;
			} catch (...) {
				Logger::error("Error: Invalid return status code at line "
					+ std::to_string(linecount));
				parsingErr = true;
				return;
			}
		} else if (keyword == "client_max_body_size") {
			long size = Sanitizer::parseSize(value, "M");
			if (size == -1) {
				Logger::error("Error: Invalid client_max_body_size at line " + std::to_string(linecount));
				parsingErr = true;
				return;
			}
			Logger::file("Location Parsed size: " + std::to_string(size));
			currentLocation.client_max_body_size = size;
		}
	}
}
