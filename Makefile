#curl -X POST http://localhost:8001 \
#     -H "Content-Type: text/plain" \
#     --data "aQW8fzXeLg0rKViNBdM2cU9HsZpTjyXoE3qBl7tARnYkwGvC1Jdhu5mxLFazOSegqvMZ3RBUbyJ8pkPoKlfQXtgcNsw4WE0dY9iHa7FqVnmrzTL6CJGbXU2AoeIK5shxtlYwD3cOqpSNgXZejMFvBk0HRLJna9uCmWi1fyGTV54MbscOYPDqtNEx0AezCdKpZGLWvjHgXYrhTUS13i7oxMuJ5mbc9YwXt6FKLldNRzaH2PO0QEGCvRkseTJp9gWfMdybnq5UoXhZKLY84tSlMwJNFvA7X2OYZBeuGDr3cCVKhLxPSEJgMnUoW9iTdAzXkflRcN3ygqO7Hb1BTvWYLmKpfzsdcahEt90XJ4GuoCVFbq3r5YTwKpLIMmnzAeR8gZfUxDBvCJWOoY7uTmNsA94qbgpFxXMTydRevlZoKiHSfAjBNGmUC9HnrqP73Rxy0atXjVLZceMbsYwguK9OtfqDWEIxlFpAvchGYzT0SnRmKUePbHDLoXaQJjWMgOtCdE0NsTPFx9AZmRuqIVCybLvYWgtKEa1c7hnqpmZgFRcUqXtNS8owblHEskzTGKYRjDCLu2pViBnjmfOXwPtgs97vZQWAbey3HqLJtkcEdNxM4hUGJzLyafVGKpeBqrYWoiR8XsKnTbMy7jlfZOuEQDJvHmt1sACxRGFvBZKe8qW49UhYNgLmdUpZCayJEnTwkg3hrKFxv5VQOLR2XpMrKViNBdM2cU9HsZpTjyXoE3qBl7tARnYkwGvC1Jdhu5mxLFazOSegqvMZ3RBUbyJ8pkPoKlfQXtgcNsw4WE0dY9iHa7FqVnmrzTL6CJGbXU2AoeIK5shxtlYwD3cOqpSNgXZejMFvBk0HRLJna9uCmWi1fyGTV54MbscOYPDqtNEx0AezCdKpZGLWvjHgXYrhTUS13i7oxMuJ5mbc9YwXt6FKLldNRzaH2PO0QEGCvRkseTJp9gWfMdybnq5UoXhZKLY84tSlMwJNFvA7X2OYZBeuGDr3cCVKhLxPSEJgMnUoW9iTdAzXkflRcN3ygqO7Hb1BTvWYLmKpfzsdcahEt90XJ4GuoCVFbq3r5YTwKpLIMmnzAeR8gZfUxDBvCJWOoY7uTmNsA94qbgpFxXMTydRevlZoKiHSfAjBNGmUC9HnrqP73Rxy0atXjVLZceMbsYwguK9OtfqDWEIxlFpAvchGYzT0SnRmKUePbHDLoXaQJjWMgOtCdE0NsTPFx9AZmRuqIVCybLvYWgtKEa1c7hnqpmZgFRcUqXtNS8owblHEskzTGKYRjDCLu2pViBnjmfOXwPtgs97vZQWAbey3HqLJtkcEdNxM4hUGJzLyafVGKpeBqrYWoiR8XsKnTbMy7jlfZOuEQDJvHmt1sACxRGFvBZKe8qW49UhYNgLmdUpZCayJEnTwkg3hrKFxv5VQOLR2XpMT9wsztdHbYlcfpo0GdWia7nqkS"

#curl --resolve schaff:80:127.0.0.1 http://schaff:9090
# curl --resolve schaaf:9090:127.0.0.1 http://schaaf:9090
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
🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑🐑\n\n\
$(GREEN)Task at the end:$(X)\n\
$(X)	🦄 ALLE TESTS MIT LEAK....\n\
$(X)	🦄 Wir sollten nochmal alle config operatoren und hostname durchchecken siehe webserv.hpp in location und serverblock\n\
$(X)  \n\

#------------------------------------------------------------------------------#
#--------------                      GENERAL                      -------------#
#------------------------------------------------------------------------------#

NAME=webserv

#------------------------------------------------------------------------------#
#--------------                       FLAGS                       -------------#
#------------------------------------------------------------------------------#

CC=c++
CFLAGS = -Wall -Wextra -Werror -Wshadow -std=c++17 -g -include $(PCH)
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

