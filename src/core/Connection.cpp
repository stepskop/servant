#include "Connection.hpp"
#include "Response.hpp"
#include <unistd.h>

Connection::Connection(int fd): fd(fd), state(READING_HEADERS), in_buf(""), out_buf(""), sent(0) {}
Connection::~Connection() {
    close(this->fd);
}

void Connection::respond(size_t status, const std::string& body) {
    out_buf.append(build_response(status, body));
    sent = 0;
    state = WRITING;
}
