#include "Request.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <cstddef>
#include <string>
#include <utility>
#include <vector>

int parse_header(std::string &block, Request &req) {
    std::vector<std::string> lines = split(block, CRLF);
    std::string first_line = lines[0];

    // Split "GET /path?with=query HTTP/1.1" -> ["GET", "/path?with=query", "HTTP/1.1"]
    std::vector<std::string> request_line = split(first_line, " ");
    if (request_line.size() != 3) {
        Logger::debug(Str() << "First request line has " << request_line.size() << " elements. Expected 3");
        return 400; // Bad request
    }

    req.method = request_line[0]; // Raw token; method support checked by dispatcher.

    // Target must be origin-form (start with '/'); split path from query at first '?'.
    std::string target = request_line[1];
    if (target.empty() || target[0] != '/') {
        Logger::debug(Str() << "Target not in origin-form: " << target);
        return 400;
    }
    size_t q = target.find('?');
    if (q == std::string::npos) {
        req.target = target;
    } else {
        req.target = target.substr(0, q);
        req.query = target.substr(q + 1);
    }

    // Version must match "HTTP/X.Y" shape (400), then be exactly HTTP/1.1 (else 505).
    std::string version = request_line[2];
    if (version.size() != 8 || version.compare(0, 5, "HTTP/") != 0
        || !isdigit(version[5]) || version[6] != '.' || !isdigit(version[7])) {
        Logger::debug(Str() << "Malformed HTTP version: " << version);
        return 400;
    }
    if (version != "HTTP/1.1") {
        Logger::debug(Str() << "Unsupported HTTP version: " << version);
        return 505;
    }
    req.version = version;

    // Iterate through header lines.
    for (size_t i = 1; i < lines.size(); i++) {

        // split(block, CRLF) yields a trailing empty token; skip blank lines.
        if (lines[i].empty()) continue;

        // Split on the first ':' only — values may contain ':' (e.g. "Host: x:8080").
        size_t colon = lines[i].find(':');
        if (colon == std::string::npos) {
            Logger::debug(Str() << "Header line has no colon: " << lines[i]);
            return 400;
        }

        std::string name = lines[i].substr(0, colon);
        std::string value = trim(lines[i].substr(colon + 1));

        // Reject empty name or whitespace between name and ':'. (RFC 7230)
        if (name.empty() || name != trim(name)) {
            Logger::debug(Str() << "Malformed header name: " << name);
            return 400;
        }


        // Normalize name to lowercase for case-insensitive lookups.
        for (size_t j = 0; j < name.size(); j++) {
            name[j] = std::tolower(name[j]);
        }

        req.headers.insert(std::make_pair(name, value));
    }

    // HTTP/1.1 mandates a Host header.
    if (req.headers.find("host") == req.headers.end()) {
        Logger::debug("Missing required Host header");
        return 400;
    }

    return 200;
}
