#include "Connection.hpp"
#include "Logger.hpp"
#include "Mime.hpp"
#include "Response.hpp"
#include "Request.hpp"
#include "Config.hpp"
#include "Router.hpp"
#include "Utils.hpp"
#include <cstddef>
#include <unistd.h>
#include <ctime>

Connection::Connection(int fd, const std::vector<const ServerConfig*> *server_group)
    :   fd(fd),
        server((*server_group)[0]),
        location(NULL),
        server_group(server_group),
        state(READING_HEADERS),
        sent(0),
        res_status(0),
        cgi(NULL),
        keep_alive(false),
        last_activity(std::time(NULL)) {}

Connection::~Connection() {
    this->teardown_cgi();
    close(this->fd);
}

void Connection::teardown_cgi() {
    delete this->cgi;
    this->cgi = NULL;
}

bool Connection::should_register_cgi() const {
    return this->state == WAITING_CGI && this->cgi != NULL;
}

void Connection::send(Response res) {
    size_t status = res.get_status();

    // If the response is an error and has no body, try to serve a custom error page if configured.
    if (status >= 400 && !res.has_body() && this->location) {
        std::map<int, std::string>::const_iterator it = this->location->error_pages.find(status);
        if (it != this->location->error_pages.end()) {
            std::string body;
            std::string path = this->location->root + it->second;
            if (read_file(path, body) == 200) {
                res.header("Content-Type", get_mime_type(path)).body(body);
            }
        }
    }

    res.header("Connection", this->keep_alive ? "keep-alive" : "close");

    this->res_status = status;
    this->out_buf.append(res.serialize());
    this->sent = 0;
    this->last_activity = std::time(NULL);
    this->state = WRITING;
}

void Connection::fail(Response res) {
    this->keep_alive = false; // Framing abandoned -> don't reparse leftover bytes.
    this->send(res);
}

bool Connection::consume(const char* data, size_t len) {
    this->in_buf.append(data, len);
    return this->frame();
}

bool Connection::frame() {
    std::string header_end = Str() << CRLF << CRLF;
    size_t pos = this->in_buf.find(header_end);

    if (this->state == READING_HEADERS) {
        if (pos == std::string::npos) { // If no header found.
            if (this->in_buf.length() < MAX_HEADER_SIZE) return false; // Can read more.
            // Early reject while client may still be sending ->
            // close() with unread data sends RST, client may lose this 400.
            // Fix: Use shutdown() after this, and recv() until depleted.
            return (this->fail(Response(400)), false); // Cannot read more.
        }

        std::string header_block = this->in_buf.substr(0, pos);

        int parse_res = parse_header(header_block, this->req);

        // If parsing doesn't end with 200. Return 400: Bad request.
        if (parse_res != 200) return (this->fail(Response(parse_res)), false);

        // Connection token value is case-insensitive (RFC 7230).
        std::string conn_hdr = get_value(req.headers, "connection");
        if (req.version == "HTTP/1.1") {
            this->keep_alive = !insensitive_equals(conn_hdr, "close");
        } else { // HTTP/1.0
            this->keep_alive = insensitive_equals(conn_hdr, "keep-alive");
        }

        // Resolve the server and location for this request.
        // This is done after parsing the headers to ensure that the Host header is available for server selection.
        resolve(*this);

        std::string content_length_raw = get_value(this->req.headers, "content-length");
        std::string transfer_encoding_raw = get_value(this->req.headers, "transfer-encoding");

        bool has_content_length = !content_length_raw.empty();
        bool has_transfer_encoding = !transfer_encoding_raw.empty();

        if (has_transfer_encoding) {
            // Only "chunked" is supported; any other coding (gzip, compress, ...) -> 501.
            if (transfer_encoding_raw != "chunked") return (this->fail(Response(501)), false);
            if (has_content_length) return (this->fail(Response(400)), false);
            this->req.chunked = true;
        }

        if (has_content_length) {
            long content_length = 0;
            if (!is_digits(content_length_raw) || !safe_atol(content_length_raw, content_length)) return (this->fail(Response(400)), false);
            if (static_cast<size_t>(content_length) > this->location->client_max_body_size) return (this->fail(Response(413)), false); // Body is too big.
            this->req.body_size = content_length;
        }

        if (has_content_length || this->req.chunked) {
            this->last_activity = std::time(NULL);
            this->state = READING_BODY;
        }

        // Remove request header from the buffer.
        this->in_buf.erase(0, pos + header_end.size());
    }

    if (this->state == READING_BODY) {
        // Chunked bodies are self-delimiting: decode until the 0-size chunk.
        if (this->req.chunked) {
            int unchunking_res = unchunk_data(this->in_buf, this->req, this->location->client_max_body_size);
            if (unchunking_res == 0) return false; // Need to read more.
            if (unchunking_res != 200) return (this->fail(Response(unchunking_res)), false); // If unchunking errors.
            return true;
        }

        if (this->in_buf.size() < this->req.body_size) return false; // Need to read more;
        this->req.body.swap(this->in_buf);

        // If body is bigger = there is more some tail (maybe a new request piped)
        if (this->req.body.size() > this->req.body_size) {
            // Copy the tail.
            this->in_buf.assign(this->req.body, this->req.body_size, std::string::npos);
        }
        this->req.body.resize(this->req.body_size);
    }

    return true; // Fully framed -> ready to serve.
}

void Connection::reset() {
    this->req = Request();
    this->out_buf.clear();
    this->sent = 0;
    this->res_status = 0;
    this->state = READING_HEADERS;
    this->last_activity = std::time(NULL);

    if (this->cgi != NULL) Logger::error(with_fd(this->fd, "CGI is not NULL, this should not happen"));
}
