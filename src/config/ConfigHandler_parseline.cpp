#include "./ConfigHandler.hpp"

// Logs A Parsing Error and Sets the Parsingerr Flag to True
bool ConfigHandler::parseErr(const std::string &str)
{
	parsingErr = true;
	return (Logger::error(str));
}



// Handles The Start of A Server Block, Ensuring No Nesting
bool ConfigHandler::handleServerBlock(void)
{
	if (inServerBlock)
		return parseErr("Error: Nested server block at line " + std::to_string(linecount));
	inServerBlock = true;
	registeredServerConfs.push_back(ServerBlock());
	return true;
}


// Handles The Start of A Location Block Within A Server Block
bool ConfigHandler::handleLocationBlock(std::istringstream& stream)
{
	if (!inServerBlock || inLocationBlock)
		return parseErr("Error: Invalid location block at line " + std::to_string(linecount));
	std::string location_path;
	stream >> location_path;

	std::string rest;
	std::getline(stream, rest);
	rest = trim(rest);

	if (rest != "{")
		return parseErr("Error: Location definition must be followed by its path and { on same line at line " + std::to_string(linecount));
	inLocationBlock = true;
	registeredServerConfs.back().locations.push_back(Location());
	Location& newLocation = registeredServerConfs.back().locations.back();
	newLocation.path = location_path;
	newLocation.root = registeredServerConfs.back().root;
	return true;
}


// Processes the 'Liste' Directive by Converting Its Value to an integer
bool ConfigHandler::handleListenDirective(const std::string& value)
{
	try
	{
		registeredServerConfs.back().port = std::stoi(value);
	}
	catch (const std::out_of_range& e)
	{
		return parseErr("Error: 'listen' value at line " + std::to_string(linecount) + " cannot be interpreted as a valid port");
	}
	return true;
}


// Parses and Validates the 'Error_Page' Directive
bool ConfigHandler::handleErrorPage(const std::string& value)
{
	std::istringstream iss(value);
	std::vector<std::string> tokens;
	std::string token;

	while (iss >> token)
		tokens.push_back(token);

	if (tokens.size() < 2)
		return parseErr("Error: Invalid error_page format at line " + std::to_string(linecount));
	std::string path = tokens.back();
	tokens.pop_back();
	for (const std::string &codeStr : tokens)
	{
		try
		{
			int code = std::stoi(codeStr);
			if (code < 400 || code > 599)
				return parseErr("Error: Invalid error code '" + codeStr + "' at line " + std::to_string(linecount));
			registeredServerConfs.back().errorPages[code] = path;
		}
		catch (...)
		{
			return parseErr("Error: Invalid error code '" + codeStr + "' at line " + std::to_string(linecount));
		}
	}
	return true;
}


// Parses and sets the 'client_max_body_size' Directive
bool ConfigHandler::handleClientMaxBodySize(const std::string& value)
{
	long size = Sanitizer::parseSize(value, "M");
	if (size == -1)
		return parseErr("Error: Invalid client_max_body_size at line " + std::to_string(linecount));
	registeredServerConfs.back().client_max_body_size = size;
	return true;
}


// Parses and sets the 'Timeout' Directive, Ensuring It's Within A Valid Range
bool ConfigHandler::handleTimeout(const std::string& value)
{
	try
	{
		int timeout = std::stoi(value);
		if (!Sanitizer::sanitize_timeout(timeout))
			return parseErr("Error: Invalid timeout value at line " + std::to_string(linecount) + " (must be between 10 and 120 seconds)");
		registeredServerConfs.back().timeout = timeout;
	}
	catch (...)
	{
		return parseErr("Error: Invalid timeout value at line " + std::to_string(linecount));
	}
	return true;
}


// Parses and Sets Allowed Http Methods for a Location
bool ConfigHandler::handleLocationMethods(Location& currentLocation, const std::string& value)
{
	currentLocation.allowGet = false;
	currentLocation.allowPost = false;
	currentLocation.allowDelete = false;

	currentLocation.methods = value;

	std::istringstream iss(value);
	std::string method;
	while (iss >> method) {
		if (method == "GET")
			currentLocation.allowGet = true;
		if (method == "POST")
			currentLocation.allowPost = true;
		if (method == "DELETE")
			currentLocation.allowDelete = true;
	}
	return true;
}


// Parses and Validates the 'Return' Directive for Location Redirection
bool ConfigHandler::handleLocationReturn(Location& currentLocation, const std::string& value)
{
	std::istringstream iss(value);
	std::string code, url;
	iss >> code;
	std::getline(iss >> std::ws, url);

	try
	{
		int status = std::stoi(code);
		if ((status != 301 && status != 302) || url.empty())
			return parseErr("Error: Invalid return directive at line " + std::to_string(linecount) + ". Format: return 301|302 URL;");
		currentLocation.return_code = code;
		currentLocation.return_url = url;
	}
	catch (...)
	{
		return parseErr("Error: Invalid return status code at line " + std::to_string(linecount));
	}
	return true;
}


