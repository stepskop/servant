#include "ConfigResolver.hpp"
#include "Utils.hpp"
#include <cctype>
#include <cstddef>
#include <limits>
#include <map>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

// "N", "10k", "5m", "1g" -> bytes. Rejects other suffixes / junk; guards
// overflow. Directive-agnostic: errors describe the size value, not the caller.
static std::size_t to_bytes(const std::string &raw) {
    size_t mult = 1;
    std::string digits = raw;

    char suffix = raw[raw.size() - 1];
    if (!std::isdigit(static_cast<unsigned char>(suffix))) {
        switch (std::tolower(static_cast<unsigned char>(suffix))) {
            case 'k': mult = 1024UL; break;
            case 'm': mult = 1024UL * 1024; break;
            case 'g': mult = 1024UL * 1024 * 1024; break;
            default: throw std::runtime_error(Str() << "invalid size suffix in '" << raw << "'");
        }
        digits = raw.substr(0, raw.size() - 1);
    }

    long n = 0;
    if (digits.empty() || !is_digits(digits) || !safe_atol(digits, n)) {
        throw std::runtime_error(Str() << "invalid size '" << raw << "'");
    }

    size_t val = static_cast<size_t>(n);
    if (val > std::numeric_limits<size_t>::max() / mult) {
        throw std::runtime_error(Str() << "size too large '" << raw << "'");
    }
    return val * mult;
}

// Split "host:port" | "port" and range-check the port. Writes host/port into out.
static void resolve_listen(const std::string &listen, ServerConfig &out) {
    if (listen.empty()) throw std::runtime_error("server block missing 'listen'");

    std::vector<std::string> parts = split(listen, ":");
    std::string host;
    std::string port;
    if (parts.size() == 1) {
        host = "0.0.0.0";
        port = parts[0];
    }
    else if (parts.size() == 2) {
        host = parts[0];
        port = parts[1];
    }
    else throw std::runtime_error(Str() << "invalid listen '" << listen << "'");

    long p = 0;
    if (!is_digits(port) || !safe_atol(port, p) || p < 1 || p > 65535) {
        throw std::runtime_error(Str() << "invalid port in listen '" << listen << "'");
    }

    out.host = host;
    out.port = port;
}

// Validate + convert raw (code, path) pairs into the typed map. Later entries
// for the same code win (matches directive order).
static void resolve_error_pages(const std::vector<std::pair<std::string, std::string> > &raw, std::map<int, std::string> &out) {
    for (size_t i = 0; i < raw.size(); i++) {
        std::string raw_code = raw[i].first;
        std::string raw_location = raw[i].second;

        long code = 0;
        if (!is_digits(raw_code) || !safe_atol(raw_code, code) || code < 400 || code > 599) {
            throw std::runtime_error(Str() << "error_page code must be 4xx/5xx (got '" << raw[i].first << "')");
        }
        out[static_cast<int>(code)] = raw_location;
    }
}

static LocationConfig resolve_location(const RawLocationConfig &raw, const ServerConfig &server) {
    LocationConfig loc;

    if (raw.path.empty() || raw.path[0] != '/') {
        throw std::runtime_error(Str() << "location path must start with '/' (got '" << raw.path << "')");
    }
    loc.path = raw.path;

    // methods: default to {GET}; validate the subset.
    if (raw.methods.empty()) {
        loc.methods.insert("GET");
    } else {
        for (size_t i = 0; i < raw.methods.size(); i++) {
            const std::string &m = raw.methods[i];
            if (m != "GET" && m != "POST" && m != "DELETE") {
                throw std::runtime_error(Str() << "unsupported method '" << m << "' in location " << raw.path);
            }
            loc.methods.insert(m);
        }
    }

    // Inheritance: fall back to the server's (already-resolved) values.
    loc.root  = raw.root.empty()  ? server.root  : raw.root;
    loc.index = raw.index.empty() ? server.index : raw.index;
    loc.autoindex = (raw.autoindex == "on");
    loc.client_max_body_size = raw.client_max_body_size.empty() ? server.client_max_body_size : to_bytes(raw.client_max_body_size);
    loc.upload_dir = raw.upload_dir;
    loc.cgi_extension = raw.cgi_extension;
    loc.cgi_interpreter = raw.cgi_interpreter;

    // error_pages: inherit the server's, then overlay the location's own.
    loc.error_pages = server.error_pages;
    resolve_error_pages(raw.error_pages, loc.error_pages);

    // return <code> <target>
    if (!raw.redirect_code.empty()) {
        long code = 0;
        if (!is_digits(raw.redirect_code) || !safe_atol(raw.redirect_code, code) || code < 300 || code > 399) {
            throw std::runtime_error(Str() << "return code must be 3xx (got '" << raw.redirect_code << "')");
        }
        if (raw.redirect_target.empty()) {
            throw std::runtime_error("return directive missing target");
        }
        loc.redirect = std::make_pair(static_cast<int>(code), raw.redirect_target);
    }

    // If the location has no root and the server has no root, that's an error.
    // The server's root is inherited if the location doesn't define one, so this check is only needed if both are empty.
    // Throws only when realy needed -> the location has no root and the location has no redirect (which would make the root irrelevant).
    if (loc.root.empty() && loc.redirect.first == 0) {
       throw std::runtime_error(Str() << "location " << raw.path << " has no root and the server declares none to inherit");
    }

    return loc;
}

static ServerConfig resolve_server(const RawServerConfig &raw) {
    ServerConfig server;

    resolve_listen(raw.listen, server);

    server.server_names = raw.server_names;
    server.root  = raw.root;
    server.index = !raw.index.empty() // may stay empty (Phase 4 default)
        ? raw.index
        : "index.html";
    server.client_max_body_size = !raw.client_max_body_size.empty()
        ? to_bytes(raw.client_max_body_size)
        : DEFAULT_MAX_BODY_SIZE;
    resolve_error_pages(raw.error_pages, server.error_pages);

    bool has_root = false;
    for (size_t i = 0; i < raw.locations.size(); i++) {
        server.locations.push_back(resolve_location(raw.locations[i], server));
        if (server.locations.back().path == "/") has_root = true;
    }

    // Guarantee a catch-all "/" location so every request resolves to one and
    // select_location never returns NULL. Inherits server root/index, GET-only.
    if (!has_root) {
        RawLocationConfig raw_root;
        raw_root.path = "/";
        server.locations.push_back(resolve_location(raw_root, server));
    }

    return server;
}

Config resolve(const RawConfig &raw) {
    if (raw.servers.empty()) throw std::runtime_error("Config must define at least one server block");

    Config config;
    for (size_t i = 0; i < raw.servers.size(); i++) {
        config.servers.push_back(resolve_server(raw.servers[i]));
    }
    return config;
}
