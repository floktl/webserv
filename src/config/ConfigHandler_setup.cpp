#include "./ConfigHandler.hpp"

// Constructor: Initializes Configuration-related variables
ConfigHandler::ConfigHandler(Server *_server)
{
	server = _server;
	configFileValid = false;
	inServerBlock = false;
	inLocationBlock = false;
	linecount = 1;
	parsingErr = false;
	locBlockTar = "";
	env = NULL;
}

// Destructor: Default Destructor (No Dynamic Allocation)
ConfigHandler::~ConfigHandler() {}

// Parses the Content of the Configuration File Line by Line
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

// Ensure All Opened Blocks Are Properly Closed
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

// Parses Command-Line Argents and Register The Config File If Provided
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

// Returns Whether the Config File was successfully validated
bool ConfigHandler::getconfigFileValid(void) const
{
	return configFileValid;
}

// Retrieves Registered Server Configurations and Adds A Default Vhosts Gate IF Necessary
std::vector<ServerBlock> ConfigHandler::get_registeredServerConfs()
{
	if (registeredServerConfs.empty())
	{
		throw std::runtime_error("No registered configurations found!");
	}

// Check IF Any Server Block IS Using Port 80 (Default HTTP Port)
	for (auto &conf : registeredServerConfs)
	{
		if (conf.port == 80)
			server->has_gate = true;
	}

// Define a default vhosts gate if port 80 is not in use
	ServerBlock vhosts_gate;
	Location vhosts_gate_location;
	vhosts_gate.port = 80;
	vhosts_gate.name = "localhost";
	vhosts_gate.root = "/";

// Minimal Required Fields for the Location
	vhosts_gate_location.path = "/";
	vhosts_gate_location.root = "/";
	vhosts_gate_location.default_file = DEFAULT_FILE;

// Add the location to the server block
	vhosts_gate.locations.push_back(vhosts_gate_location);
	vhosts_gate.locations.push_back(vhosts_gate_location);

// Register VHOSTS GATE ONLY IF No Other Server is Using Port 80
	if (!server->has_gate)
		registeredServerConfs.push_back(vhosts_gate);

	return registeredServerConfs;
}
