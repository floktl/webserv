#include "./ConfigHandler.hpp"

ConfigHandler::ConfigHandler()
{
	configFileValid = false;
	inServerBlock = false;
	inLocationBlock = false;
	linecount = 1;
	parsingErr = false;
	locBlockTar = "";
	env = NULL;
};

ConfigHandler::~ConfigHandler() {};

bool ConfigHandler::parseConfigContent(std::string filename)
{
	std::ifstream configFile(filename);
	if (!configFile.is_open())
	{
		Logger::error("Could not open config file");
		return false;
	}

	std::string line;
	while (std::getline(configFile, line))
	{
		line = trim(line);
		if (!line.empty())
		{
			parseLine(line);
			if (parsingErr)
			{
				configFile.close();
				return false;
			}
		}
		linecount++;
	}

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

bool ConfigHandler::isConfigFile(const std::string& filepath)
{
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
	{
		Logger::error() << "Usage: ./webserv [CONFIG_PATH]";
	}
}

bool ConfigHandler::getconfigFileValid(void) const
{
	return configFileValid;
}

std::vector<ServerBlock> ConfigHandler::get_registeredServerConfs(void) const
{
	if (registeredServerConfs.empty())
	{
		throw std::runtime_error("No registered configurations found!");
	}
	return registeredServerConfs;
}