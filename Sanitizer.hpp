/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   Sanitizer.hpp                                      :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/28 15:27:25 by jeberle           #+#    #+#             */
/*   Updated: 2024/11/28 15:31:28 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */


#ifndef SANITIZER_HPP
# define SANITIZER_HPP

class Sanitizer {
	private:
	public:
		Sanitizer();
		~Sanitizer();
		bool sanitize_portNr(std::string portNr);
		bool sanitize_serverName(std::string serverName);
		bool sanitize_root(std::string root);
		bool sanitize_index(std::string index);
		bool sanitize_errorPage(std::string errorPage);
		bool sanitize_locationPath(std::string locationPath);
		bool sanitize_locationMethods(std::string locationMethods);
		bool sanitize_locationCgi(std::string locationCgi);
		bool sanitize_locationCgiParam(std::string locationCgiParam);
		bool sanitize_locationRedirect(std::string locationRedirect);
};

#endif