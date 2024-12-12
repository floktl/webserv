/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   helper.cpp                                         :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/12/01 10:32:07 by fkeitel           #+#    #+#             */
/*   Updated: 2024/12/12 10:55:38 by jeberle          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

#include "helper.hpp"

// Helper function to set a socket to non-blocking mode
//  Purpose of Non-Blocking Mode
//	By default sockets in C/C++ are blocking. This means that if operation -
//		(like read, accept) cannot proceed immediately, program will pause -
//		(block) until the operation completes.
//	In non-blocking mode, socket returns control to program immediately -
//		if oper. can't proceed, allow program handle other tasks in meantime.

bool setNonBlocking(int fd) {
	int flags = fcntl(fd, F_GETFL, 0);
	if (flags == -1) return false;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK) != -1;
}
