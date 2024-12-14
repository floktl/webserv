/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigHandler.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/27 11:27:56 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/14 17:34:12 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef CONFIGHANDLER_HPP
# define CONFIGHANDLER_HPP

# ifndef CONFIG_OPTS
#  define CONFIG_OPTS "server,listen,server_name,root,index,error_page,location,client_max_body_size"
# endif

# ifndef LOCATION_OPTS
#  define LOCATION_OPTS "methods,return,root,autoindex,default_file,cgi,cgi_param,upload_store,client_max_body_size"
# endif

# include "../structs/webserv.hpp"
# include <string>
# include <vector>
# include <map>
# include <stdexcept>
# include <fstream>
# include "./Logger.hpp"
# include "./../main.hpp"
# include "./../server/Server.hpp"
# include "./../requests/RequestHandler.hpp"
# include <set>
# include <sstream>
# include <algorithm>
# include <dirent.h>
# include <iostream>

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