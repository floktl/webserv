/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   helper.hpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/01 10:35:42 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/12 10:56:03 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef HELPER_HPP
#define HELPER_HPP

#include "../server/Server.hpp"
#include <limits.h>  // For PATH_MAX

bool setNonBlocking(int fd);

#endif