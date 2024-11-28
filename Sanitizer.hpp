/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Sanitizer.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/28 15:27:25 by jeberle           #+#    #+#             */
/*   Updated: 2024/11/28 16:58:06 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#ifndef SANITIZER_HPP
# define SANITIZER_HPP

# include "./main.hpp"
# include "./Logger.hpp"
# include <set>
# include <vector>
# include <sstream>

class Sanitizer {
	private:
	public:
		Sanitizer();
		~Sanitizer();
		static bool sanitize_portNr(int portNr);
		static bool sanitize_serverName(std::string serverName);
		static bool sanitize_root(std::string root);
		static bool sanitize_index(std::string index);
		static bool sanitize_errorPage(std::string errorPage);
		static bool sanitize_locationPath(std::string locationPath);
		static bool sanitize_locationMethods(std::string locationMethods);
		static bool sanitize_locationCgi(std::string locationCgi);
		static bool sanitize_locationCgiParam(std::string locationCgiParam);
		static bool sanitize_locationRedirect(std::string locationRedirect);
};

#endif