/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   helper.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/01 10:35:42 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/03 14:03:40 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HELPER_HPP
#define HELPER_HPP

#include "../server/Server.hpp"
#include <unistd.h>  // For getcwd
#include <limits.h>  // For PATH_MAX

void setNonBlocking(int fd);
void printCurrentWorkingDirectory(void);

#endif