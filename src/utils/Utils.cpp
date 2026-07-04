#include "Utils.hpp"
#include <string>
#include <sstream>
#include <fstream>
#include <map>
#include <climits>

// Read the whole file at `path` into `out`. Returns 200 on success, 403 if it
// can't be opened, 500 on a read error mid-stream. Opened in binary mode so
// served bytes (images, etc.) are not mangled.
int read_file(const std::string &path, std::string &out) {
    std::ifstream file(path.c_str(), std::ios::binary);
    if (!file.is_open()) return 403;

    std::ostringstream buffer;
    buffer << file.rdbuf();
    if (file.bad()) return 500;

    out = buffer.str();
    return 200;
}

Str::operator std::string() const {
    return ss.str();
}

std::vector<std::string> split(std::string s, const std::string& delimiter) {
    std::vector<std::string> tokens;
    size_t pos = 0;
    std::string token;
    while ((pos = s.find(delimiter)) != std::string::npos) {
        token = s.substr(0, pos);
        tokens.push_back(token);
        s.erase(0, pos + delimiter.length());
    }
    tokens.push_back(s);

    return tokens;
}

std::string get_value(const std::map<std::string, std::string> &map, const std::string &key) {
    std::map<std::string, std::string>::const_iterator it = map.find(key);

    if (it == map.end()) return "";
    return it->second;
}

// True only if non-empty and every char is 0-9. No sign, no space.
bool is_digits(const std::string &s) {
    if (s.empty()) return false;
    for (size_t i = 0; i < s.size(); ++i)
        if (s[i] < '0' || s[i] > '9')
            return false;
    return true;
}

// Parse optional-signed decimal into long. Rejects junk and overflow.
// Returns false on empty, bad char, or out-of-range.
bool safe_atol(const std::string &s, long &out) {
    size_t i = 0;
    bool neg = false;

    if (i < s.size() && (s[i] == '+' || s[i] == '-')) {
        neg = (s[i] == '-');
        ++i;
    }
    if (i >= s.size()) return false; // sign only, no digits

    long result = 0;
    for (; i < s.size(); ++i) {
        if (s[i] < '0' || s[i] > '9') return false;
        int digit = s[i] - '0';
        if (neg) {
            // building negative magnitude; guard against < LONG_MIN
            if (result < (LONG_MIN + digit) / 10) return false;
            result = result * 10 - digit;
        } else {
            if (result > (LONG_MAX - digit) / 10) return false;
            result = result * 10 + digit;
        }
    }
    out = result;
    return true;
}

// Lexically collapse "." and ".." segments of an absolute URL path.
// Pure string work — no FS access, so symlinks/missing files don't matter.
// Returns false if a ".." would pop above root (e.g. "/../etc/passwd").
bool normalize_path(const std::string &path, std::string &out) {
    std::vector<std::string> stack;
    std::vector<std::string> segments = split(path, "/");

    for (size_t i = 0; i < segments.size(); ++i) {
        const std::string &seg = segments[i];
        if (seg.empty() || seg == ".")
            continue; // collapse "//" and "/./"
        if (seg == "..") {
            if (stack.empty()) return false; // escapes above root
            stack.pop_back();
            continue;
        }
        stack.push_back(seg);
    }

    out = "/";
    for (size_t i = 0; i < stack.size(); ++i) {
        out += stack[i];
        if (i + 1 < stack.size()) out += "/";
    }
    // Preserve a trailing slash so directory targets still resolve to index.
    if (!stack.empty() && path[path.size() - 1] == '/') out += "/";
    return true;
}

// Strip leading/trailing OWS (space + horizontal tab).
std::string trim(const std::string& s) {
    const std::string ows = " \t";
    size_t start = s.find_first_not_of(ows);
    if (start == std::string::npos)
        return "";
    size_t end = s.find_last_not_of(ows);
    return s.substr(start, end - start + 1);
}
