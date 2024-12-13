/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ErrorHandler.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 07:15:29 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/13 07:37:13 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ERRORHANDLER_HPP
#define ERRORHANDLER_HPP

#include <map>
#include <string>

class ErrorHandler
{
private:
    int client_fd;
    const std::map<int, std::string>& errorPages;

public:
    ErrorHandler(int client_fd, const std::map<int, std::string>& errorPages);
    void sendErrorResponse(int statusCode, const std::string& message);
};

#endif
