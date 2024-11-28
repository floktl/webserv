/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Utils.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/27 11:27:56 by jeberle           #+#    #+#             */
/*   Updated: 2024/11/28 12:54:53 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef UTILS_HPP
# define UTILS_HPP

# include <string>
# include <vector>
# include <stdexcept>
# include <fstream>
# include "./Logger.hpp"
# include "./main.hpp"
# include <set>
# include <sstream>
# include <algorithm>
# include <fstream>

struct ConfLocations {
	int port;
	std::string methods;
	std::string cgi;
	std::string cgi_param;
	std::string redirect;
};

struct FileConfData {
	int port;
	std::string name;
	std::string root;
	std::string index;
	std::string error_page;
	std::vector<ConfLocations> locations;
};

class Utils {
	private:
		int linecount;
		bool configFileValid;
		bool inServerBlock;
		bool inLocationBlock;
		bool parsingErr;
		std::string locBlockTar;
		std::vector<FileConfData> registeredConfs;
	public:
		Utils();
		~Utils();
		void parseLine(std::string line);
		void parseArgs(int argc, char **argv);
		bool isConfigFile(const std::string& filepath);
		bool parseConfigContent(std::string filename);
		bool getconfigFileValid(void) const;
		void printRegisteredConfs(std::string filename);

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