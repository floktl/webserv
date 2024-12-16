/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.hpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: fkeitel <fkeitel@student.42.fr>            +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/28 10:35:17 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/16 09:45:12 by fkeitel          ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# ifndef DEBUG
#  define DEBUG 1

#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstring>
#include <vector>
#include <map>
#include <string>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/wait.h>
# include <iostream>
# include <set>
# include <sstream>
# include <algorithm> // For std::transform
# include <sys/stat.h>
# include <dirent.h>		// für DIR, opendir, closedir
# include <stdexcept>
# include <fstream>
#include <chrono>
#include <iomanip>
#include <system_error>

#include "./structs/webserv.hpp"
#include "./utils/ConfigHandler.hpp"
#include "./utils/Logger.hpp"
#include "./utils/utils.hpp"
#include "./cgi/CgiHandler.hpp"
# include "./requests/RequestHandler.hpp"
# include "./utils/Logger.hpp"
# include "./server/Server.hpp"
# include "structs/webserv.hpp"

extern std::map<int, RequestState> g_requests;
extern std::map<int, int> g_fd_to_client;
extern int g_epfd;

# endif