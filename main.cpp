/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.cpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/26 12:58:21 by jeberle           #+#    #+#             */
/*   Updated: 2024/11/27 17:25:57 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "./Utils.hpp"
#include "./Logger.hpp"

int main(int argc, char **argv)
{
	Utils utils;

	utils.parseArgs(argc, argv);
	if (utils.getconfigFileValid())
	{
		Logger::yellow() << "WEBSERV starting ...";
	}
	return (0);
}
