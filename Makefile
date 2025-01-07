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
$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)$(GREEN)█$(X)$(YELLOW)█$(X)\n\

#------------------------------------------------------------------------------#
#--------------                      GENERAL                      -------------#
#------------------------------------------------------------------------------#

NAME=webserv

#------------------------------------------------------------------------------#
#--------------                       FLAGS                       -------------#
#------------------------------------------------------------------------------#

CC=c++
CFLAGS=-Wall -Wextra -Werror -Wshadow -std=c++11 -g
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
		src/static/StaticHandler.cpp \
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
	@make && valgrind --leak-check=full --track-origins=yes ./$(NAME) config/test.conf

clean:
	@rm -rf $(OBJ_DIR)
	@echo "$(RED)objects deleted$(X)"

fclean: clean
	@rm -f $(NAME)
	@echo "$(RED)binaries deleted$(X)"

re: fclean all
