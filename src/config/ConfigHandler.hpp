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
	Server *server;
	char **env;
	int linecount;
	bool configFileValid;
	bool inServerBlock;
	bool inLocationBlock;
	bool parsingErr;
	std::string locBlockTar;
	std::vector<ServerBlock> registeredServerConfs;

	// **Setup.cpp: Constructor, Destructor, and Core Config Handling**
	bool parseConfigContent(std::string filename);
	bool isConfigFile(const std::string& filepath);
	void printRegisteredConfs(std::string filename, std::string pwd);

	// **ParseLine.cpp: Parsing and Directive Handling**
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

	// **Sanitize.cpp: Configuration Validation**
	bool confErr(const std::string &str);
	bool sanitizeConfData(void);
	bool validateErrorPages();
	bool validateLocationConfigs(ServerBlock& serverConf, size_t serverIndex);

public:
	// **Public API**
	ConfigHandler(Server *_server);
	~ConfigHandler();

	std::vector<ServerBlock> get_registeredServerConfs(void);
	bool getconfigFileValid(void) const;
	void parseArgs(int argc, char **argv, char **envp);
	bool parseLine(std::string line);

	// **Exceptions**
	class InvalidFileNameException : public std::exception {
	public:
		const char* what() const throw();
	};

	class InvalidFileContentException : public std::exception {
	public:
		const char* what() const throw();
	};
};
