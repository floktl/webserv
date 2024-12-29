/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ErrorHandler.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 07:15:29 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/29 14:58:03 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ERRORHANDLER_HPP
#define ERRORHANDLER_HPP

#include "../main.hpp"

class Server; // Forward declaration to avoid circular dependency

class ErrorHandler
{
public:
	ErrorHandler(Server& server); // Constructor takes Server reference
	~ErrorHandler();

	// Function to generate the error response
	std::string generateErrorResponse(int statusCode, const std::string& message, RequestState &req) const;

private:
	Server& server; // Private link to Server
	std::string loadErrorPage(const std::string& filePath) const;
};

#endif // ERRORHANDLER_HPP
