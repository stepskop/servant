#include "Utils.hpp"
#include <string>
#include <sstream>

Str::operator std::string() const {
    return ss.str();
}
