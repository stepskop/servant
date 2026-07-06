#include "Connection.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <unistd.h>
#include <cerrno>
#include <cstring>

// Resolve the target under location.root (traversal-guarded, like static) and unlink it.
// Success -> 204; the failure status is decided by errno.
void delete_file(Connection& conn) {
    Request& req = conn.req;

    std::string file_path = Str() << conn.location->root << req.target;

    if (unlink(file_path.c_str()) == 0) {
        Logger::info(with_fd(conn.fd, Str() << "Deleted " << file_path));
        return conn.send(Response(204));
    }

    int err = errno;
    Logger::warn(with_fd(conn.fd, Str() << "Couldn't delete " << file_path << ": " << strerror(err)));
    switch (err) {
        case ENOENT:
        case ENOTDIR:
            return conn.send(Response(404)); // nothing there
        case EISDIR:
        case EPERM:
        case EACCES:
            return conn.send(Response(403)); // exists but refused
        default:
            return conn.send(Response(500));
    }
}
