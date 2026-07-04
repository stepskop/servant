#include "StaticFileHandler.hpp"
#include "Request.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "Mime.hpp"
#include <cstddef>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <dirent.h>

// Read the whole regular file at `path` into `out`. Always closes the fd, so
// no descriptor leaks on any return path. Returns the status to respond with:
// 200 on success, 403 if open() fails, 500 if read() fails mid-file.
static int read_file(const std::string& path, off_t size, std::string& out) {
    int fd = open(path.c_str(), O_RDONLY);
    if (fd == -1) return 403;

    char buf[8192];
    off_t total = 0;
    while (total < size) {
        ssize_t n = read(fd, buf, sizeof(buf));
        if (n < 0) {
            close(fd);
            return 500;
        }
        if (n == 0) break;
        out.append(buf, n);
        total += n;
    }

    close(fd);
    return 200;
}

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
static void serve_file(Connection& conn, const std::string& file_path, off_t file_size) {
    Logger::debug(Str() << "Opening the file: " << file_path);
    std::string content;
    int status = read_file(file_path, file_size, content);
    if (status != 200) {
        Logger::error(with_fd(conn.fd, Str() << "Couldn't serve the file: " << file_path));
        return conn.respond(status);
    }

    return conn.respond(200, content, get_mime_type(file_path));
}

// Handle a directory request: redirect a missing trailing slash, serve the
// index file if present, else autoindex or 403 depending on config.
static void serve_directory(Connection& conn, const std::string& target, const std::string& dir_path) {
    // No trailing slash -> 301 to "/sub/" so relative URLs resolve right.
    if (target[target.size() - 1] != '/') {
        return conn.redirect(301, target + "/");
    }

    // Try the index file first; serve it if present.
    std::string index_path = dir_path + conn.location->index;
    Logger::debug(Str() << "Stating the index file: " << conn.location->index);
    struct stat index_stat_buf;
    if (!conn.location->index.empty() && stat(index_path.c_str(), &index_stat_buf) == 0 && S_ISREG(index_stat_buf.st_mode)) {
        return serve_file(conn, index_path, index_stat_buf.st_size);
    }

    // If no index file, check autoindex config. If off, respond 403.
    if (!conn.location->autoindex) {
        return conn.respond(403);
    }

    // Autoindex is on, so build a directory listing and respond with it.
    std::string listing;
    if (!build_autoindex(dir_path, target, listing)) {
        return conn.respond(403);
    }

    return conn.respond(200, listing);
}

void serve_static(Connection& conn) {
    Request& req = conn.req;

    // Collapse "." / ".." lexically and reject any target that escapes root.
    std::string safe_target;
    if (!normalize_path(req.target, safe_target)) {
        Logger::warn(with_fd(conn.fd, Str() << "Path traversal blocked: " << req.target));
        return conn.respond(403);
    }

    // Build the full path to the requested file.
    std::string file_path = Str() << conn.location->root << safe_target;

    Logger::debug(Str() << "Stating the file: " << file_path);
    struct stat stat_buf;

    // If stat() fails, the file doesn't exist or is inaccessible -> 404.
    if (stat(file_path.c_str(), &stat_buf) == -1) {
        Logger::error(with_fd(conn.fd, Str() << "Couldn't stat() the file: " << file_path));
        return conn.respond(404);
    }

    // Check if the path is a regular file or directory. If not, respond 403.
    int stat_mode = stat_buf.st_mode & S_IFMT;
    if (stat_mode != S_IFREG && stat_mode != S_IFDIR) {
        Logger::warn(with_fd(conn.fd, Str() << file_path << " is not a file or dir. Stat mode: " << stat_mode));
        return conn.respond(403);
    }

    // If it's a directory, handle it according to the location config.
    if (stat_mode == S_IFDIR) {
        return serve_directory(conn, safe_target, file_path);
    }

    // It's a regular file, so serve it.
    return serve_file(conn, file_path, stat_buf.st_size);
}