// Parses and sets 'client_max_body_size' Specifically for location blocks
bool ConfigHandler::handleLocationClientMaxBodySize(Location& currentLocation, const std::string& value)
{
	long size = Sanitizer::parseSize(value, "M");
	if (size == -1)
		return parseErr("Error: Invalid client_max_body_size at line " + std::to_string(linecount));
	currentLocation.client_max_body_size = size;
	return true;
}


// Ensures Correct Usage of Opening Brackets for Server and Location Blocks
bool ConfigHandler::handleOpeningBrackets(std::string &keyword, std::istringstream &stream, std::string &line)
{
	if (line.find("}") != std::string::npos)
		return parseErr("Error: at line " + std::to_string(linecount) + " Only one scope per line allowed");

	if (keyword == "server")
		return handleServerBlock();
	else if (keyword == "location")
		return handleLocationBlock(stream);
	return (true);
}

// Ensures Correct Usage of Closing Brackets for Server and Location Blocks
bool ConfigHandler::handleClosingBrackets(void)
{
	if (!inServerBlock && !inLocationBlock)
		return parseErr("Error: Unexpected } at line " + std::to_string(linecount));
	if (inLocationBlock)
		inLocationBlock = false;
	else
		inServerBlock = false;
	return true;
}


// Expands Environment Variables Within A Given Value
bool ConfigHandler::extractAndExpandEnvVar(std::istringstream &stream, std::string &value)
{
	std::getline(stream, value);
	value = trim(value);
	if (value.empty() || value.back() != ';')
		return parseErr("Error: Missing semicolon at line " + std::to_string(linecount));
	value.pop_back();
	value = trim(value);
	value = expandEnvironmentVariables(value, env);
	return true;
}


// Processes Directives Related To The Server Block Configuration
bool ConfigHandler::handleServerDirective(const std::string& keyword, const std::string& value)
{
	static const std::vector<std::string> serverOpts = parseOptionsToVector(CONFIG_OPTS);

	if (std::find(serverOpts.begin(), serverOpts.end(), keyword) == serverOpts.end() || keyword == "server")
		return parseErr("Error: Invalid server directive '" + keyword + "' at line " + std::to_string(linecount));

	if (keyword == "listen")
		return handleListenDirective(value);
	else if (keyword == "server_name")
		registeredServerConfs.back().name = value;
	else if (keyword == "root")
		registeredServerConfs.back().root = value;
	else if (keyword == "index")
		registeredServerConfs.back().index = value;
	else if (keyword == "error_page")
		return handleErrorPage(value);
	else if (keyword == "client_max_body_size")
		return handleClientMaxBodySize(value);
	else if (keyword == "timeout")
		return handleTimeout(value);
	return true;
}


// Processes Directives Related To The Location Block Configuration
bool ConfigHandler::handleLocationDirective(const std::string& keyword, const std::string& value)
{
	static const std::vector<std::string>	locationOpts = parseOptionsToVector(LOCATION_OPTS);;

	if (std::find(locationOpts.begin(), locationOpts.end(), keyword) == locationOpts.end())
		return parseErr("Error: Invalid location directive '" + keyword + "' at line " + std::to_string(linecount));

	Location& currentLocation = registeredServerConfs.back().locations.back();
	if (keyword == "methods")
		return handleLocationMethods(currentLocation, value);
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
	else if (keyword == "return")
		return handleLocationReturn(currentLocation, value);
	else if (keyword == "client_max_body_size")
		return handleLocationClientMaxBodySize(currentLocation, value);

	return true;
}

// Parses A Single Line from the configuration File and Processes Its Directive
bool ConfigHandler::parseLine(std::string line)
{
	std::istringstream stream(line);
	std::string keyword, value;

	stream >> keyword;
	keyword = trim(keyword);
	line = trim(line);
	if (line.find("#") == 0)
		return false;
	if (line.find("{") != std::string::npos)
		return handleOpeningBrackets(keyword, stream, line);
	if (keyword == "}")
		return handleClosingBrackets();
	if (!inServerBlock)
		return parseErr("Error: Configuration outside server block at line " + std::to_string(linecount));
	if (!extractAndExpandEnvVar(stream, value))
		return false;
	if (!inLocationBlock)
		return handleServerDirective(keyword, value);
	else
		return handleLocationDirective(keyword, value);
}
