#include "StaticFileHandler.hpp"
#include "Request.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include "Mime.hpp"
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

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

void serve_static(Connection& conn) {
    Request& req = conn.req;

    // TEMP: Reject requests that are not GET.
    if (req.method != "GET") return conn.respond(501);

    // Collapse "." / ".." lexically and reject any target that escapes ROOT.
    std::string safe_target;
    if (!normalize_path(req.target, safe_target)) {
        Logger::warn(with_fd(conn.fd, Str() << "Path traversal blocked: " << req.target));
        return conn.respond(403);
    }

    std::string file_path = Str() << ROOT << safe_target;

    Logger::debug(Str() << "Stating the file: " << file_path);
    struct stat sb;
    if (stat(file_path.c_str(), &sb) == -1) {
        Logger::error(with_fd(conn.fd, Str() << "Couldn't stat() the file: " << file_path));
        return conn.respond(404);
    }

    int stat_mode = sb.st_mode & S_IFMT;
    if (stat_mode != S_IFREG && stat_mode != S_IFDIR) {
        Logger::warn(with_fd(conn.fd, Str() << file_path << " is not a file or dir. Stat mode: " << stat_mode));
        return conn.respond(403);
    }

    // TEMP: Autoindex from wish -> If dir append default file.
    if (stat_mode == S_IFDIR) {
        // No trailing slash -> 301 to "/sub/" so relative URLs resolve right.
        if (safe_target[safe_target.size() - 1] != '/') {
            return conn.redirect(safe_target + "/");
        }
        file_path.append(DEFAULT_FILE);

        // Re-stat it.
        if (stat(file_path.c_str(), &sb) == -1) {
            Logger::error(with_fd(conn.fd, Str() << "Couldn't stat() the file: " << file_path));
            return conn.respond(404);
        }
    }

    Logger::debug(Str() << "Opening the file: " << file_path);
    std::string content;
    int status = read_file(file_path, sb.st_size, content);
    if (status != 200) {
        Logger::error(with_fd(conn.fd, Str() << "Couldn't serve the file: " << file_path));
        return conn.respond(status);
    }

    return conn.respond(200, content, get_mime_type(file_path));
}
