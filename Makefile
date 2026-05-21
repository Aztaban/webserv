# -------- Project --------
NAME = webserv

SRC_DIR := src
OBJ_DIR := build
INC_DIR := include

# -------- Compiler --------
CXX := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98
CPPFLAGS := -I$(INC_DIR)

RM := rm -rf

# -------- Sources --------
SRCS := \
	$(SRC_DIR)/main.cpp \
	$(SRC_DIR)/config/ServerConfig.cpp \
	$(SRC_DIR)/core/WebServer.cpp \
	$(SRC_DIR)/network/Listener.cpp \
	$(SRC_DIR)/network/ClientConnection.cpp \
	$(SRC_DIR)/network/EventLoop.cpp

OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)


# -------- Pretty --------
RESET := \033[0m
BOLD  := \033[1m
RED   := \033[31m
GRN   := \033[32m
YEL   := \033[33m
BLU   := \033[34m

# -------- Rules --------
all: $(NAME)

$(NAME): $(OBJS)
	@printf "$(BLU)Link$(RESET)   $(NAME)... "
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME) && \
		printf "%-22s[$(GRN)OK$(RESET)]\n" || \
		(printf "[$(RED)FAIL$(RESET)]\n"; exit 1)

$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@printf "CXX    %-35s " "$<"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@ && \
		printf "[$(GRN)OK$(RESET)]\n" || \
		(printf "[$(RED)FAIL$(RESET)]\n"; exit 1)

clean:
	@printf "$(YEL)Objects have been deleted.$(RESET)\n"
	@$(RM) $(OBJS) $(DEPS)
	@test -d $(OBJ_DIR) && find $(OBJ_DIR) -type d -empty -delete || true

fclean: clean
	@printf "$(YEL)Binary has been deleted.$(RESET)\n"
	@$(RM) $(NAME)

re: fclean all

run: $(NAME)
	@./$(NAME)

.PHONY: all clean fclean re run

-include $(DEPS)