SRCS = \
	src/main.cpp \
	src/error/ErrorHandler.cpp \
	src/utils/Logger.cpp \
	src/utils/Sanitizer.cpp \
	src/config/config_debug.cpp \
	src/config/config_utils.cpp \
	src/config/ConfigHandler_parseline.cpp \
	src/config/ConfigHandler_sanitize.cpp \
	src/config/ConfigHandler_setup.cpp \
	src/server/Server_body_parsing.cpp \
	src/server/Server_cgi.cpp \
	src/server/Server_cookies.cpp \
	src/server/Server_debug.cpp \
	src/server/Server_download.cpp \
	src/server/Server_Epoll_Management.cpp \
	src/server/Server_execute_CGI.cpp \
	src/server/Server_file_handling.cpp \
	src/server/Server_helpers.cpp \
	src/server/Server_hostname.cpp \
	src/server/Server_init.cpp \
	src/server/Server_loop.cpp \
	src/server/Server_parse_Header.cpp \
	src/server/Server_path_access.cpp \
	src/server/Server_path_helpers.cpp \
	src/server/Server_redirect.cpp \
	src/server/Server_static_handler.cpp \
	src/server/Server_static_response.cpp

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

clear:
	@echo "🧹 Clearing uploaded files..."
	@rm -rf ./var/www/staticupload/uploads/*
	@rm -rf ./var/www/php/uploads/*

container-build:
	@if ! docker ps | grep -q webserv; then \
		echo "$(YELLOW)Building the container environment$(X)"; \
		docker compose -f ./utils/docker/docker-compose.yml build --no-cache; \
	else \
		echo "$(YELLOW)Container already built.. skip build process$(X)"; \
	fi

container-up:
	@if ! docker ps | grep -q webserv; then \
		echo "$(YELLOW)Starting the container environment$(X)"; \
		docker compose -p webserv -f ./utils/docker/docker-compose.yml up -d; \
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

test: $(NAME)
	@if [ -e "./webserv.log" ]; then \
		echo "$(YELLOW)Clearing webserv.log$(X)"; \
		> ./webserv.log; \
	fi
	@echo "$(GREEN)Running static analysis...$(X)"
	@echo "$(GREEN)Total C++ Project Lines:$(X) $(shell find . -type f \( -name "*.cpp" -o -name "*.hpp" \) | xargs wc -l | tail -n 1 | awk '{print $$1}')"
	@./$(NAME) config/test.conf

leak: $(NAME)
	@if [ -e "./webserv.log" ]; then \
		echo "$(YELLOW)Clearing webserv.log$(X)"; \
		> ./webserv.log; \
	fi
	@echo "$(GREEN)Running static analysis...$(X)"
	@echo "$(GREEN)Total C++ Project Lines:$(X) $(shell find . -type f \( -name "*.cpp" -o -name "*.hpp" \) | xargs wc -l | tail -n 1 | awk '{print $$1}')"
	@valgrind -s --leak-check=full --show-leak-kinds=all ./$(NAME) config/test.conf

#------------------------------------------------------------------------------#
#--------------                   CLEANUP TARGETS                   -------------#
#------------------------------------------------------------------------------#

clean: clear
	@rm -rf $(OBJ_DIR)
	@echo "$(RED)objects deleted$(X)"

fclean: clean
	@rm -f $(NAME) $(PCHGCH)
	@echo "$(RED)binaries deleted$(X)"

CLEAN_DIRS = var/www/staticupload

re: fclean all

GREEN := \033[32m
RED := \033[31m
YELLOW := \033[33m
BLUE := \033[34m
MAGENTA := \033[35m
CYAN := \033[36m
X := \033[0m
CLEAR := \033[H\033[J

sheep:
	@clear
	@echo "$(GREEN)🐑 Welcome to Sheep vs. Pigs! 🦄$(X)"
	@echo "$(YELLOW)Catch the pig before the unicorns catch you!$(X)"
	@echo "Press Q to quit."
	@sleep 2
	@bash -c ' \
		grid_size=10; \
		sheep_x=5; \
		sheep_y=5; \
		pig_x=7; \
		pig_y=7; \
		uni_x1=$$((RANDOM % grid_size)); \
		uni_y1=$$((RANDOM % grid_size)); \
		uni_x2=$$((RANDOM % grid_size)); \
		uni_y2=$$((RANDOM % grid_size)); \
		uni_x3=$$((RANDOM % grid_size)); \
		uni_y3=$$((RANDOM % grid_size)); \
		while true; do \
			echo -e "$(CLEAR)"; \
			echo -e "$(GREEN) SHEEP 🐑 | $(RED)PIG 🐖 | $(BLUE)UNICORNS 🦄$(X)"; \
			echo ""; \
			for y in $$(seq 0 $$grid_size); do \
				for x in $$(seq 0 $$grid_size); do \
					if [ $$x -eq $$sheep_x ] && [ $$y -eq $$sheep_y ]; then \
						echo -ne "$(GREEN)🐑$(X) "; \
					elif [ $$x -eq $$pig_x ] && [ $$y -eq $$pig_y ]; then \
						echo -ne "$(RED)🐖$(X) "; \
					elif [ $$x -eq $$uni_x1 ] && [ $$y -eq $$uni_y1 ]; then \
						echo -ne "$(BLUE)🦄$(X) "; \
					elif [ $$x -eq $$uni_x2 ] && [ $$y -eq $$uni_y2 ]; then \
						echo -ne "$(BLUE)🦄$(X) "; \
					elif [ $$x -eq $$uni_x3 ] && [ $$y -eq $$uni_y3 ]; then \
						echo -ne "$(BLUE)🦄$(X) "; \
					else \
						echo -ne "⬛ "; \
					fi; \
				done; \
				echo ""; \
			done; \
			echo ""; \
			echo -e "$(GREEN)W$(X) - sheep Move Up    $(GREEN)A$(X) - sheep Move Left    $(GREEN)S$(X) - sheep Move Down    $(GREEN)D$(X) - sheep Move Right"; \
			echo -e "$(GREEN)I$(X) - pig Move Up    $(GREEN)J$(X) - pig Move Left    $(GREEN)K$(X) - pig Move Down    $(GREEN)L$(X) - pig Move Right"; \
			echo -e "$(MAGENTA)Q$(X) - Quit"; \
			read -n1 -s key; \
			case $$key in \
				w) if [ $$sheep_y -gt 0 ]; then sheep_y=$$((sheep_y - 1)); fi;; \
				s) if [ $$sheep_y -lt $$grid_size ]; then sheep_y=$$((sheep_y + 1)); fi;; \
				a) if [ $$sheep_x -gt 0 ]; then sheep_x=$$((sheep_x - 1)); fi;; \
				d) if [ $$sheep_x -lt $$grid_size ]; then sheep_x=$$((sheep_x + 1)); fi;; \
				i) if [ $$pig_y -gt 0 ]; then pig_y=$$((pig_y - 1)); fi;; \
				k) if [ $$pig_y -lt $$grid_size ]; then pig_y=$$((pig_y + 1)); fi;; \
				j) if [ $$pig_x -gt 0 ]; then pig_x=$$((pig_x - 1)); fi;; \
				l) if [ $$pig_x -lt $$grid_size ]; then pig_x=$$((pig_x + 1)); fi;; \
				q) echo -e "$(MAGENTA)👋 Exiting game!$(X)"; exit;; \
				*) echo -e "$(CYAN)🤔 Invalid key!$(X)";; \
			esac; \
			if [ $$sheep_x -eq $$pig_x ] && [ $$sheep_y -eq $$pig_y ]; then \
				echo -e "$(BLUE)🎉 Sheep caught the pig! 🐖💀$(X)"; \
				sleep 2; \
				exit; \
			fi; \
			if ([ $$sheep_x -eq $$uni_x1 ] && [ $$sheep_y -eq $$uni_y1 ]) || \
				([ $$sheep_x -eq $$uni_x2 ] && [ $$sheep_y -eq $$uni_y2 ]) || \
				([ $$sheep_x -eq $$uni_x3 ] && [ $$sheep_y -eq $$uni_y3 ]) || \
				([ $$pig_x -eq $$uni_x1 ] && [ $$pig_y -eq $$uni_y1 ]) || \
				([ $$pig_x -eq $$uni_x2 ] && [ $$pig_y -eq $$uni_y2 ]) || \
				([ $$pig_x -eq $$uni_x3 ] && [ $$pig_y -eq $$uni_y3 ]); then \
				echo -e "$(RED)💀 A unicorn caught you! GAME OVER! 🦄$(X)"; \
				sleep 2; \
				exit; \
			fi; \
			move=$$((RANDOM % 4)); \
			case $$move in \
				0) if [ $$uni_y1 -gt 0 ]; then uni_y1=$$((uni_y1 - 1)); fi;; \
				1) if [ $$uni_y1 -lt $$grid_size ]; then uni_y1=$$((uni_y1 + 1)); fi;; \
				2) if [ $$uni_x1 -gt 0 ]; then uni_x1=$$((uni_x1 - 1)); fi;; \
				3) if [ $$uni_x1 -lt $$grid_size ]; then uni_x1=$$((uni_x1 + 1)); fi;; \
			esac; \
			move=$$((RANDOM % 4)); \
			case $$move in \
				0) if [ $$uni_y2 -gt 0 ]; then uni_y2=$$((uni_y2 - 1)); fi;; \
				1) if [ $$uni_y2 -lt $$grid_size ]; then uni_y2=$$((uni_y2 + 1)); fi;; \
				2) if [ $$uni_x2 -gt 0 ]; then uni_x2=$$((uni_x2 - 1)); fi;; \
				3) if [ $$uni_x2 -lt $$grid_size ]; then uni_x2=$$((uni_x2 + 1)); fi;; \
			esac; \
			move=$$((RANDOM % 4)); \
			case $$move in \
				0) if [ $$uni_y3 -gt 0 ]; then uni_y3=$$((uni_y3 - 1)); fi;; \
				1) if [ $$uni_y3 -lt $$grid_size ]; then uni_y3=$$((uni_y3 + 1)); fi;; \
				2) if [ $$uni_x3 -gt 0 ]; then uni_x3=$$((uni_x3 - 1)); fi;; \
				3) if [ $$uni_x3 -lt $$grid_size ]; then uni_x3=$$((uni_x3 + 1)); fi;; \
			esac; \
			sleep 0.002; \
		done \
	'
	@echo "$(SUCCESS)"