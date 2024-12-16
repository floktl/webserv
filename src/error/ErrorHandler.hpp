/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   ErrorHandler.hpp                                   :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/13 07:15:29 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/16 15:59:35 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef ERRORHANDLER_HPP
#define ERRORHANDLER_HPP

#include "../main.hpp"

class ErrorHandler
{
	public:
		ErrorHandler(Server& server);
	private:
		Server& server;
};

#endif
