#!/usr/bin/env bash
# Compile webserv inside a Linux container with GNU g++.
# Catches Mac-only code (kqueue, <sys/event.h>, clang-only
# extensions) that compiles silently on macOS but breaks on Linux.
#
# Usage:
#   tools/linux-build/linux-build.sh            # make re  (clean rebuild)
#   tools/linux-build/linux-build.sh make       # plain build
#   tools/linux-build/linux-build.sh make clean # any make target
#   tools/linux-build/linux-build.sh bash       # drop into a Linux shell

set -euo pipefail

IMAGE="webserv-linux-build"
# Repo root = two levels up from this script (tools/linux-build/).
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_ROOT="$(cd "$SCRIPT_DIR/../.." && pwd)"

echo ">> Building Linux image ($IMAGE)..."
docker build -q -t "$IMAGE" "$SCRIPT_DIR" >/dev/null

# Default action: clean rebuild via the project Makefile.
CMD=("make" "re")
if [ "$#" -gt 0 ]; then
    CMD=("$@")
fi

# Only allocate a TTY when actually attached to one (interactive `bash`);
# otherwise docker errors out under pipes/CI.
TTY_FLAGS=("-i")
if [ -t 0 ] && [ -t 1 ]; then
    TTY_FLAGS=("-i" "-t")
fi

echo ">> Running in Linux container: ${CMD[*]}"
docker run --rm -p 8080:8080 "${TTY_FLAGS[@]}" \
    -v "$REPO_ROOT:/webserv" \
    "$IMAGE" \
    "${CMD[@]}"

echo ">> Done. Linux build succeeded."
