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
$(X)	🦄 ALLE TESTS MIT MAKE LEAK!!!!!!!....\n\
$(X)	🦄 wildcard checken....\n\
$(X)	🦄 forbidden fucntions\n\
$(X)	🦄 einmal read write send....\n\
$(X)	🦄 file logs und logs checken....\n\
$(X)	🦄 Checking the value of errno is strictly forbidden after a read or a write operation\n\
$(X)	🦄 error code pruefen gegen statuses.... \n\
$(X)	🦄 unused variables functions etc... \n\
$(X)	🦄 cgi code aufraeumen... \n\
$(X)	🦄 Siege Tests 95,5% avaibkabde | check size and mnenory usage...()leaks no restarts on siege usage\n\
$(X)	🦄 Wir sollten nochmal alle config operatoren durchchecken siehe webserv.hpp in location und serverblock\n\
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
#LDFLAGS=-flto=$(shell nproc)

#ifeq ($(DEBUG), 1)
#	CFLAGS += -fsanitize=address -g
#endif

#DEPFLAGS=-MMD -MP -MT $@
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
	@python3 src/Jeberle_warner.py
	@if [ $$? -ne 0 ]; then \
		echo "$(RED)Static analysis failed! Fix the issues before running the server.$(X)"; \
		exit 1; \
	fi
	@./$(NAME) config/test.conf

leak: $(NAME)
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
	@valgrind -s --leak-check=full --show-leak-kinds=all ./$(NAME) config/test.conf


sheeptest:
	@if [ -e "./webserv.log" ]; then \
		echo "$(YELLOW)Clearing webserv.log$(X)"; \
		> ./webserv.log; \
	fi
	@echo "$(GREEN)Running static analysis...$(X)"
	@echo "$(GREEN)Total C++ Project Lines:$(X) $(shell find . -type f \( -name "*.cpp" -o -name "*.hpp" \) | xargs wc -l | tail -n 1 | awk '{print $$1}')"
	@python3 src/Jeberle_warner.py
	@if [ $$? -ne 0 ]; then \
		echo "$(RED)Static analysis failed! Fix the issues before running the server.$(X)"; \
	fi
	@if [ -f "./$(NAME)" ]; then \
		echo "$(GREEN)No changes detected, skipping compilation and game.$(X)"; \
	else \
		echo "$(YELLOW)Compiling in the background...$(X)"; \
		make $(NAME) > /dev/null 2>&1 & \
		comp_pid=$$!; \
		while kill -0 $$comp_pid 2>/dev/null; do \
			echo -ne "$(CYAN)⏳ Compilation in progress... Play the game!$(X)\r"; \
			make sheep; \
		done; \
		wait $$comp_pid; \
		echo ""; \
		if [ -f "./$(NAME)" ]; then \
			echo "$(GREEN)✅ Compilation Finished!$(X)"; \
			echo "$(YELLOW)Starting the server...$(X)"; \
		else \
			echo "$(RED)❌ Compilation Failed!$(X)"; \
			exit 1; \
		fi; \
	fi
	./$(NAME) config/test.conf; \



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