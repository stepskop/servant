<div align="center">
    <picture>
      <source media="(prefers-color-scheme: dark)" srcset="assets/banner.png">
      <img src="assets/banner-light.png" alt="servant — Servant of the Web" width="600">
    </picture>

# servant
A HTTP server written in C++.

A single thread serves many clients at once: every socket is non-blocking and
multiplexed through one `poll()` loop. No thread per connection, no blocking I/O.

</div>

---

## Build & run

```sh
make                    # build ./webserv
./webserv               # run with ./default.conf
./webserv my.conf       # run with a specific config file
```

The listen address(es), document root, allowed methods, error pages, etc. all
come from the config file — nothing is hardcoded.

## Lifecycle

```mermaid
flowchart TD
    boot["main()<br/>add_listener(host, port)"] --> listen["Listener::start()<br/>socket + bind() + listen()"]
    listen --> poll

    poll{"EventLoop::run()<br/>poll() over every fd"}

    poll -- "listener POLLIN" --> accept["accept_connection<br/>accept() + new Connection"]
    accept --> poll

    poll -- "client POLLIN" --> read["handle_read<br/>recv() once"]
    poll -- "client POLLOUT" --> write["handle_write<br/>send() drains out_buf"]

    read --> consume["Connection::consume<br/>frame the request"]
    consume -- "incomplete" --> poll
    consume -- "full request" --> resolve["resolve()<br/>select server + location"]
    resolve --> route["route()<br/>check method, select handler"]
    route -- "static / upload / delete" --> serve["handler<br/>serve_static / upload / delete"]
    route -- "CGI location" --> cgi["handle_cgi<br/>fork+execve, pipes into poll set<br/>state = WAITING_CGI"]
    serve --> respond["conn.send(Response)<br/>fill out_buf, state = WRITING"]
    respond --> poll

    poll -- "cgi pipe POLLIN/POLLOUT" --> cgi
    cgi -- "stdout EOF / timeout" --> respond
    cgi -- "in flight" --> poll

    write -- "partial" --> poll
    write -- "fully sent" --> close["close connection"]
```

A `Connection` is a small state machine driven by `EventLoop`:
`READING_HEADERS → READING_BODY → PROCESSING → WRITING → CLOSING`, with CGI
requests detouring through `WAITING_CGI` between `PROCESSING` and `WRITING`.

`resolve_poll_event()` maps the current state to the poll flags the loop should
wait on (`POLLIN` while reading, `POLLOUT` while writing, nothing otherwise), so
a connection is only woken when it can make progress.

### Framing

`Connection::consume()` appends received bytes and advances the framing FSM:

- **Headers** — buffered until `\r\n\r\n`. Capped at `MAX_HEADER_SIZE` (8 KB);
  malformed or oversized headers get a `400`.
- **Body** — read up to `Content-Length`, capped at the matched location's
  `client_max_body_size` (from config → `413`). Chunked transfer-encoding is
  currently rejected with `501`.
- Pipelined bytes past the body are kept in the buffer for the next request.

It returns `true` only once a full request is framed and ready to serve.

## Routing

