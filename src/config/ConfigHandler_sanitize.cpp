#include "ConfigHandler.hpp"

// Marks The Config File as Invalid and Logs to Error IF A Message is Provided
bool ConfigHandler::confErr(const std::string &str = "")
{
	configFileValid = false;
	return str.empty() ? false : Logger::error(str);
}

// Validates Error Pages in Each Server Block, Ensuring Correct Format and Paths
bool ConfigHandler::validateErrorPages()
{
	for (size_t j = 0; j < registeredServerConfs.size(); ++j)
	{
		ServerBlock& conf = registeredServerConfs[j];
		std::map<int, std::string> errorPagesToValidate = conf.errorPages;

		for (std::map<int, std::string>::const_iterator it = errorPagesToValidate.begin();
			it != errorPagesToValidate.end(); ++it)
		{
			int code = it->first;
			std::string path = it->second;

			std::string normalizedPath = path;
			std::ostringstream errorPageDef;
			errorPageDef << code << " " << normalizedPath;

			std::string errorPageString = errorPageDef.str();
			if (!Sanitizer::sanitize_errorPage(errorPageString, expandEnvironmentVariables("$PWD", env)))
				return confErr("Error page config not valid for code: " + std::to_string(code));
			conf.errorPages[code] = normalizedPath;
		}
	}
	return true;
}

// Validates All Location Blocks Within A Server Block, Ensuring Correctness of Directives
bool ConfigHandler::validateLocationConfigs(ServerBlock& serverConf, size_t serverIndex)
{
	for (size_t j = 0; j < serverConf.locations.size(); ++j)
	{
		Location& loc = serverConf.locations[j];

		// Validate Location Path and Methods
		if ((!Sanitizer::sanitize_locationPath(loc.path, expandEnvironmentVariables("$PWD", env)))
			|| (!loc.methods.empty() && !Sanitizer::sanitize_locationMethods(loc.methods)))
			return confErr();

		// Validate Return (redirect) Directives
		if (!loc.return_code.empty())
		{
			int code = std::stoi(loc.return_code);
			if (code != 301 && code != 302)
				return confErr("Invalid redirect status code: " + loc.return_code);
			if (loc.return_url.empty())
				return confErr("Return directive requires a URL");
			if (loc.return_url[0] != '/' &&
				loc.return_url.substr(0, 7) != "http://" &&
				loc.return_url.substr(0, 8) != "https://")
				return confErr("Invalid return URL format");
		}

		// Validate Location Specific Directives
		if ((!loc.root.empty() && !Sanitizer::sanitize_locationRoot(loc.root, expandEnvironmentVariables("$PWD", env)))
			|| (!Sanitizer::sanitize_locationAutoindex(loc.autoindex))
			|| (!loc.default_file.empty() && !Sanitizer::sanitize_locationDefaultFile(loc.default_file))
			|| (!loc.cgi.empty() && !Sanitizer::sanitize_locationCgi(loc.cgi, loc.cgi_filetype, expandEnvironmentVariables("$PWD", env))))
			return confErr();

		// Set and validate client_max_body_size
		if (loc.client_max_body_size == -1)
			loc.client_max_body_size = serverConf.client_max_body_size;
		if (!Sanitizer::sanitize_clMaxBodSize(loc.client_max_body_size))
			return confErr("Invalid client_max_body_size for location in server block " + std::to_string(serverIndex + 1));

		// Validate upload_store directive
		if (!Sanitizer::sanitize_locationUploadStore(loc.upload_store, expandEnvironmentVariables("$PWD", env), serverConf.root, loc.root))
			return confErr();

		// Reset upload_store if cgi is enabled
		if (!loc.cgi.empty())
			loc.upload_store = "uploads";
	}
	return true;
}

// Sanitities and Validates All Server Configuration Data
bool ConfigHandler::sanitizeConfData(void)
{
	std::set<int> usedPorts;

	for (size_t i = 0; i < registeredServerConfs.size(); ++i)
	{
		// Ensure each server block has a valid port
		if (registeredServerConfs[i].port == 0)
			return confErr("Port number is mandatory for server block " + std::to_string(i + 1));
		if (registeredServerConfs[i].port == 0)
			return confErr("Port number is mandatory for server block " + std::to_string(i + 1));
		// Validate Ports and Ensure They are unique
		if (Sanitizer::sanitize_portNr(registeredServerConfs[i].port))
		{
			if (usedPorts.find(registeredServerConfs[i].port) != usedPorts.end())
			{
				Logger::magenta("Server Block " + std::to_string((i + 1))
					+ " ignored, because Port " + std::to_string(registeredServerConfs[i].port) + " already used.");
				registeredServerConfs.erase(registeredServerConfs.begin() + i);
				--i;
				continue;
			}
			else
				usedPorts.insert(registeredServerConfs[i].port);
		}
		else
			return confErr("Port number is not in system range on server block " + std::to_string(i + 1));

		// Validate root, server name, and index directives
		if (!Sanitizer::sanitize_root(registeredServerConfs[i].root, expandEnvironmentVariables("$PWD", env)) ||
			(!registeredServerConfs[i].name.empty() && !Sanitizer::sanitize_serverName(registeredServerConfs[i].name)) ||
			(!registeredServerConfs[i].index.empty() && !Sanitizer::sanitize_index(registeredServerConfs[i].index)))
			return confErr();

		// Validate and set default client_max_body_size
		if (registeredServerConfs[i].client_max_body_size == -1 ||
			!Sanitizer::sanitize_clMaxBodSize(registeredServerConfs[i].client_max_body_size))
			registeredServerConfs[i].client_max_body_size = DEFAULT_MAXBODYSIZE;

		if (registeredServerConfs[i].locations.empty())
			return confErr("Server block " + std::to_string(i + 1) + " must have at least one location block");
		if (!Sanitizer::sanitize_timeout(registeredServerConfs[i].timeout))
			return confErr("Invalid timeout value for server block " + std::to_string(i + 1));
		if (!validateLocationConfigs(registeredServerConfs[i], i))
			return false;
	}
	if (!validateErrorPages())
		return false;
	return true;
}
