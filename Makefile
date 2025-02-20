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
🐑    🐑   🐑    🐑🐑🐑🐑🐑🐑  🐑🐑🐑🐑🐑🐑    🐑🐑🐑🐑🐑🐑  🐑🐑🐑🐑🐑🐑  🐑    🐑🐑        🐑           🐑 $(X)\n\
🐑                                                                                                          🐑\n\
🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑\n\
$(X) 🐑 Wir muessen noch alle config werte im req handler abfangen!!!!!!\n\
$(X) 🐑 Checking the value of errno is strictly forbidden after a read or a write operation\n\
$(X) 🐑 You can use every macro and define like FD_SET, FD_CLR, FD_ISSET, FD_ZERO (understanding what and how they do it is very useful).\n\
$(X) 🐑 Siege Tests 95,5% avaibkabde | check size and mnenory usage...()leaks) no restarts on siege usage\n\
$(X) 🐑 Conf Timeout. NICHT die epoll timout sondern die Request Timeout\n\
$(X) 🐑 Timeout bei langer dauer der Processes CGIU e g PHP infinty while !!!!!!\n\
$(X) 🐖 Über CGI-Scripts mit deren eigener Upload-Logik\n\
$(X) 🐑 wildacrad und pre header compilation checken....\n\
$(X) 🐖 acheck autoindex\n\\n\
$(X) 🐖 ????????? location paths.... root!!! \n\
$(X) 🐖 // fallback for common error codes\n\
$(X) 🐖 const int defaultErrorCodes[] = {400, 401, 403, 404, 500, 502, 503, 504};\n\
$(X) 🐖 for (int errorCode : defaultErrorCodes) {\n\
$(X) 🐖 	if (conf.errorPages.find(errorCode) == conf.errorPages.end()) {\n\
$(X) 🐖 		conf.errorPages[errorCode] = "/50x.html";\n\
$(X) 🐖 	}\n\
$(X) 🐖 }\n\
$(X) 🐖 FILE Download .\n\
$(X) 🐑 Infinte redirect....  508 Error Page....  Loop locations redirect anaylsis in Configs .....\n\
$(X) 🐑 server {\n\
$(X) 🐑 	listen 8800;\n\
$(X) 🐑 	server_name localhost;\n\
$(X) 🐑 \n\
$(X) 🐑 	root ./var/www/html;\n\
$(X) 🐑 	index index.html;\n\
$(X) 🐑 \n\
$(X) 🐑 	location / {\n\
$(X) 🐑 		return 301 /team;\n\
$(X) 🐑 	}\n\
$(X) 🐑 \n\
$(X) 🐑 	location /team {\n\
$(X) 🐑 		return 301 /;\n\
$(X) 🐑 	}\n\
$(X) 🐑 }\n\
$(X) 🐑 leere DELETE Request (einpaar checks noch einbauen bitte)\n\
$(X) 🐑 staticHandler onhalt des block comments\n\
$(X) 🐑 enums to enum classes check all calls...\n\

# helper command to search for fucntions with more than 40 lines
#find . -type f -name "*.cpp" | xargs awk '
#/^[a-zA-Z_].*\(/ { func_name = $0; line_count = 0; }
#/{/ { inside_func = 1; }
#/}/ { if (inside_func) { inside_func = 0; if (line_count > 40) print FILENAME ": " func_name " - " line_count " lines"; } }
#inside_func { line_count++; }

# coiunt lines in all cpp and hpp files
#find . -type f \( -name "*.cpp" -o -name "*.hpp" \) | xargs wc -l
#------------------------------------------------------------------------------#
#--------------                      GENERAL                      -------------#
#------------------------------------------------------------------------------#

NAME=webserv

#------------------------------------------------------------------------------#
#--------------                       FLAGS                       -------------#
#------------------------------------------------------------------------------#

CC=c++
CFLAGS = -Wall -Wextra -Werror -Wshadow -std=c++17 -include $(PCH)
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
	@echo "$(GREEN)Running static analysis...$(X)"
	@echo "$(GREEN)Total C++ Project Lines:$(X) $(shell find . -type f \( -name "*.cpp" -o -name "*.hpp" \) | xargs wc -l | tail -n 1 | awk '{print $$1}')"
	@python3 src/Jeberle_warner.py
	@if [ $$? -ne 0 ]; then \
		echo "$(RED)Static analysis failed! Fix the issues before running the server.$(X)"; \
		exit 1; \
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
