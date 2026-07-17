#include "Request.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <cstddef>
#include <string>
#include <utility>
#include <vector>
#include <limits>

Request::Request(): body_size(0), initialized(false), chunked(false) {}

int parse_header(const std::string &block, Request &req) {
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

    std::string safe_target;
    if (!normalize_path(req.target, safe_target)) {
        Logger::debug(Str() << "Path traversal blocked: " << req.target);
        return 403;
    }
    req.target = safe_target;

    // Version must match "HTTP/X.Y" shape (400), then be exactly HTTP/1.1 (else 505).
    std::string version = request_line[2];
    if (version.size() != 8 || version.compare(0, 5, "HTTP/") != 0
        || version[6] != '.'
        || !isdigit(static_cast<unsigned char>(version[5]))
        || !isdigit(static_cast<unsigned char>(version[7]))) {
        Logger::debug(Str() << "Malformed HTTP version: " << version);
        return 400;
    }
    if (version != "HTTP/1.1" && version != "HTTP/1.0") {
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
            name[j] = std::tolower(static_cast<unsigned char>(name[j]));
        }

        req.headers.insert(std::make_pair(name, value));
    }

    // HTTP/1.1 mandates a Host header; HTTP/1.0 does not.
    if (req.version == "HTTP/1.1" && req.headers.find("host") == req.headers.end()) {
        Logger::debug("Missing required Host header");
        return 400;
    }

    req.initialized = true;

    return 200;
}

// Parse strinf to unsigned hexadecimal number.
static bool parse_hex(const std::string &s, size_t &out) {
    if (s.empty()) return false;

    size_t value = 0;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        size_t digit;
        if (c >= '0' && c <= '9') digit = c - '0';
        else if (c >= 'a' && c <= 'f') digit = c - 'a' + 10;
        else if (c >= 'A' && c <= 'F') digit = c - 'A' + 10;
        else return false; // non-hex: space, '-', '+', junk

        // Overflow guard before shift
        if (value > (std::numeric_limits<size_t>::max() - digit) / 16) return false;
        value = value * 16 + digit;
    }

    out = value;
    return true;
}

// Unchunks a chunked body sitting at the front of data.
//   0   -> incomplete, read more
//   200 -> complete: req.body filled, in_buf advanced past the chunked stream
//   400 -> malformed
//   413 -> total body exceeds max_body
int unchunk_data(std::string &in_buf, Request &req, size_t max_body) {
    size_t pos = 0;
    while (true) {
        size_t crlf = in_buf.find(CRLF, pos);
        if (crlf == std::string::npos) break; // Size line not fully here yet.

        std::string size_line = in_buf.substr(pos, crlf - pos);
        size_t semi = size_line.find(';');
        if (semi != std::string::npos) size_line = size_line.substr(0, semi);

        size_t chunk_size;
        if (!parse_hex(size_line, chunk_size)) return 400;

        size_t data_start = crlf + 2;

        if (chunk_size == 0) {
            size_t end = in_buf.find(CRLF, data_start);
            if (end == std::string::npos) break; // Final CRLF not here yet.
            in_buf.erase(0, end + 2); // Leave potentially piped next request.
            req.body_size = req.body.size();
            return 200;
        }

        if (req.body.size() + chunk_size > max_body) return 413;
        if (in_buf.size() < data_start + chunk_size + 2) break; // Chunk not fully here
        if (in_buf.compare(data_start + chunk_size, 2, CRLF) != 0) return 400;

        req.body.append(in_buf, data_start, chunk_size);
        pos = data_start + chunk_size + 2;
    }

    // Drop the chunks decoded this call; keep only the unfinished tail so the
    // next recv appends to it and we rescan just that, not everything again.
    in_buf.erase(0, pos);
    return 0; // need more
}
