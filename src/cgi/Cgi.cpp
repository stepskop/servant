#include "Cgi.hpp"
#include "Connection.hpp"
#include "Logger.hpp"
#include "Request.hpp"
#include "Utils.hpp"
#include <cctype>
#include <csignal>
#include <map>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <ctime>

CgiProcess::CgiProcess(pid_t pid, int stdin_fd, int stdout_fd)
    :   pid(pid),
        stdin_fd(stdin_fd),
        stdout_fd(stdout_fd),
        in_sent(0),
        started(std::time(NULL)) {}

// Sole owner of the child's lifetime: close any open pipe ends and reap the
// child so it never lingers as a zombie. Safe to run in any state because every
// fd is sentinel -1 until wired up and reset to -1 once closed elsewhere.
CgiProcess::~CgiProcess() {
    if (this->stdin_fd != -1) close(this->stdin_fd);
    if (this->stdout_fd != -1) close(this->stdout_fd);
    if (this->pid > 0) {
        kill(this->pid, SIGKILL);
        waitpid(this->pid, NULL, 0);
    }
}

// Split a CGI target into SCRIPT_NAME (up to and including the script file) and
// PATH_INFO (the trailing path, if any). e.g. "/cgi-bin/x.py/a/b" with ext ".py"
// -> script_name="/cgi-bin/x.py", path_info="/a/b".
static void split_cgi_target(const std::string &target, const std::string &ext, std::string &script_name, std::string &path_info) {
    size_t pos = target.find(ext);
    while (pos != std::string::npos) {
        size_t after = pos + ext.size();
        if (after == target.size() || target[after] == '/') {
            script_name = target.substr(0, after);
            path_info   = target.substr(after); // "" or "/rest"
            return;
        }
        pos = target.find(ext, pos + 1);
    }

    script_name = target; // Fallback: no extension match, treat the whole target as the script name.
    path_info   = "";
}

static std::vector<std::string> build_cgi_env(Connection &conn, const std::string &script_name, const std::string &path_info) {
    const Request &req = conn.req;
    std::vector<std::string> env;

    env.push_back("GATEWAY_INTERFACE=CGI/1.1");
    env.push_back("SERVER_PROTOCOL=HTTP/1.1");
    env.push_back("REDIRECT_STATUS=200"); // some interpreters (php-cgi) refuse to run without it
    env.push_back(Str() << "REQUEST_METHOD=" << req.method);
    env.push_back(Str() << "SCRIPT_NAME=" << script_name);
    env.push_back(Str() << "SCRIPT_FILENAME=" << conn.location->root << script_name);
    env.push_back(Str() << "QUERY_STRING=" << req.query);
    env.push_back(Str() << "SERVER_NAME=" << conn.server->host);
    env.push_back(Str() << "SERVER_PORT=" << conn.server->port);

    // Extra path after the script name, per RFC 3875. PATH_TRANSLATED maps it
    // onto the filesystem under root (same root semantics as everything else).
    if (!path_info.empty()) {
        env.push_back(Str() << "PATH_INFO=" << path_info);
        env.push_back(Str() << "PATH_TRANSLATED=" << conn.location->root << path_info);
    }

    std::string content_type = get_value(req.headers, "content-type");
    if (!content_type.empty()) env.push_back(Str() << "CONTENT_TYPE=" << content_type);
    if (!req.body.empty()) env.push_back(Str() << "CONTENT_LENGTH=" << req.body.size());

    // Every request header as HTTP_<UPPER_SNAKE>. Content-Length/-Type are
    // passed unprefixed above, so skip them here.
    for (std::map<std::string, std::string>::const_iterator it = req.headers.begin(); it != req.headers.end(); it++) {
        if (it->first == "content-length" || it->first == "content-type") continue;
        std::string name = it->first;
        for (size_t i = 0; i < name.size(); i++) {
            name[i] = (name[i] == '-')
                ? '_'
                : std::toupper(static_cast<unsigned char>(name[i]));
        }
        env.push_back("HTTP_" + name + "=" + it->second);
    }
    return env;
}

