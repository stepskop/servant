#ifndef UTILS_HPP
# define UTILS_HPP

# include <map>
# include <string>
# include <sstream>
# include <vector>

# define CRLF "\r\n"
# define LF "\n"

// Ostream-style builder that converts to std::string.
class Str {
    std::ostringstream ss;
public:
    template <typename T>
    Str& operator<<(const T& v) { ss << v; return *this; }
    operator std::string() const;
};

// Split s into the parts separated by delimiter.
std::vector<std::string> split(std::string s, const std::string& delimiter);

// Strip leading and trailing whitespace.
std::string trim(const std::string& s);

// Look up a key in the map.
std::string get_value(const std::map<std::string, std::string> &map, const std::string &key);

// Whether s is all decimal digits.
bool is_digits(const std::string &s);

// Parse s as a long.
bool safe_atol(const std::string &s, long &out);

// Lexically collapse "." and ".." in an absolute URL path.
bool normalize_path(const std::string &path, std::string &out);

// Read the whole regular file at path into out.
int read_file(const std::string &path, std::string &out);

// An open file to serve a response body from. fd is -1 when there is none.
struct FileBody {
    int fd;
    size_t size;
};

/*
 * Open the regular file at path to serve as a response body. On failure nothing
 * is left open and fd is -1.
 */
FileBody open_body(const std::string &path);

// Set a file descriptor to non-blocking mode.
void set_nonblocking(int fd);

// Set close-on-exec so the fd is not inherited by execve'd CGI children.
void set_cloexec(int fd);

// Case-insensitive string equality.
bool insensitive_equals(const std::string &a, const std::string &b);

// Lowercased copy of s (ASCII).
std::string to_lower(std::string s);

#endif
