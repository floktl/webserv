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
🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑\n\
🐑                                                                                                          🐑\n\
🐑  🐑       🐑  🐑🐑🐑🐑🐑🐑  🐑🐑🐑🐑🐑🐑    🐑🐑🐑🐑🐑🐑  🐑🐑🐑🐑🐑🐑  🐑🐑🐑🐑🐑    🐑      🐑$(X)       🐑\n\
🐑  🐑       🐑  🐑            🐑          🐑  🐑            🐑            🐑       🐑    🐑    🐑      🐑$(X)\n\
🐑  🐑   🐑  🐑  🐑🐑🐑🐑🐑🐑  🐑🐑🐑🐑🐑🐑    🐑🐑🐑🐑🐑🐑  🐑🐑🐑🐑🐑🐑  🐑🐑🐑🐑🐑      🐑  🐑      🐑 $(X)\n\
🐑  🐑 🐑 🐑 🐑  🐑            🐑          🐑            🐑  🐑            🐑   🐑          🐑🐑        🐑$(X)\n\
$(BLACK)🐑    $(X)🐑   🐑    🐑🐑🐑🐑🐑🐑  🐑🐑🐑🐑🐑🐑    🐑🐑🐑🐑🐑🐑  🐑🐑🐑🐑🐑🐑  🐑    🐑🐑        🐑           🐑 $(X)\n\
🐑                                                                                                          🐑\n\
🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑\n\
$(X) 🐑 Wir muessen noch alle config werte im req handler abfangen!!!!!!\n\
$(X) 🐑 Checking the value of errno is strictly forbidden after a read or a write operation\n\
$(X) 🐑 You can use every macro and define like FD_SET, FD_CLR, FD_ISSET, FD_ZERO (understanding what and how they do it is very useful).\n\
$(X) 🐑 Stress test shell script das de kiste fickt!!!!!!!!!\n\
$(X) 🐑 Conf Timeout. NICHT die epoll timout sondern die Request Timeout\n\
$(X) 🐑 Timeout bei langer dauer der Processes CGIU e g PHP infinty while !!!!!!\n\
$(X) 🐑 Cookies in CGI und Session managment\n\
$(X) 🐑 Über CGI-Scripts mit deren eigener Upload-Logik\n\
$(X) 🐑 check how to redirect vhosts_gate into services\n\
$(X) 🐑 The first server for a host:port will be the default for this host:port (that means it will answer to all the requests that don’t belong to an other server).\n\
$(X) 🇫🇷 Shorten jeberles wonderful contig\n\
$(X) 🇫🇷 Logger file in Error handle fucntion.... \n\
$(X) 🇫🇷 Forbidden functions.\n\
$(X) 🇫🇷 Include file \n\
$(X) 🇫🇷 100MBe \n\
$(X) 🇫🇷 Chunk und MB (1048576) size als CONST \n\

#------------------------------------------------------------------------------#
#--------------                      GENERAL                      -------------#
#------------------------------------------------------------------------------#

NAME=webserv

#------------------------------------------------------------------------------#
#--------------                       FLAGS                       -------------#
#------------------------------------------------------------------------------#

CC=c++
CFLAGS = -Wall -Wextra -Werror -Wshadow -std=c++17 -O2 -pipe -march=native -flto -DNDEBUG -Wno-unused-but-set-variable -include $(PCH)
LDFLAGS=-flto=$(shell nproc)

ifeq ($(DEBUG), 1)
	CFLAGS += -fsanitize=address -g
endif

DEPFLAGS=-MMD -MP -MT $@
PCH = ./src/utils/pch.hpp
PCHGCH = $(PCH).gch

$(PCHGCH): $(PCH)
	$(CC) $(CFLAGS) -x c++-header $< -o $@

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
		src/error/ErrorHandler.cpp \
		src/utils/Logger.cpp \
		src/utils/Sanitizer.cpp \
		$(wildcard src/config/*cpp) \
		$(wildcard src/server/*cpp) \

#------------------------------------------------------------------------------#
#--------------                      OBJECTS                      -------------#
#------------------------------------------------------------------------------#

OBJECTS := $(addprefix $(OBJ_DIR)/, $(SRCS:%.cpp=%.o))

#------------------------------------------------------------------------------#
#--------------                      COMPILE                      -------------#
#------------------------------------------------------------------------------#

.PHONY: all clean fclean re

all: $(PCHGCH) $(NAME)
	@if [ -e "./webserv.log" ]; then \
		echo "$(YELLOW)Clearing webserv.log$(X)"; \
		> ./webserv.log; \
	else \
		echo "$(YELLOW)Creating webserv.log$(X)"; \
		touch ./webserv.log; \
	fi

-include $(OBJECTS:.o=.d)

$(OBJ_DIR)/%.o: %.cpp $(PCHGCH)
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
	@docker exec -it webserv bash ./patrick.sh

prune:
	@if docker ps -a | grep -q $(NAME); then \
		echo "$(RED)Removing existing container...$(X)"; \
		docker stop $(NAME) && docker rm $(NAME); \
	else \
		echo "$(YELLOW)No container named '$(NAME)' to remove.$(X)"; \
	fi
	@echo "$(GREEN)All done!$(X)"

test: $(NAME)
	@if [ -e "./webserv.log" ]; then \
		echo "$(YELLOW)Clearing webserv.log$(X)"; \
		> ./webserv.log; \
	fi
	@./$(NAME) config/test.conf

leak:
	@if [ -e "./webserv.log" ]; then \
		echo "$(YELLOW)Clearing webserv.log$(X)"; \
		> ./webserv.log; \
	fi
	@make && valgrind --leak-check=full --track-origins=yes ./$(NAME)  config/test.conf

#------------------------------------------------------------------------------#
#--------------                   CLEANUP TARGETS                   -------------#
#------------------------------------------------------------------------------#

clean:
	@rm -rf $(OBJ_DIR)
	@echo "$(RED)objects deleted$(X)"

fclean: clean
	@rm -f $(NAME) $(PCHGCH)
	@echo "$(RED)binaries deleted$(X)"

re: fclean all

sheep:
	@echo "$(SUCCESS)"
