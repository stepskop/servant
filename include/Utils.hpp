#ifndef UTILS_HPP
# define UTILS_HPP

# include <string>
# include <sstream>

# define CRLF "\r\n"

class Str {
    std::ostringstream ss;
public:
    template <typename T>
    Str& operator<<(const T& v) { ss << v; return *this; }
    operator std::string() const;
};

#endif