Once framed, `resolve()` selects the `ServerConfig` (by `Host` header among the
listener's virtual hosts) and the longest-prefix `LocationConfig` for the
target (matched on segment boundaries, trailing slash ignored). `route()` then:

1. enforces the location's allowed methods (`405` with an `Allow` header),
2. applies any configured `return` redirect (`301`/`302` with `Location`),
3. if the location has a `cgi_extension` and the target's script segment ends
   with it, dispatches to the CGI handler (see below),
4. otherwise dispatches by method: `GET` → static serving, `POST` → upload,
   `DELETE` → delete. Anything else → `501`.

## CGI

A request matching a CGI location runs a script through its `cgi_interpreter`
via `fork()` + `execve()` without blocking the loop: the child's stdin/stdout
pipes join the poll set, the request body is streamed in and the output read
back until EOF, then the child is reaped with `waitpid(WNOHANG)`. Its output is
split at the first blank line — CGI headers (incl. `Status:`/`Location:`) merge
into the response, the rest is the body. A script that overruns its deadline is
killed and answered `504`; other failures map to `404`/`403`/`500`/`502`.

## Responses

Responses are built with a small chainable `Response` object and sent through
one choke point, `Connection::send()`:

```cpp
conn.send(Response(200).header("Content-Type", mime).body(content));
conn.send(Response(301).header("Location", target + "/"));
conn.send(Response(404));   // body auto-filled from the location's error_page, or a default
```

`send()` serializes to the wire form (always emitting `Connection: close` and
`Content-Length`) and, for a bodyless error status, serves the configured
custom `error_page` file if one is set, falling back to a built-in page.

## Layout

```
include/            public headers (one per .cpp, -Iinclude)
src/
  main.cpp          boot: load config, then hand off to the EventLoop
  core/             the networking engine — sockets, polling, connections
  http/             the HTTP/1.1 protocol — parse requests, build responses
  handlers/         decide what a request does and produce its response
  cgi/              fork/execve a CGI script and pump its pipes through poll
  config/           turn the config file into the server/location model
  utils/            shared helpers (logging, strings, paths, file reads)
www/                example document root used by default.conf
tools/linux-build/  Docker wrapper to build/test on Linux from macOS
```

## Components

| Component | Responsibility |
|-----------|----------------|
| `EventLoop` | Owns all `Listener`s and `Connection`s. Builds the pollfd set each tick, dispatches readable/writable FDs, accepts new clients, reaps dead ones. Catches `SIGINT`/`SIGTERM` for clean shutdown. |
| `Listener` | A bound, listening socket for one `host:port`. |
| `Connection` | Per-client state: `fd`, in/out buffers, `state`, parsed `Request`, matched server/location. Frames requests via `consume()`, queues output via `send()`. |
| `Request` | Parsed method, target, query, version, lowercased headers, body. |
| `Response` | Chainable response builder (`status`, `.header()`, `.body()`); `serialize()` produces the wire form. |
| `Config` | Parses the config file into `ServerConfig`/`LocationConfig` (root, index, methods, redirects, `client_max_body_size`, `error_page`, autoindex, `cgi_extension`/`cgi_interpreter`) via a three-stage Tokenizer → parser → resolver pipeline. |
| `Router` | Selects the server (by `Host`) and longest-prefix location, enforces allowed methods, applies redirects, detects CGI, and dispatches to a handler. |
| `StaticFileHandler` | Serves a file under the matched location's `root`, using its `index` for directories, or an autoindex listing. |
| `UploadHandler` | Handles `POST` — multipart and raw bodies → `201` with a `Location` for the created resource. |
| `DeleteHandler` | Handles `DELETE` — `204`/`404`/`403`, traversal-guarded. |
| `Cgi` / `CgiProcess` | Launches a CGI child (fork/execve), owns its pipe fds + pid + deadline, and parses its output into a `Response`. Pipes are driven by the poll loop; the child is reaped and killed-on-timeout. |
| `Logger` / `Utils` | Logging and string/file helpers shared across the codebase. |

## WIP Notes

- `SIGPIPE` is ignored so a write to a closed socket fails the `send()` instead
  of killing the process.
- `poll()` blocks indefinitely (`-1`) when idle; while a CGI child is running the
  timeout shrinks to its nearest deadline so a runaway script hits its `504`. A
  signal interrupts `poll()` (`EINTR`) so the loop can recheck the shutdown flag.
- `GET` (static + autoindex), `POST` (upload), and `DELETE` are all handled;
  CGI runs through the poll loop. Chunked transfer-encoding is still rejected
  with `501` (unchunking is the next piece).
- Keep-alive is not yet wired up — every response carries `Connection: close`
  and the connection closes after one response.
