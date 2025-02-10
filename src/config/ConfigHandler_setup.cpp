#include "./ConfigHandler.hpp"

// Constructor: Initializes configuration-related variables
ConfigHandler::ConfigHandler()
{
	configFileValid = false;
	inServerBlock = false;
	inLocationBlock = false;
	linecount = 1;
	parsingErr = false;
	locBlockTar = "";
	env = NULL;
}

// Destructor: Default destructor (no dynamic allocation)
ConfigHandler::~ConfigHandler() {}

// Parses the content of the configuration file line by line
bool ConfigHandler::parseConfigContent(std::string filename)
{
	std::ifstream configFile(filename);
	if (!configFile.is_open())
	{
		return false;
	}
	std::string line;
	while (std::getline(configFile, line))
	{
		line = trim(line);
		if (!line.empty())
		{
			if (!parseLine(line))
			{
				if (parsingErr)
				{
					configFile.close();
					return false;
				}
			}
		}
		linecount++;
	}

	// Ensure all opened blocks are properly closed
	if (inServerBlock || inLocationBlock)
	{
		Logger::error("Error: Unclosed block at end of file");
		configFile.close();
		return false;
	}

	configFile.close();
	configFileValid = true;
	return true;
}

// Validates if a given file is a .conf config file and parses it
bool ConfigHandler::isConfigFile(const std::string& filepath)
{
	std::ifstream file(filepath.c_str());
	if (!file.good())
	{
		Logger::error("File does not exist");
		return false;
	}

	// Ensure the file extension is .conf
	if (filepath.size() >= 5 && filepath.substr(filepath.size() - 5) != ".conf")
	{
		Logger::error("File has to be of type .conf");
		return false;
	}

	// Parse and validate the config file
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

// Parses command-line arguments and registers the config file if provided
void ConfigHandler::parseArgs(int argc, char **argv, char **envp)
{
	if (argc == 2)
	{
		env = envp;
		std::string filepath(argv[1]);
		if (isConfigFile(filepath))
		{
			Logger::green("\nConfig " + filepath + " registered successfully!\n");
			printRegisteredConfs(filepath, expandEnvironmentVariables("$PWD", env));
		}

	}
	else
		Logger::error() << "Usage: ./webserv [CONFIG_PATH]";
}

// Returns whether the config file was successfully validated
bool ConfigHandler::getconfigFileValid(void) const
{
	return configFileValid;
}

// Retrieves registered server configurations and adds a default vhosts gate if necessary
std::vector<ServerBlock> ConfigHandler::get_registeredServerConfs()
{
	if (registeredServerConfs.empty())
	{
		throw std::runtime_error("No registered configurations found!");
	}

	// Check if any server block is using port 80 (default HTTP port)
	for (auto &conf : registeredServerConfs)
	{
		if (conf.port == 80)
			serverInstance->has_gate = true;
	}

	// Define a default vhosts gate if port 80 is not in use
	ServerBlock vhosts_gate;
	Location vhosts_gate_location;
	vhosts_gate.port = 80;
	vhosts_gate.name = "localhost";
	vhosts_gate.root = "/";

	// Minimal required fields for the location
	vhosts_gate_location.path = "/"; // Match all requests
	vhosts_gate_location.root = "/"; // Root path
	vhosts_gate_location.default_file = "index.html"; // Default file

	// Add the location to the server block
	vhosts_gate.locations.push_back(vhosts_gate_location);
	vhosts_gate.locations.push_back(vhosts_gate_location);

	// Register vhosts gate only if no other server is using port 80
	if (!serverInstance->has_gate)
		registeredServerConfs.push_back(vhosts_gate);

	return registeredServerConfs;
}
