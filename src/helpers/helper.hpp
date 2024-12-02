/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   helper.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/01 10:35:42 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/02 12:56:14 by fkeitel          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HELPER_HPP
#define HELPER_HPP

#include "../server/Server.hpp"
#include <unistd.h>  // For getcwd
#include <limits.h>  // For PATH_MAX

void setNonBlocking(int fd);
void printCurrentWorkingDirectory(void);
std::string getAbsolutePath(const std::string& root);

#endif