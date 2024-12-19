/* ************************************************************************** */
/*                                                                            */
/*                                                        :::      ::::::::   */
/*   main.hpp                                           :+:      :+:    :+:   */
/*                                                    +:+ +:+         +:+     */
/*   By: jonathaneberle <jonathaneberle@student.    +#+  +:+       +#+        */
/*                                                +#+#+#+#+#+   +#+           */
/*   Created: 2024/11/28 10:35:17 by jeberle           #+#    #+#             */
/*   Updated: 2024/12/19 16:27:39 by jonathanebe      ###   ########.fr       */
/*                                                                            */
/* ************************************************************************** */

# ifndef MAIN_HPP
#  define MAIN_HPP

# ifndef DEBUG
#  define DEBUG 1
# endif

# ifndef CHUNK_SIZE
#  define CHUNK_SIZE 8192
# endif
 

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
# include <algorithm>
# include <sys/stat.h>
# include <dirent.h>
# include <stdexcept>
# include <fstream>
#include <chrono>
#include <iomanip>
#include <system_error>

#include "./structs/webserv.hpp"
#include "./config/ConfigHandler.hpp"
#include "./utils/Logger.hpp"
#include "./cgi/CgiHandler.hpp"
# include "./requests/RequestHandler.hpp"
# include "./utils/Sanitizer.hpp"
# include "./utils/Logger.hpp"
# include "./static/StaticHandler.hpp"
# include "./error/ErrorHandler.hpp"
# include "./server/Server.hpp"
# include "structs/webserv.hpp"

# endif
