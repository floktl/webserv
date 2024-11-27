/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Utils.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/27 11:27:56 by jeberle           #+#    #+#             */
/*   Updated: 2024/11/27 16:05:29 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef UTILS_HPP
# define UTILS_HPP

# include <string>
# include <vector>
# include <stdexcept>
# include <fstream>
# include "./Logger.hpp"

struct FileConfData {
	std::string path;
};

class Utils {
	private:
		std::vector<FileConfData> registeredConfs;
	public:
		Utils();
		~Utils();
		void parseArgs(int argc, char **argv);
		bool isConfigFile(const std::string& filepath);
		bool addConfig(const FileConfData& fileInfo);

		class InvalidFileNameException : public std::exception {
			public:
				const char* what() const throw(); // Exception-Methode korrekt deklariert
		};

		class InvalidFileContentException : public std::exception {
			public:
				const char* what() const throw();
		};
};

#endif