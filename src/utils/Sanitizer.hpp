/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Sanitizer.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/28 15:27:25 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/16 09:32:34 by fkeitel          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#ifndef SANITIZER_HPP
# define SANITIZER_HPP

# include "./../main.hpp"

class Sanitizer {
	private:
		static bool isValidPath(std::string& path, const std::string& context, const std::string& pwd);
		static long parseSize(const std::string& sizeStr, const std::string& unit);
		static bool isValidFilename(const std::string& filename, bool allowPath);
	public:
		Sanitizer();
		~Sanitizer();
		static bool sanitize_portNr(int portNr);
		static bool sanitize_serverName(std::string& serverName);
		static bool sanitize_root(std::string& root, const std::string& pwd);
		static bool sanitize_index(std::string& index);
		static bool sanitize_errorPage(std::string& errorPage, const std::string& pwd);
		static bool sanitize_clMaxBodSize(std::string& clMaxBodSize);
		static bool sanitize_locationPath(std::string& locationPath, const std::string& pwd);
		static bool sanitize_locationMethods(std::string& locationMethods);
		static bool sanitize_locationReturn(std::string& locationReturn);
		static bool sanitize_locationRoot(std::string& locationRoot, const std::string& pwd);
		static bool sanitize_locationAutoindex(std::string& locationAutoindex);
		static bool sanitize_locationDefaultFile(std::string& locationDefaultFile);
		static bool sanitize_locationUploadStore(std::string& locationUploadStore, const std::string& pwd);
		static bool sanitize_locationClMaxBodSize(std::string& locationClMaxBodSize);
		static bool sanitize_locationCgi(std::string& locationCgi, const std::string& pwd);
		static bool sanitize_locationCgiParam(std::string& locationCgiParam);
		static bool sanitize_locationRedirect(std::string& locationRedirect);
};

#endif