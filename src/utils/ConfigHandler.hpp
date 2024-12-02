/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ConfigHandler.hpp                                  :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/27 11:27:56 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/02 15:55:02 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef UTILS_HPP
# define UTILS_HPP

# include <string>
# include <vector>
# include <stdexcept>
# include <fstream>
# include "./Logger.hpp"
# include "./../main.hpp"
# include "./../server/Server.hpp"
# include "./../client/ClientHandler.hpp"
# include <set>
# include <sstream>
# include <algorithm>
# include <fstream>

struct ConfLocations {
	int			port;
	std::string path;
	std::string methods;
	std::string cgi;
	std::string cgi_param;
	std::string redirect;
};

struct FileConfData {
	int			port;
	int			server_fd;
	std::string name;
	std::string root;
	std::string index;
	std::string error_page;
	std::vector<ConfLocations> locations;
};

class ConfigHandler {
	private:
		char **env;
		int linecount;
		bool configFileValid;
		bool inServerBlock;
		bool inLocationBlock;
		bool parsingErr;
		std::string locBlockTar;
		std::vector<FileConfData> registeredConfs;
	public:
		ConfigHandler();
		~ConfigHandler();
		void parseLine(std::string line);
		void parseArgs(int argc, char **argv, char **envp);
		bool isConfigFile(const std::string& filepath);
		bool sanitizeConfData(void);
		bool parseConfigContent(std::string filename);
		bool getconfigFileValid(void) const;
		void printRegisteredConfs(std::string filename);
		std::vector<FileConfData> get_registeredConfs(void) const;

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