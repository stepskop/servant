#include "Router.hpp"
#include "Cgi.hpp"
#include "Config.hpp"
#include "Connection.hpp"
#include "Request.hpp"
#include "Handler.hpp"
#include "Utils.hpp"

static const ServerConfig* select_server(const std::vector<const ServerConfig*> &server_group , const std::string &host) {
    // Strip port (example.com:80 -> example.com) and lowercase: the Host header
    // is case-insensitive, and server_names are stored lowercased.
    std::string host_without_port = to_lower(host.substr(0, host.find(':')));

    for (size_t i = 0; i < server_group.size(); i++) {
        if (server_group[i]->server_names.count(host_without_port)) return server_group[i];
    }
    return server_group[0]; // Fall back to first
}

// A path location matches a target only on a segment boundary: "/up" must
// not match "/uploads", while "/uploads" matches "/uploads" and "/uploads/x".
// A trailing slash on the configured path is ignored ("/uploads/" == "/uploads").
static bool location_matches(const std::string &path, const std::string &target) {
    // Root matches anything
    if (path == "/") return true;

    // Require the target to start with the path.
    if (target.compare(0, path.size(), path) != 0) return false;

    // Path matches; require a segment boundary so "/up" won't match "/uploads".
    // So we we can consider path as matched when either:
    // 1. The target ends at the path ("/uploads" matches "/uploads")
    // 2. The target continues with a slash ("/uploads/x" matches "/uploads)
    bool ends_at_target = target.size() == path.size();
    bool ends_at_segment = target.size() > path.size() && target[path.size()] == '/';

    return ends_at_target || ends_at_segment;
}

static const LocationConfig* select_location(const ServerConfig &server, const std::string &target) {
    const LocationConfig *best_match = NULL;
    size_t best_length = 0;

    for (size_t i = 0; i < server.locations.size(); i++) {
        const LocationConfig &location = server.locations[i];

        std::string path = location.path;
        // Drop trailing slash. /uploads and /uploads/ are equivalent.
        if (path.size() > 1 && path[path.size() - 1] == '/') {
            path.erase(path.size() - 1);
        }

        if (location_matches(path, target) && path.size() > best_length) {
            best_length = path.size();
            best_match = &location;
        }
    }

    return best_match;
}

static bool is_method_allowed(const LocationConfig &location, const std::string &method) {
    return location.methods.count(method) > 0;
}

static bool is_cgi(const LocationConfig &location, const std::string &target) {
    std::string ext = location.cgi_extension;

    if (ext.empty()) return false;

    std::string path = target.substr(0, target.find('?'));   // strip query
    size_t pos = path.find(ext);
    while (pos != std::string::npos) {
        size_t after = pos + ext.size();
        if (after == path.size() || path[after] == '/')      // ".py" then EOL or '/'
            return true;
        pos = path.find(ext, pos + 1);
    }
    return false;
}

void resolve(Connection &conn) {
    std::string host = get_value(conn.req.headers, "host");
    conn.server = select_server(*conn.server_group, host);
    conn.location = select_location(*conn.server, conn.req.target);
}

void route(Connection &conn) {
    Request &req = conn.req;

    if (!is_method_allowed(*conn.location, req.method)) {
        // A 405 must carry `Allow: <allowed methods>`.
        std::string allow;
        for (std::set<std::string>::const_iterator it = conn.location->methods.begin(); it != conn.location->methods.end(); ++it) {
            if (!allow.empty()) allow += ", ";
            allow += *it;
        }
        return conn.send(Response(405).header("Allow", allow));
    }

    // Handle redirection if configured in the location.
    if (conn.location->redirect.first != 0) {
        return conn.send(Response(conn.location->redirect.first).header("Location", conn.location->redirect.second));
    }

    // If it is a CGI request, handle it with the CGI handler.
    if (is_cgi(*conn.location, req.target)) {
        return handle_cgi(conn);
    }

    // Select the appropriate handler based on the request method and location configuration.
    if (req.method == "GET") {
        return serve_static(conn);
    } else if (req.method == "POST") {
        return upload_file(conn);
    } else if (req.method == "DELETE") {
        return delete_file(conn);
    } else {
        return conn.send(Response(501));
    }
}
