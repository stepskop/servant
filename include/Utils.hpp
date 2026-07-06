#ifndef UTILS_HPP
# define UTILS_HPP

# include <map>
# include <string>
# include <sstream>
# include <vector>

# define CRLF "\r\n"
# define LF "\n"

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
// Lexically collapse "." and ".." in an absolute URL path.
// Returns false if a ".." escapes above root (traversal attempt).
bool normalize_path(const std::string &path, std::string &out);
// Read the whole regular file at `path` into `out`. Returns 200 on success,
// 403 if it can't be opened, 500 on a read error mid-file.
int read_file(const std::string &path, std::string &out);

// Sets the given file descriptor to non-blocking mode.
void set_nonblocking(int fd);

// Sets close-on-exec so the fd is not inherited by execve'd CGI children.
void set_cloexec(int fd);

// Case-insensitive string comparison. Returns true if the strings are equal ignoring case.
bool insensitive_equals(const std::string &a, const std::string &b);

#endif
