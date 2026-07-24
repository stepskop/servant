#include "Connection.hpp"
#include "Logger.hpp"
#include "Utils.hpp"
#include <fstream>
#include <sys/stat.h>

// Reduce a client-supplied filename to a bare basename: drop everything up to
// the last '/' or '\', reject empty / "." / "..". Prevents an upload escaping
// upload_dir regardless of what the client sends.
static bool safe_basename(const std::string &raw, std::string &out) {
    std::string::size_type cut = raw.find_last_of("/\\");
    out = (cut == std::string::npos) ? raw : raw.substr(cut + 1);
    if (out.empty() || out == "." || out == "..") {
        return false;
    }
    return true;
}

// Pull the filename="..." token out of a part's header block. The token is only
// honored on the Content-Disposition line, so a stray filename=" in any other
// header can't be mistaken for the upload name.
static bool disposition_filename(const std::string &headers, std::string &out) {
    std::vector<std::string> lines = split(headers, CRLF);
    for (std::vector<std::string>::size_type i = 0; i < lines.size(); ++i) {
        if (to_lower(lines[i]).compare(0, 20, "content-disposition:") != 0) {
            continue;
        }
        std::string::size_type key = lines[i].find("filename=\"");
        if (key == std::string::npos) {
            return false;
        }
        std::string::size_type start = key + 10; // past filename="
        std::string::size_type end = lines[i].find('"', start);
        if (end == std::string::npos) {
            return false;
        }
        out = lines[i].substr(start, end - start);
        return true;
    }
    return false;
}

// Extract the first file part (name + bytes) from a multipart/form-data body.
// `boundary` is the raw token from the Content-Type header (without leading --).
// Supports at least one file per request, which is all Phase 4 requires.
static bool parse_multipart(const std::string &body, const std::string &boundary, std::string &name, std::string &content) {
    const std::string delim = "--" + boundary;

    std::string::size_type pos = body.find(delim);
    while (pos != std::string::npos) {
        pos += delim.size();
        // A trailing "--" right after the delimiter marks the final boundary.
        if (body.compare(pos, 2, "--") == 0) {
            break;
        }
        if (body.compare(pos, 2, CRLF) != 0) {
            return false; // malformed: boundary not followed by CRLF
        }
        pos += 2;

        // Part headers run until a blank line.
        std::string::size_type head_end = body.find(CRLF CRLF, pos);
        if (head_end == std::string::npos) {
            return false;
        }
        std::string headers = body.substr(pos, head_end - pos);
        std::string::size_type data_start = head_end + 4;

        // The part body ends at the CRLF preceding the next boundary.
        std::string::size_type next = body.find(delim, data_start);
        if (next == std::string::npos) {
            return false;
        }
        std::string::size_type data_end = next;
        if (data_end >= 2 && body.compare(data_end - 2, 2, CRLF) == 0) {
            data_end -= 2;
        }

        std::string fname;
        if (disposition_filename(headers, fname) && !fname.empty()) {
            name = fname;
            content = body.substr(data_start, data_end - data_start);
            return true;
        }
        pos = next; // no filename in this part; advance to the next boundary
    }
    return false;
}

void upload_file(Connection &conn) {
    Request &req = conn.req;

    // Uploads only make sense where the config opted in.
    if (conn.location->upload_dir.empty()) {
        Logger::debug(with_fd(conn.fd, "POST to a location without upload_dir"));
        return conn.send(Response(403));
    }

    std::string name;
    std::string content;

    std::string content_type = get_value(req.headers, "content-type");
    std::string expected_content_type = "multipart/form-data";
    if (content_type.compare(0, expected_content_type.size(), expected_content_type) == 0) {
        std::string::size_type bpos = content_type.find("boundary=");
        if (bpos == std::string::npos) {
            return conn.send(Response(400));
        }
        std::string boundary = trim(content_type.substr(bpos + 9));
        if (!parse_multipart(req.body, boundary, name, content)) {
            Logger::warn(with_fd(conn.fd, "multipart body without a usable file part"));
            return conn.send(Response(400));
        }
    } else {
        // Raw body: derive the name from the URL tail. (EXPERIMENTAL);
        name = req.target;
        content = req.body;
    }

    std::string filename;
    if (!safe_basename(name, filename)) {
        Logger::debug(with_fd(conn.fd, Str() << "Rejected upload filename: " << name));
        return conn.send(Response(403));
    }

    // Extract the subdirectory under the location prefix.
    std::string subdir = req.target; // "/location/subdir/file.txt"
    if (subdir.compare(0, conn.location->path.size(), conn.location->path) == 0) {
        subdir = subdir.substr(conn.location->path.size()); // "/subdir/file.txt"
    }
    std::string::size_type slash = subdir.find_last_of('/');
    subdir = (slash != std::string::npos) ? subdir.substr(0, slash) : ""; // "/subdir"

    // Build the directory path and check that it exists. The upload_dir is the root.
    std::string dir = Str() << conn.location->upload_dir << subdir;
    struct stat stat_buffer;
    if (stat(dir.c_str(), &stat_buffer) != 0 || !S_ISDIR(stat_buffer.st_mode)) {
        Logger::warn(with_fd(conn.fd, Str() << "Upload target directory does not exist: " << dir));
        return conn.send(Response(404));
    }

    // Build the full path to the destination file and write it.
    std::string file_path = Str() << dir << "/" << filename;

    std::ofstream outfile(file_path.c_str(), std::ios::binary);
    if (!outfile.is_open()) {
        Logger::error(with_fd(conn.fd, Str() << "Couldn't open file for writing: " << file_path));
        return conn.send(Response(500));
    }
    outfile.write(content.c_str(), content.size());
    if (outfile.bad()) {
        Logger::error(with_fd(conn.fd, Str() << "Error writing to file: " << file_path));
        return conn.send(Response(500));
    }
    outfile.close();

    Logger::info(with_fd(conn.fd, Str() << "Uploaded " << content.size() << " bytes to " << file_path));

    // 201 must point at the created resource. Mirror the on-disk layout back
    // onto the URL space: <location prefix><subdir>/<filename> — the same path a
    // subsequent GET or DELETE resolves to.
    std::string resource_url = Str() << conn.location->path << subdir << "/" << filename;
    return conn.send(Response(201).header("Location", resource_url));
}
