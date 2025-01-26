#include "ConfigHandler.hpp"

bool ConfigHandler::sanitizeConfData(void)
{
	std::set<int> usedPorts;
	for (size_t i = 0; i < registeredServerConfs.size(); ++i) {
		if (registeredServerConfs[i].port == 0) {
			Logger::error("Port number is mandatory for server block " + std::to_string(i + 1));
			configFileValid = false;
			return false;
		}
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

		if (!Sanitizer::sanitize_root(registeredServerConfs[i].root, expandEnvironmentVariables("$PWD", env))) {
			configFileValid = false;
			return false;
		}

		if (!registeredServerConfs[i].name.empty() &&
			!Sanitizer::sanitize_serverName(registeredServerConfs[i].name)) {
			configFileValid = false;
			return false;
		}

		if (!registeredServerConfs[i].index.empty() &&
			!Sanitizer::sanitize_index(registeredServerConfs[i].index)) {
			configFileValid = false;
			return false;
		}

		if (registeredServerConfs[i].client_max_body_size == 0) {
			registeredServerConfs[i].client_max_body_size = 1048576;
		} //else if (!Sanitizer::sanitize_clMaxBodSize(registeredServerConfs[i].client_max_body_size)) {
		//	Logger::red("Invalid client_max_body_size for server block " + std::to_string(i + 1));
		//	configFileValid = false;
		//	return false;
		//}

		for (size_t j = 0; j < registeredServerConfs.size(); ++j) {
			ServerBlock& conf = registeredServerConfs[j];
			std::map<int, std::string> errorPagesToValidate = conf.errorPages;

			for (std::map<int, std::string>::const_iterator it = errorPagesToValidate.begin();
				it != errorPagesToValidate.end(); ++it) {
				int code = it->first;
				std::string path = it->second;

				std::string normalizedPath = path;
				std::ostringstream errorPageDef;
				errorPageDef << code << " " << normalizedPath;

				std::string errorPageString = errorPageDef.str();
				if (!Sanitizer::sanitize_errorPage(errorPageString, expandEnvironmentVariables("$PWD", env))) {
					Logger::red("Error page config not valid for code: " + std::to_string(code));
					configFileValid = false;
					return false;
				}
				conf.errorPages[code] = normalizedPath;
			}
		}

		if (registeredServerConfs[i].locations.empty()) {
			Logger::error("Server block " + std::to_string(i + 1) + " must have at least one location block");
			configFileValid = false;
			return false;
		}

		if (!Sanitizer::sanitize_timeout(registeredServerConfs[i].timeout)) {
			Logger::error("Invalid timeout value for server block " + std::to_string(i + 1));
			configFileValid = false;
			return false;
		}

		for (size_t j = 0; j < registeredServerConfs[i].locations.size(); ++j) {
			Location& loc = registeredServerConfs[i].locations[j];

			if (!Sanitizer::sanitize_locationPath(loc.path, expandEnvironmentVariables("$PWD", env))) {
				configFileValid = false;
				return false;
			}

			if (!loc.methods.empty() &&
				!Sanitizer::sanitize_locationMethods(loc.methods)) {
				configFileValid = false;
				return false;
			}

			if (!loc.return_code.empty()) {
				int code = std::stoi(loc.return_code);
				if (code != 301 && code != 302) {
					Logger::error("Invalid redirect status code: " + loc.return_code);
					configFileValid = false;
					return false;
				}

				if (loc.return_url.empty()) {
					Logger::error("Return directive requires a URL");
					configFileValid = false;
					return false;
				}

				if (loc.return_url[0] != '/' &&
					loc.return_url.substr(0, 7) != "http://" &&
					loc.return_url.substr(0, 8) != "https://") {
					Logger::error("Invalid return URL format");
					configFileValid = false;
					return false;
				}
			}

			if (!loc.root.empty() &&
				!Sanitizer::sanitize_locationRoot(loc.root, expandEnvironmentVariables("$PWD", env))) {
				configFileValid = false;
				return false;
			}

			if (!Sanitizer::sanitize_locationAutoindex(loc.autoindex, loc.doAutoindex)) {
				configFileValid = false;
				return false;
			}

			if (!loc.default_file.empty() &&
				!Sanitizer::sanitize_locationDefaultFile(loc.default_file)) {
				configFileValid = false;
				return false;
			}


			if (!loc.cgi.empty() &&
				!Sanitizer::sanitize_locationCgi(loc.cgi, loc.cgi_filetype, expandEnvironmentVariables("$PWD", env))) {
				configFileValid = false;
				return false;
			}

			if (loc.client_max_body_size == 0) {
				loc.client_max_body_size = registeredServerConfs[i].client_max_body_size;
			} else {
				if (!Sanitizer::sanitize_clMaxBodSize(loc.client_max_body_size)) {
					Logger::red("Invalid client_max_body_size for location in server block " + std::to_string(i + 1));
					configFileValid = false;
					return false;
				}
			}

			if (loc.client_max_body_size == 0) {
				loc.client_max_body_size = 1048576;
			}
			if (!Sanitizer::sanitize_locationUploadStore(
					loc.upload_store,
					expandEnvironmentVariables("$PWD", env),
					registeredServerConfs[i].root,
					loc.root)) {
				configFileValid = false;
				return false;
			}
			if (!loc.cgi.empty())
			{
				loc.upload_store = "";
			}
		}
	}
	return true;
}
