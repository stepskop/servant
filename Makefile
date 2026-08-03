NAME        = webserv

CXX         = c++
CXXFLAGS    = -Wall -Wextra -Werror -std=c++98 -pedantic -pedantic-errors

# ----- Version -------------------------------------------------------------- #
# The version the banner prints. Read from the last git tag, with the leading v
# dropped so it matches the published Docker tags (v1.2.3 -> 1.2.3, plus -dirty
# when the tree has uncommitted changes). Outside a checkout it is "dev": the
# Docker builds copy only the sources, so they pass the number in themselves.
#   make VERSION=1.2.3
GIT_VERSION := $(shell git describe --tags --dirty --always 2>/dev/null)
VERSION     ?= $(if $(GIT_VERSION),$(patsubst v%,%,$(GIT_VERSION)),dev)
CXXFLAGS    += -DVERSION='"$(VERSION)"'

CONFIG_DIR   = config/
CONFIG_SRC   = Config.cpp Tokenizer.cpp ConfigParser.cpp ConfigResolver.cpp

CORE_DIR     = core/
CORE_SRC     = Connection.cpp EventLoop.cpp Listener.cpp

HTTP_DIR     = http/
HTTP_SRC     = Request.cpp Response.cpp Status.cpp Mime.cpp

HANDLERS_DIR = handlers/
HANDLERS_SRC = Router.cpp StaticFileHandler.cpp UploadHandler.cpp DeleteHandler.cpp

CGI_DIR      = cgi/
CGI_SRC      = Cgi.cpp

UTILS_DIR    = utils/
UTILS_SRC    = Banner.cpp Logger.cpp Utils.cpp

MAIN         = main.cpp

SRC_DIR      = ./src/
SRCS         = $(addprefix $(CONFIG_DIR),   $(CONFIG_SRC))   \
               $(addprefix $(CORE_DIR),     $(CORE_SRC))     \
               $(addprefix $(HTTP_DIR),     $(HTTP_SRC))     \
               $(addprefix $(HANDLERS_DIR), $(HANDLERS_SRC)) \
               $(addprefix $(CGI_DIR),      $(CGI_SRC))      \
               $(addprefix $(UTILS_DIR),    $(UTILS_SRC))    \
               $(MAIN)

# ----- Headers (central include/) ------------------------------------------- #
INCLUDE_DIR  = ./include/
HEADERS      = -I$(INCLUDE_DIR)
# Every object depends on the headers: edit a .hpp and its dependents rebuild.
# Listed explicitly (no wildcard) — add new headers here.
HDR          = Banner.hpp Config.hpp ConfigParser.hpp ConfigResolver.hpp Connection.hpp EventLoop.hpp Listener.hpp Logger.hpp \
               Request.hpp Response.hpp Status.hpp Utils.hpp Mime.hpp \
               Handler.hpp Cgi.hpp
INCLUDES     = $(addprefix $(INCLUDE_DIR), $(HDR))

# ----- Objects (mirror the src/ tree into build/) --------------------------- #
OBJ_DIR      = ./build/
OBJS         = $(addprefix $(OBJ_DIR), $(SRCS:.cpp=.o))

# The version reaches the compiler through CXXFLAGS, but make compares file
# timestamps and never sees a changed flag: on its own, a new VERSION would leave
# the previous string baked into the objects. This file records the version the
# objects were built with, and every object depends on it. It is rewritten only
# when the version actually differs, so an unchanged version rebuilds nothing.
# The write happens while make reads this file, ahead of any rule.
VERSION_STAMP := $(OBJ_DIR).version
$(shell mkdir -p $(OBJ_DIR); \
        [ "$$(cat $(VERSION_STAMP) 2>/dev/null)" = "$(VERSION)" ] \
          || printf '%s\n' "$(VERSION)" > $(VERSION_STAMP))

# ----- Rules ---------------------------------------------------------------- #
all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "Done."

$(OBJ_DIR)%.o: $(SRC_DIR)%.cpp $(INCLUDES) $(VERSION_STAMP)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(HEADERS) -c $< -o $@

# Tests live under tests/ with their own Makefile (the dir may be gitignored):
#   make -C tests        # unit + integration + leaks
clean:
	rm -rf $(OBJ_DIR)

fclean: clean
	rm -rf $(NAME)

re: fclean all

linux:
	./tools/linux-build/linux-build.sh make re

full: linux re

.PHONY: all clean fclean re full linux
