#------------------------------------------------------------------------------#
#--------------                       PRINT                       -------------#
#------------------------------------------------------------------------------#

BLACK := \033[90m
RED := \033[31m
GREEN := \033[32m
YELLOW := \033[33m
BLUE := \033[34m
MAGENTA := \033[35m
CYAN := \033[36m
X := \033[0m

SUCCESS := \n\
$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)\n\
$(X)\n\
█     █  ███████  ██████   ███████  ███████  ███████  █     █$(X)\n\
█     █  █        █     █  █        █        █     █  █     █$(X)\n\
█  █  █  ███████  ██████   ███████  ███████  ███████   █   █ $(X)\n\
█ █ █ █  █        █     █        █  █        █   █      █ █  $(X)\n\
$(BLACK)_$(X)█   █   ███████  ██████   ███████  ███████  █    ██     █   $(X)\n\
$(X) Wir muessen noch alle config werte im req handler abfangen!!!!!!\n\
$(X) Checking the value of errno is strictly forbidden after a read or a write operation\n\
$(X) You can use every macro and define like FD_SET, FD_CLR, FD_ISSET, FD_ZERO (understanding what and how they do it is very useful).\n\
$(X) Forbidden functions.\n\
$(X) 🐑 Clients must be able to upload files. You need at least GET, POST, and DELETE methods FOR STATIC FILES HTML\n\
$(X) 🐑 Limit client body size.\n\
$(X) 🐑 Timeout bei langer dauer der Processes CGIU e g PHP infinty while !!!!!!\n\
$(X) 🐑 Upload Files:    Make the route able to accept uploaded files and configure where they should be saved. chunked!!!!!!\n\
$(X) Stress test shell script das de kiste fickt!!!!!!!!!\n\
$(X) 🐑 Set a default file to answer if the request is a directory.\n\
$(X) 🐑 Large files into chunks!!!!!\n\
$(X) 🐑 Upload sucess message aendern... gescheit machen... dynamisch oder so.....\n\
$(X) 🐑 Cookies und Session managment\n\
$(X) 🐑 Direkt über den Server mit konfigurierbarem Speicherort\n\
$(X) 🐑 Über CGI-Scripts mit deren eigener Upload-Logik\n\
$(X) 🐑 The first server for a host:port will be the default for this host:port (that means it will answer to all the requests that don’t belong to an other server).\n\
$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)\n\

#------------------------------------------------------------------------------#
#--------------                      GENERAL                      -------------#
#------------------------------------------------------------------------------#

NAME=webserv

#------------------------------------------------------------------------------#
#--------------                       FLAGS                       -------------#
#------------------------------------------------------------------------------#

CC=c++
CFLAGS=-Wall -Wextra -Werror -Wshadow -std=c++17 -g -Wno-unused-but-set-variable
LDFLAGS=

ifeq ($(DEBUG), 1)
	CFLAGS += -fsanitize=address -g
endif

DEPFLAGS=-MMD -MP

#------------------------------------------------------------------------------#
#--------------                        DIR                        -------------#
#------------------------------------------------------------------------------#

OBJ_DIR := ./obj
DEP_DIR := $(OBJ_DIR)/.deps
INC_DIRS := .
SRC_DIRS := .

vpath %.cpp $(SRC_DIRS)
vpath %.h $(INC_DIRS)
vpath %.d $(DEP_DIR)

#------------------------------------------------------------------------------#
#--------------                        SRC                        -------------#
#------------------------------------------------------------------------------#

SRCS=	src/main.cpp \
		\
		src/error/ErrorHandler.cpp \
		\
		src/utils/Logger.cpp \
		src/utils/config_utils.cpp \
		src/utils/Sanitizer.cpp \
		\
		src/config/ConfigHandler_setup.cpp \
		src/config/ConfigHandler_parseline.cpp \
		src/config/ConfigHandler_sanitize.cpp \
		src/config/debug.cpp \
		\
		src/client/ClientHandler.cpp \
		\
		src/server/Server_loop.cpp \
		src/server/Server_init.cpp \
		src/server/Server_helpers.cpp \
		src/server/Server_event_handlers.cpp \
		\
		src/server/TaskManager.cpp \
		\
		src/cgi/CgiHandler_cleanup.cpp \
		src/cgi/CgiHandler_execute.cpp \
		src/cgi/CgiHandler_setup.cpp \
		\
		src/requests/RequestHandler_task.cpp \
		src/requests/RequestHandler_utils.cpp \
		src/requests/RequestHandler_parse.cpp \
		src/requests/RequestHandler_autoind.cpp \
		src/requests/RequestHandler_builder.cpp

#------------------------------------------------------------------------------#
#--------------                      OBJECTS                      -------------#
#------------------------------------------------------------------------------#

OBJECTS := $(addprefix $(OBJ_DIR)/, $(SRCS:%.cpp=%.o))

#------------------------------------------------------------------------------#
#--------------                      COMPILE                      -------------#
#------------------------------------------------------------------------------#

.PHONY: all clean fclean re

all: $(NAME)
	@if [ -e "./webserv.log" ]; then \
		echo "$(YELLOW)Clearing webserv.log$(X)"; \
		> ./webserv.log; \
	else \
		echo "$(YELLOW)Creating webserv.log$(X)"; \
		touch ./webserv.log; \
	fi

-include $(OBJECTS:.o=.d)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(@D)
	@$(CC) $(CFLAGS) $(DEPFLAGS) -c $< -o $@

$(NAME): $(OBJECTS)
	@$(CC) -o $@ $^ $(LDFLAGS)
	@echo "$(SUCCESS)"

container-build:
	@if ! docker ps | grep -q webserv; then \
		echo "$(YELLOW)Building the container environment$(X)"; \
		docker compose -f ./docker-compose.yml build --no-cache; \
	else \
		echo "$(YELLOW)Container already built.. skip build process$(X)"; \
	fi

container-up:
	@if ! docker ps | grep -q webserv; then \
		echo "$(YELLOW)Starting the container environment$(X)"; \
		docker compose -p webserv -f ./docker-compose.yml up -d; \
	else \
		echo "$(YELLOW)Container already running.. skip its creation$(X)"; \
	fi

container:
	@make container-build
	@make container-up
	@docker exec -it webserv bash

prune:
	@if docker ps -a | grep -q $(NAME); then \
		echo "$(RED)Removing existing container...$(X)"; \
		docker stop $(NAME) && docker rm $(NAME); \
	else \
		echo "$(YELLOW)No container named '$(NAME)' to remove.$(X)"; \
	fi
	@echo "$(GREEN)All done!$(X)"

test:
	@if [ -e "./webserv.log" ]; then \
		echo "$(YELLOW)Clearing webserv.log$(X)"; \
		> ./webserv.log; \
	fi
	@make && ./$(NAME) config/test.conf

leak:
	@if [ -e "./webserv.log" ]; then \
		echo "$(YELLOW)Clearing webserv.log$(X)"; \
		> ./webserv.log; \
	fi
	@make && valgrind --leak-check=full --track-origins=yes ./$(NAME)  config/test.conf

clean:
	@rm -rf $(OBJ_DIR)
	@echo "$(RED)objects deleted$(X)"

fclean: clean
	@rm -f $(NAME)
	@echo "$(RED)binaries deleted$(X)"

re: fclean all
