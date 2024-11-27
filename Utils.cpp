/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/27 11:28:08 by jeberle           #+#    #+#             */
/*   Updated: 2024/11/27 16:16:40 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./Utils.hpp"

Utils::Utils() {};
Utils::~Utils() {};

bool Utils::isConfigFile(const std::string& filepath) {
	std::ifstream file(filepath.c_str());
	return file.good();
}

bool Utils::addConfig(const FileConfData& fileInfo) {
	for (std::vector<FileConfData>::const_iterator it = registeredConfs.begin(); it != registeredConfs.end(); ++it) {
		if (it->path == fileInfo.path) {
			std::cerr << "Duplicate config file: " << fileInfo.path << std::endl;
			return false;
		}
	}
	registeredConfs.push_back(fileInfo);
	return true;
}

void Utils::parseArgs(int argc, char **argv) {
	for (int i = 1; i < argc; ++i) {
		std::string filepath(argv[i]);
		if (isConfigFile(filepath)) {
			FileConfData fileData = {filepath};
			if (addConfig(fileData)) {
				Logger::green() << "Config added: " << filepath;
			} else {
				Logger::error() << "Failed to add config: " << filepath;
			}
		} else {
			Logger::error() << "Invalid config file: " << filepath;
		}
	}
}

const char* Utils::InvalidFileNameException::what() const throw() {
	return "Invalid config file name";
}

const char* Utils::InvalidFileContentException::what() const throw() {
	return "Invalid file content";
}