# -------- Project --------
NAME      = webserv
TEST_NAME = http_test

SRC_DIR := src
OBJ_DIR := build
INC_DIR := include

# -------- Compiler --------
CXX := c++
CXXFLAGS := -Wall -Wextra -Werror -std=c++98
CPPFLAGS := -I$(INC_DIR)

RM := rm -rf

# -------- Sources (SERVER) --------
SRCS := \
	$(SRC_DIR)/main.cpp \
	$(SRC_DIR)/config/ServerConfig.cpp \
	$(SRC_DIR)/core/RequestHandler.cpp \
	$(SRC_DIR)/core/WebServer.cpp \
	$(SRC_DIR)/network/Listener.cpp \
	$(SRC_DIR)/network/ClientConnection.cpp \
	$(SRC_DIR)/network/EventLoop.cpp \
	$(SRC_DIR)/routing/Router.cpp \
	$(SRC_DIR)/utils/Logger.cpp \
	$(SRC_DIR)/http/HttpRequestParser.cpp \
	$(SRC_DIR)/http/HttpResponse.cpp \
	$(SRC_DIR)/http/HttpResponseBuilder.cpp

OBJS := $(SRCS:%.cpp=$(OBJ_DIR)/%.o)
DEPS := $(OBJS:.o=.d)

# -------- Sources (TEST) --------
TEST_SRCS := \
	$(SRC_DIR)/http/test.cpp \
	$(SRC_DIR)/http/HttpRequestParser.cpp \
	$(SRC_DIR)/http/HttpRequestHandler.cpp \
	$(SRC_DIR)/http/HttpResponseBuilder.cpp

TEST_OBJS := $(TEST_SRCS:%.cpp=$(OBJ_DIR)/%.o)

# -------- Colors --------
RESET := \033[0m
BOLD  := \033[1m
RED   := \033[31m
GRN   := \033[32m
YEL   := \033[33m
BLU   := \033[34m

# -------- Default --------
all: $(NAME)

# -------- Server --------
$(NAME): $(OBJS)
	@printf "$(BLU)Link$(RESET)   $(NAME)... "
	@$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME) && \
		printf "%-25s[$(GRN)OK$(RESET)]\n" || \
		(printf "[$(RED)FAIL$(RESET)]\n"; exit 1)

# -------- Test --------
test: $(TEST_NAME)

$(TEST_NAME): $(TEST_OBJS)
	@printf "$(BLU)Link$(RESET)   $(TEST_NAME)... "
	@$(CXX) $(CXXFLAGS) $(TEST_OBJS) -o $(TEST_NAME) && \
		printf "%-25s[$(GRN)OK$(RESET)]\n" || \
		(printf "[$(RED)FAIL$(RESET)]\n"; exit 1)

runtest: $(TEST_NAME)
	@./$(TEST_NAME)

# -------- Compile --------
$(OBJ_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	@printf "CXX    %-35s " "$<"
	@$(CXX) $(CXXFLAGS) $(CPPFLAGS) -MMD -MP -c $< -o $@ && \
		printf "[$(GRN)OK$(RESET)]\n" || \
		(printf "[$(RED)FAIL$(RESET)]\n"; exit 1)

# -------- Run server --------
run: $(NAME)
	@./$(NAME)

# -------- Clean --------
clean:
	@printf "$(YEL)Cleaning objects...$(RESET)\n"
	@$(RM) $(OBJ_DIR)

fclean: clean
	@printf "$(YEL)Cleaning binaries...$(RESET)\n"
	@$(RM) $(NAME) $(TEST_NAME)

re: fclean all

# -------- PHONY --------
.PHONY: all clean fclean re run test runtest