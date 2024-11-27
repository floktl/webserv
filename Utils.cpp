/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Utils.cpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/27 11:28:08 by jeberle           #+#    #+#             */
/*   Updated: 2024/11/27 12:37:21 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./Utils.hpp"

Utils::Utils();
Utils::~Utils();
bool Utils::isConfigFile(const std::string& filepath);
bool Utils::addConfig(const FileConfData& fileInfo);

const char *Utils::InvalidFileNameException::what() const throw() {
	return "Invalid config file name";
}
const char *Utils::InvalidFileContentException::what() const throw() {
	return "Invalid file content";
}