#include "Request.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "Mime.hpp"
#include "Connection.hpp"
#include <cstddef>
#include <sys/stat.h>
#include <dirent.h>

// Escape HTML metacharacters so filenames/paths can't inject markup into the
// autoindex page. '&' first, or we'd double-escape the entities below.
static std::string html_escape(const std::string& in) {
    std::string out;
    out.reserve(in.size());
    for (size_t i = 0; i < in.size(); ++i) {
        switch (in[i]) {
            case '&':  out += "&amp;";  break;
            case '<':  out += "&lt;";   break;
            case '>':  out += "&gt;";   break;
            case '"':  out += "&quot;"; break;
            case '\'': out += "&#39;";  break;
            default:   out += in[i];
        }
    }
    return out;
}

static bool build_autoindex(const std::string& dir_path, const std::string& uri, std::string& out) {
    DIR* d = opendir(dir_path.c_str());
    if (!d) return false;

    std::string safe_uri = html_escape(uri);
    std::ostringstream body;
    body << "<html><head><title>Index of " << safe_uri << "</title></head><body>"
         << "<h1>Index of " << safe_uri << "</h1><ul>";

    struct dirent* e;
    while ((e = readdir(d)) != NULL) {
        std::string name = e->d_name;
        if (name == ".") continue;
        // Append "/" to subdirs so the browser knows to re-request as a dir.
        // stat() needs the raw name; escape only when writing it into the HTML.
        struct stat es;
        if (stat((dir_path + name).c_str(), &es) == 0 && S_ISDIR(es.st_mode)) {
            name += "/";
        }
        std::string safe_name = html_escape(name);
        body << "<li><a href=\"" << safe_uri << safe_name << "\">" << safe_name << "</a></li>";
    }
    closedir(d);

    body << "</ul></body></html>";
    out = body.str();
    return true;
}

// Read the regular file at `file_path` and respond with it, mapping any
// read_file failure to the matching error status.
static void serve_file(Connection& conn, const std::string& file_path) {
    Logger::debug(Str() << "Opening the file: " << file_path);
    std::string content;
    int status = read_file(file_path, content);
    if (status != 200) {
        Logger::error(with_fd(conn.fd, Str() << "Couldn't serve the file: " << file_path));
        return conn.send(Response(status));
    }

    return conn.send(Response(200).header("Content-Type", get_mime_type(file_path)).body(content));
}

// Handle a directory request: redirect a missing trailing slash, serve the
// index file if present, else a listing (autoindex on). With no listing: a
// configured-but-absent index is 404, an unconfigured index is 403.
static void serve_directory(Connection& conn, const std::string& target, const std::string& dir_path) {
    // No trailing slash -> 301 to "/sub/" so relative URLs resolve right.
    if (target[target.size() - 1] != '/') {
        return conn.send(Response(301).header("Location", target + "/"));
    }

    // Try the index file first; serve it if present.
    if (!conn.location->index.empty()) {
        std::string index_path = dir_path + conn.location->index;
        Logger::debug(Str() << "Stating the index file: " << conn.location->index);
        struct stat index_stat_buf;
        if (stat(index_path.c_str(), &index_stat_buf) == 0 && S_ISREG(index_stat_buf.st_mode)) {
            return serve_file(conn, index_path);
        }
        // An index is configured but this directory doesn't have it. With no
        // listing to fall back to, the requested resource is simply missing -> 404.
        if (!conn.location->autoindex) {
            return conn.send(Response(404));
        }
    } else if (!conn.location->autoindex) {
        // No index configured and directory listing disabled -> forbidden.
        return conn.send(Response(403));
    }

    // Autoindex is on, so build a directory listing and respond with it.
    std::string listing;
    if (!build_autoindex(dir_path, target, listing)) {
        return conn.send(Response(403));
    }

    return conn.send(Response(200).body(listing));
}

void serve_static(Connection& conn) {
    Request& req = conn.req;

    // Build the full path to the requested file (honours root vs alias).
    std::string file_path = conn.location->fs_path(req.target);

    Logger::debug(Str() << "Stating the file: " << file_path);
    struct stat stat_buf;

    // If stat() fails, the file doesn't exist or is inaccessible -> 404.
    if (stat(file_path.c_str(), &stat_buf) == -1) {
        Logger::error(with_fd(conn.fd, Str() << "Couldn't stat() the file: " << file_path));
        return conn.send(Response(404));
    }

    // Check if the path is a regular file or directory. If not, respond 403.
    int stat_mode = stat_buf.st_mode & S_IFMT;
    if (stat_mode != S_IFREG && stat_mode != S_IFDIR) {
        Logger::warn(with_fd(conn.fd, Str() << file_path << " is not a file or dir. Stat mode: " << stat_mode));
        return conn.send(Response(403));
    }

    // If it's a directory, handle it according to the location config.
    if (stat_mode == S_IFDIR) {
        return serve_directory(conn, req.target, file_path);
    }

    // It's a regular file, so serve it.
    return serve_file(conn, file_path);
}
