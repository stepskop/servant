#ifndef UTILS_HPP
# define UTILS_HPP

# include <map>
# include <string>
# include <sstream>
# include <vector>

# define CRLF "\r\n"

class Str {
    std::ostringstream ss;
public:
    template <typename T>
    Str& operator<<(const T& v) { ss << v; return *this; }
    operator std::string() const;
};

std::vector<std::string> split(std::string s, const std::string& delimiter);
std::string trim(const std::string& s);
std::string get_value(const std::map<std::string, std::string> &map, const std::string &key);
bool is_digits(const std::string &s);
bool safe_atol(const std::string &s, long &out);

#endif