void handle_cgi(Connection &conn) {
    // Split off any trailing PATH_INFO so we stat/exec the script itself, not
    // "/cgi-bin/x.py/extra" (which would 404).
    std::string script_name;
    std::string path_info;
    split_cgi_target(conn.req.target, conn.location->cgi_extension, script_name, path_info);

    // Check that the script is a regular file and readable. If not -> 404 / 403.
    std::string fs_path = conn.location->root + script_name; // same string chdir+argv resolve to
    struct stat st;
    if (stat(fs_path.c_str(), &st) == -1) return conn.send(Response(404));
    if (!S_ISREG(st.st_mode))             return conn.send(Response(404));
    if (access(fs_path.c_str(), R_OK))    return conn.send(Response(403));

    // The interpreter itself must exist and be executable.
    if (access(conn.location->cgi_interpreter.c_str(), X_OK) != 0) {
        return conn.send(Response(500));
    }

    // Create pipe that can be read by the CGI process for its stdin and written to by the parent.
    int stdin_pipe[2];
    int stdout_pipe[2];

    if (pipe(stdin_pipe) == -1) {
        Logger::error("Failed to create CGI stdin pipe.");
        return conn.send(Response(500));
    }
    if (pipe(stdout_pipe) == -1) {
        Logger::error("Failed to create CGI stdout pipe.");
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        return conn.send(Response(500));
    }

    // Close-on-exec on every pipe end. The child's dup2'd stdin/stdout (fd 0/1)
    // drop the flag and survive execve; these originals and the parent-kept ends
    // won't leak into this or any later CGI child.
    set_cloexec(stdin_pipe[0]);
    set_cloexec(stdin_pipe[1]);
    set_cloexec(stdout_pipe[0]);
    set_cloexec(stdout_pipe[1]);

    // Create the environment.
    std::vector<char*> envp;
    std::vector<std::string> env = build_cgi_env(conn, script_name, path_info);
    // Convert to char* array for execve.
    for (size_t i = 0; i < env.size(); i++) {
        envp.push_back(const_cast<char*>(env[i].c_str()));
    }
    envp.push_back(NULL);

    // Fork the CGI child. The child execs the interpreter, the parent continues to poll() the CGI's stdout and write() to its stdin.
    pid_t pid = fork();
    if (pid == -1) {
        Logger::error(with_fd(conn.fd, "Failed to fork CGI process."));

        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);

        conn.send(Response(500));
        return;
    }

    if (pid == 0) {
        // Child process: redirect stdin/stdout to the pipes.
        if (dup2(stdin_pipe[0], STDIN_FILENO) == -1) {
            Logger::error("Failed to redirect stdin for CGI process.");
            _exit(1);
        }
        if (dup2(stdout_pipe[1], STDOUT_FILENO) == -1) {
            Logger::error("Failed to redirect stdout for CGI process.");
            _exit(1);
        }

        // Close the unused ends of the pipes in the child.
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);

        // Close the parent's socket; the child doesn't need it.
        close(conn.fd);
        conn.fd = -1;

        // chdir into the script's OWN directory (not just location->root) so
        // relative file opens inside a nested script resolve the way they do
        // under nginx/Apache. fs_path == root + target; split off the basename.
        size_t last_slash_pos = fs_path.find_last_of('/');
        std::string script_dir  = (last_slash_pos == std::string::npos) ? "." : fs_path.substr(0, last_slash_pos);
        std::string script_file = (last_slash_pos == std::string::npos) ? fs_path : fs_path.substr(last_slash_pos + 1);
        if (chdir(script_dir.c_str()) == -1) {
            _exit(1);
        }

        // execve() the CGI interpreter with the script (now cwd-relative) as argv[1].
        std::string script_path = "./" + script_file;
        char *const argv[] = {
            const_cast<char*>(conn.location->cgi_interpreter.c_str()),
            const_cast<char*>(script_path.c_str()),
            NULL
        };
        execve(conn.location->cgi_interpreter.c_str(), argv, &envp[0]); // envp.data() is C++11 :(
        _exit(1); // If execve fails, exit the child process.
    }

    // Close child's ends of the pipes.
    close(stdin_pipe[0]);
    close(stdout_pipe[1]);

    int stdin_fd = stdin_pipe[1];
    int stdout_fd = stdout_pipe[0];

    set_nonblocking(stdin_fd);
    set_nonblocking(stdout_fd);

    conn.cgi = new CgiProcess(pid, stdin_fd, stdout_fd);
    conn.cgi->in_buf.swap(conn.req.body);
    conn.cgi->in_sent = 0;

    conn.state = WAITING_CGI;
}

