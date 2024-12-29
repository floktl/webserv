#ifndef CONFIGHANDLER_HPP
# define CONFIGHANDLER_HPP

# ifndef CONFIG_OPTS
#  define CONFIG_OPTS "server,listen,server_name,root,index,error_page,location,client_max_body_size"
# endif

# ifndef LOCATION_OPTS
#  define LOCATION_OPTS "methods,return,root,autoindex,default_file,cgi,cgi_param,upload_store,client_max_body_size"
# endif

#include "../main.hpp"

struct ServerBlock;

class ConfigHandler {
private:
	char **env;
	int linecount;
	bool configFileValid;
	bool inServerBlock;
	bool inLocationBlock;
	bool parsingErr;
	std::string locBlockTar;
	std::vector<ServerBlock> registeredServerConfs;

public:
	ConfigHandler();
	~ConfigHandler();
	void parseLine(std::string line);
	void parseArgs(int argc, char **argv, char **envp);
	bool isConfigFile(const std::string& filepath);
	bool sanitizeConfData(void);
	bool parseConfigContent(std::string filename);
	bool getconfigFileValid(void) const;
	void printRegisteredConfs(std::string filename, std::string pwd);
	std::vector<ServerBlock> get_registeredServerConfs(void) const;

	class InvalidFileNameException : public std::exception {
	public:
		const char* what() const throw();
	};

	class InvalidFileContentException : public std::exception {
	public:
		const char* what() const throw();
	};
};

#endif