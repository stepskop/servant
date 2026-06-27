#include "Connection.hpp"
#include <unistd.h>

Connection::Connection(int fd): fd(fd), state(READING_HEADERS), in_buf(""), out_buf(""), sent(0) {}
Connection::~Connection() {
    close(this->fd);
}
