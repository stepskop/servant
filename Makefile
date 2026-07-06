NAME        = webserv

CXX         = c++
CXXFLAGS    = -Wall -Wextra -Werror -std=c++98 -pedantic -pedantic-errors

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
UTILS_SRC    = Logger.cpp Utils.cpp

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
HDR          = Config.hpp ConfigParser.hpp ConfigResolver.hpp Connection.hpp EventLoop.hpp Listener.hpp Logger.hpp \
               Request.hpp Response.hpp Status.hpp Utils.hpp Mime.hpp \
               Handler.hpp Cgi.hpp
INCLUDES     = $(addprefix $(INCLUDE_DIR), $(HDR))

# ----- Objects (mirror the src/ tree into build/) --------------------------- #
OBJ_DIR      = ./build/
OBJS         = $(addprefix $(OBJ_DIR), $(SRCS:.cpp=.o))

# ----- Rules ---------------------------------------------------------------- #
all: $(NAME)

$(NAME): $(OBJS)
	$(CXX) $(CXXFLAGS) $(OBJS) -o $(NAME)
	@echo "Done."

$(OBJ_DIR)%.o: $(SRC_DIR)%.cpp $(INCLUDES)
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
