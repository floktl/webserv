# **************************************************************************** #
#                                                                              #
#                                                         :::      ::::::::    #
#    Makefile                                           :+:      :+:    :+:    #
#                                                     +:+ +:+         +:+      #
#    By: jeberle <jeberle@student.42.fr>            +#+  +:+       +#+         #
#                                                 +#+#+#+#+#+   +#+            #
#    Created: 2024/11/26 12:56:51 by jeberle           #+#    #+#              #
#    Updated: 2024/12/11 16:20:48 by jeberle          ###   ########.fr        #
#                                                                              #
# **************************************************************************** #

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
$(X) Und 404 Pages !!!!!!\n\
$(X) Timeout bei langer dauer der Processes CGIU e g PHP infinty while !!!!!!\n\
$(X) Upload Files:    Make the route able to accept uploaded files and configure where they should be saved. !!!!!!\n\
$(X) Stress test shell script das de kiste fickt!!!!!!!!!\n\
$(X) Turn on or off directory listing.\n\
$(X) Set a default file to answer if the request is a directory.\n\
$(X) Large files into chunks!!!!!\n\
$(X) Make shure EOF is set anywhere after eaxh output of default or cgi beahviour EOF\n\
$(X) Your program should call the CGI with the file requested as first argument.\n\
$(X) Cookies und Session managment\n\
$(X) Timeout blockiert alle anderen Requests\n\
$(X) The first server for a host:port will be the default for this host:port (that means it will answer to all the requests that don’t belong to an other server).\n\
$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)\n\

#------------------------------------------------------------------------------#
#--------------                      GENERAL                      -------------#
#------------------------------------------------------------------------------#

NAME=webserv

#------------------------------------------------------------------------------#
#--------------                       FLAGS                       -------------#
#------------------------------------------------------------------------------#

CC=c++
CFLAGS=-Wall -Wextra -Werror -Wshadow -std=c++11
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
		src/utils/Logger.cpp \
		src/utils/ConfigHandler.cpp \
		src/utils/Sanitizer.cpp \
		src/server/Server.cpp \
		src/requests/RequestHandler.cpp \
		src/helpers/helper.cpp

#------------------------------------------------------------------------------#
#--------------                      OBJECTS                      -------------#
#------------------------------------------------------------------------------#

OBJECTS := $(addprefix $(OBJ_DIR)/, $(SRCS:%.cpp=%.o))

#------------------------------------------------------------------------------#
#--------------                      COMPILE                      -------------#
#------------------------------------------------------------------------------#

.PHONY: all clean fclean re

all: $(NAME)

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
	./$(NAME) config/test.conf

clean:
	@rm -rf $(OBJ_DIR)
	@echo "$(RED)objects deleted$(X)"

fclean: clean
	@rm -f $(NAME)
	@echo "$(RED)binaries deleted$(X)"

re: fclean all