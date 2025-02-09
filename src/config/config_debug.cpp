#include "./ConfigHandler.hpp"

void ConfigHandler::printRegisteredConfs(std::string filename, std::string pwd)
{
	// map to store default error page paths
	std::map<int, std::string> errorPageDefaults;

	// scan the directory for error page files
	std::string errorPageDir = pwd + "/err_page_defaults";

	DIR *dir = opendir(errorPageDir.c_str());
	if (dir != NULL)
	{
		struct dirent *entry;
		while ((entry = readdir(dir)) != NULL)
		{
			// skip "." and ".."
			if (std::string(entry->d_name) == "." || std::string(entry->d_name) == "..")
			{
				continue;
			}
			// check if regular file
			{
				std::string fileName = entry->d_name;
				size_t dotPos = fileName.find('.');
				if (dotPos != std::string::npos)
				{
					try
					{
						int errorCode = std::stoi(fileName.substr(0, dotPos));
						std::string fullPath = errorPageDir + "/" + fileName;
						errorPageDefaults[errorCode] = fullPath;
					}
					catch (const std::exception& e)
					{
						continue;
					}
				}
			}
		}
		closedir(dir);
	}

	if (registeredServerConfs.empty()) {
		Logger::yellow("No configurations registered.");
		return;
	}

	Logger::green("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");
	Logger::yellow(" Configurations by " + filename);
	Logger::green("~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~");

	auto printValue = [](std::string& value, const std::string& label,
						const std::string& default_value = "", int padding = 30) {
		Logger::white(label, false, padding);
		if (value.empty()) {
			if (default_value.empty()) {
				Logger::black("[undefined]");
			} else {
				value = default_value;
				Logger::black("[undefined (default: " + default_value + ")]");
			}
		} else {
			Logger::yellow(value);
		}
	};

	auto printIntValue = [](int& value, const std::string& label,
						int default_value = 0, int padding = 30)
						{
		Logger::white(label, false, padding);
		if (value == 0)
		{
			if (default_value == 0)
			{
				Logger::black("[undefined]");
			}
			else{
				value = default_value;
				Logger::black("[undefined (default: " + std::to_string(default_value) + ")]");
			}
		} else {
			Logger::yellow(std::to_string(value));
		}
	};

	auto printSize = [](long long& value, const std::string& label,
					const std::string& default_value = "1m", int padding = 30) {
		Logger::white(label, false, padding);

		auto formatSize = [](long size) -> std::string {
			const long K = 1024L;
			const long M = K * 1024L;
			const long G = M * 1024L;

			if (size >= G) {
				return std::to_string(size / G) + "G";
			} else if (size >= M) {
				return std::to_string(size / M) + "M";
			} else if (size >= K) {
				return std::to_string(size / K) + "K";
			}
			return std::to_string(size);
		};

		auto parseSize = [](const std::string& sizeStr) -> long {
			if (sizeStr.empty()) return 0;

			// Extract numeric part and unit part
			std::string numStr = sizeStr;
			std::string unit;

			for (size_t i = 0; i < sizeStr.length(); ++i) {
				if (!std::isdigit(sizeStr[i])) {
					numStr = sizeStr.substr(0, i);
					unit = sizeStr.substr(i);
					break;
				}
			}

			long size;
			try {
				size = std::stol(numStr);
			} catch (...) {
				return 0;
			}

			// Convert unit to uppercase for comparison
			std::transform(unit.begin(), unit.end(), unit.begin(), ::toupper);

			if (unit == "K" || unit == "KB") return size * 1024L;
			if (unit == "M" || unit == "MB") return size * 1024L * 1024L;
			if (unit == "G" || unit == "GB") return size * 1024L * 1024L * 1024L;

			return size;
		};

		if (value == 0) {
			if (default_value.empty()) {
				Logger::black("[undefined]");
			} else {
				value = parseSize(default_value);
				Logger::black("[undefined (default: " + default_value + ")]");
			}
		} else {
			Logger::yellow(formatSize(value));
		}
	};

	for (size_t i = 0; i < registeredServerConfs.size(); ++i) {
		ServerBlock& conf = registeredServerConfs[i];
		Logger::blue("\n  Server Block [" + std::to_string(i + 1) + "]:\n");

		printIntValue(conf.port, "    Port: ", 80);
		printValue(conf.name, "    Server Name: ", "localhost");
		printValue(conf.root, "    Root: ", "/usr/share/nginx/html");
		printValue(conf.index, "    Index: ", "index.html");
		printSize(conf.client_max_body_size, "    Client Max Body Size: ", "1m");
		printIntValue(conf.timeout, "    Timeout: ", 30);

		// combined staged error page logic
		if (conf.errorPages.empty()) {
			// add all provided error pages serverside
			for (const auto& errorPage : errorPageDefaults) {
				conf.errorPages[errorPage.first] = errorPage.second;
			}

			// fallback for common error codes
			const int defaultErrorCodes[] = {400, 401, 403, 404, 500, 502, 503, 504};
			for (int errorCode : defaultErrorCodes) {
				if (conf.errorPages.find(errorCode) == conf.errorPages.end()) {
					conf.errorPages[errorCode] = "/50x.html";
				}
			}

			Logger::yellow("    Using default error pages:");
		}

		// show error pages
		for (std::map<int, std::string>::const_iterator it = conf.errorPages.begin();
			it != conf.errorPages.end(); ++it) {
			Logger::white("    Error Page: ", false, 30);
			Logger::yellow("Error " + std::to_string(it->first) + " -> " + it->second);
		}

		if (conf.locations.empty()) {
			Logger::yellow("  No Location Blocks.");
		} else {
			for (size_t j = 0; j < conf.locations.size(); ++j) {
				Location& location = conf.locations[j];
				Logger::cyan("\n    Location [" + std::to_string(j + 1) + "]:   " + location.path);

				printValue(location.methods, "      Methods: ", "GET HEAD POST");
				printValue(location.cgi, "      CGI: ");
				printValue(location.return_code, "      Redirect Code: ");
				printValue(location.return_url, "      Redirect Url: ");
				printValue(location.autoindex, "      Autoindex: ", "off");
				printValue(location.default_file, "      Default File: ");
				printValue(location.upload_store, "      Upload Store: ");
				printValue(location.root, "      Root: ", conf.root);
				printSize(location.client_max_body_size, "      Client Max Body Size: ", "1m");
			}
		}
	}
	Logger::green("\n~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~\n");
}
