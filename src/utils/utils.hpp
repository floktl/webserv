/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   utils.hpp                                          :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/16 09:18:24 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/16 09:25:41 by fkeitel          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#ifndef UTILS_HPP
#define UTILS_HPP

#include "../main.hpp"

int setNonBlocking(int fd);
void modEpoll(int epfd, int fd, uint32_t events);
void delFromEpoll(int epfd, int fd);

#endif