Response parse_cgi_output(const std::string &raw) {
    // Split at the first blank line separating CGI headers from the body.
    // Accept both CRLF and bare-LF line endings; take whichever comes first.
    size_t crlf_pos = raw.find(Str() << CRLF << CRLF);
    size_t lf_pos   = raw.find(Str() << LF << LF);

    size_t sep;
    size_t sep_len;

    if (crlf_pos != std::string::npos && (lf_pos == std::string::npos || crlf_pos < lf_pos)) {
        sep = crlf_pos;
        sep_len = 2 * std::string(CRLF).size(); // Length of CRLF CRLF
    } else if (lf_pos != std::string::npos) {
        sep = lf_pos;
        sep_len = 2 * std::string(LF).size(); // Length of LF LF
    } else {
        sep = std::string::npos;
        sep_len = 0;
    }

    // A valid CGI response is a header block, a blank line, then the body. No
    // blank line means the script produced no CGI headers at all -> malformed.
    if (sep == std::string::npos) return Response(502);

    std::string header_block = raw.substr(0, sep);
    std::string body         = raw.substr(sep + sep_len);

    size_t status = 200;
    bool has_location = false;
    bool has_required = false; // saw at least one of Status / Content-Type / Location
    std::map<std::string, std::string> headers;

    std::istringstream stream(header_block);
    std::string line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line[line.size() - 1] == '\r') line.erase(line.size() - 1);
        if (line.empty()) continue;

        size_t colon = line.find(':');
        if (colon == std::string::npos) continue; // Not a header line -> skip.

        std::string key   = trim(line.substr(0, colon));
        std::string value = trim(line.substr(colon + 1));

        // "Status: NNN [reason]" sets the HTTP status line, not a header.
        if (insensitive_equals(key, "Status")) {
            std::istringstream ss(value);
            size_t code = 0;
            ss >> code;
            if (code) status = code;
            has_required = true;
            continue;
        }
        // Response::serialize() emits these itself. Don't let the CGI script override them.
        if (insensitive_equals(key, "Content-Length") || insensitive_equals(key, "Connection")) continue;

        // Canonicalise Content-Type so it overwrites the Response default rather
        // than adding a second, differently-cased header.
        if (insensitive_equals(key, "Content-Type")) {
            key = "Content-Type";
            has_required = true;
        }
        if (insensitive_equals(key, "Location")) {
            has_location = true;
            has_required = true;
        }

        headers[key] = value;
    }

    // A CGI document response must carry at least one of Content-Type / Status /
    // Location. A header block with none (exit 0 but garbage output) is malformed.
    if (!has_required) return Response(502);

    // CGI spec: a Location with no explicit Status is a 302 redirect.
    if (has_location && status == 200) status = 302;

    Response res(status);
    for (std::map<std::string, std::string>::const_iterator it = headers.begin(); it != headers.end(); it++)
        res.header(it->first, it->second);
    res.body(body);

    return res;
}
