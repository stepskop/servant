#ifndef UTILS_HPP
# define UTILS_HPP

# include <string>
# include <sstream>

# define CRLF "\r\n"

std::string get_status_string(size_t status);
std::string build_response(size_t status, std::string body_str = "");

class Str {
    std::ostringstream ss;
public:
    template <typename T>
    Str& operator<<(const T& v) { ss << v; return *this; }
    operator std::string() const;
};

#endif
