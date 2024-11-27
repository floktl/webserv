/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Utils.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/27 11:27:56 by jeberle           #+#    #+#             */
/*   Updated: 2024/11/27 12:16:40 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef UTILS_HPP
# define UTILS_HPP

# include <string>
# include <vector>
# include <stdexcept>

struct FileConfData {
	std:string path;
}

class Utils {
	private:
		std::vector<FileConfData> registeredConfs;
	public:
		Utils();
		~Utils();
		bool isConfigFile(const std::string& filepath);
		bool addConfig(const FileConfData& fileInfo);

		const char *Utils::InvalidFileNameException::what() const throw() {
			return "Invalid config file name";
		}

		const char *Utils::InvalidFileContentException::what() const throw() {
			return "Invalid file content";
		}

#endif