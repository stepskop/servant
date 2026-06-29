// Config defaults/validation live inline in the parser
// (ConfigParser::apply_defaults_and_validate). This translation unit is kept as
// a placeholder so the Makefile's CONFIG_SRC slot stays wired; move shared
// model helpers here if they grow beyond the parser.
#include "Config.hpp"
