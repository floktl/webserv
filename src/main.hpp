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
#include <time.h>

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

#include <iostream>
#include <vector>
#include <string>
#include <iomanip> // For formattin
void printServerBlock(const ServerBlock& serverBlock);
void printRequestState(const RequestState& req);

# endif
