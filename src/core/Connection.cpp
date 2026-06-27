#include "Connection.hpp"
#include "Response.hpp"
#include <unistd.h>

Connection::Connection(int fd): fd(fd), state(READING_HEADERS), in_buf(""), out_buf(""), sent(0) {}
Connection::~Connection() {
    close(this->fd);
}

void Connection::respond(size_t status, const std::string& body, const std::string& content_type) {
    out_buf.append(build_response(status, body, content_type));
    sent = 0;
    state = WRITING;
}

void Connection::redirect(const std::string& location) {
    out_buf.append(build_redirect(location));
    sent = 0;
    state = WRITING;
}
