#pragma once
# ifndef CONFIG_OPTS
#  define CONFIG_OPTS "server,listen,server_name,root,index,error_page,location,client_max_body_size, timeout"
# endif

# ifndef LOCATION_OPTS
#  define LOCATION_OPTS "methods,return,root,autoindex,default_file,cgi,upload_store,client_max_body_size"
# endif

#include "../server/server.hpp"

struct ServerBlock;
class Server;

class ConfigHandler
{
private:
// Core configuration variables
	Server						*server;
	char						**env;
	int							linecount;
	bool						configFileValid;
	bool						inServerBlock;
	bool						inLocationBlock;
	bool						parsingErr;
	std::string					locBlockTar;
	std::vector<ServerBlock>	registeredServerConfs;

	// Setup.cpp
	bool parseConfigContent(std::string filename);
	bool isConfigFile(const std::string& filepath);

	// debug.cpp
	void printRegisteredConfs(std::string filename);

	// Parseline.cpp
	bool parseErr(const std::string &str);
	bool handleServerBlock(void);
	bool handleLocationBlock(std::istringstream& stream);
	bool handleListenDirective(const std::string& value);
	bool handleErrorPage(const std::string& value);
	bool handleClientMaxBodySize(const std::string& value);
	bool handleTimeout(const std::string& value);
	bool handleLocationMethods(Location& currentLocation, const std::string& value);
	bool handleLocationReturn(Location& currentLocation, const std::string& value);
	bool handleLocationClientMaxBodySize(Location& currentLocation, const std::string& value);
	bool handleOpeningBrackets(std::string &keyword, std::istringstream &stream, std::string &line);
	bool handleClosingBrackets(void);
	bool extractAndExpandEnvVar(std::istringstream &stream, std::string &value);
	bool handleServerDirective(const std::string& keyword, const std::string& value);
	bool handleLocationDirective(const std::string& keyword, const std::string& value);

	// sanitize.cpp:
	bool confErr(const std::string &str);
	bool validateErrorPages();
	bool validateLocationConfigs(ServerBlock& serverConf, size_t serverIndex);
	bool sanitizeConfData(void);

	public:
	// de-/constructor
	ConfigHandler(Server *_server);
	~ConfigHandler();

	// setup.cpp
	std::vector<ServerBlock>	get_registeredServerConfs(void);
	bool						getconfigFileValid(void) const;
	void						parseArgs(int argc, char **argv, char **envp);

	// parseline.cpp
	bool parseLine(std::string line);
};

// utils.cpp
std::string					trim(const std::string& str);
std::vector<std::string>	parseOptionsToVector(const std::string& opts);
std::string					expandEnvironmentVariables(const std::string& value, char** env);