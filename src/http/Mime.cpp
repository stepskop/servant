#include "Mime.hpp"
#include <string>
#include <cstddef>

struct MimeEntry {
    const char* ext;
    const char* type;
};

// Small static table — extend as needed. Matched case-sensitively for now.
static const MimeEntry MIME_TABLE[] = {
    { "html", "text/html" },
    { "htm",  "text/html" },
    { "css",  "text/css" },
    { "js",   "application/javascript" },
    { "json", "application/json" },
    { "png",  "image/png" },
    { "jpg",  "image/jpeg" },
    { "jpeg", "image/jpeg" },
    { "gif",  "image/gif" },
    { "svg",  "image/svg+xml" },
    { "ico",  "image/x-icon" },
    { "txt",  "text/plain" },
};

std::string get_mime_type(const std::string& path) {
    // Find the extension: text after the last '.', but only if that '.'
    // comes after the last '/' (so "/a.b/file" with no dot -> no extension).
    size_t slash = path.find_last_of('/');
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (slash != std::string::npos && dot < slash)) return "application/octet-stream";

    std::string ext = path.substr(dot + 1);
    const size_t n = sizeof(MIME_TABLE) / sizeof(MIME_TABLE[0]);
    for (size_t i = 0; i < n; ++i) {
        if (ext == MIME_TABLE[i].ext) return MIME_TABLE[i].type;
    }

    return "application/octet-stream";
}
