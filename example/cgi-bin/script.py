#!/usr/bin/env python3
# Minimal CGI script: emits an application/json response.
# Headers, a blank line, then the body — exactly what parse_cgi_output expects.
# Content-Length is intentionally omitted; the server computes it.
import json
import os
import sys

# Read the request body (if any) from stdin. CONTENT_LENGTH bounds it so we
# don't block waiting for an EOF that the server may delay.
raw = ""
length = os.environ.get("CONTENT_LENGTH", "")
if length.isdigit() and int(length) > 0:
    raw = sys.stdin.read(int(length))

payload = {
    "message": "Hello from CGI",
    "body": raw,
    "env": dict(os.environ),
}

body = json.dumps(payload)

sys.stdout.write("Content-Type: application/json\r\n")
sys.stdout.write("\r\n")
sys.stdout.write(body)
