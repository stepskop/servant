# Linux build check

Compile `webserv` in a Linux container with GNU **g++**
Catches Mac-only code (kqueue / `<sys/event.h>`,
clang-only extensions, glibc vs libc++ differences) that builds fine on macOS
but breaks on Linux.

## Requirements
- Docker running.

## Usage
```sh
tools/linux-build/linux-build.sh             # make re (clean rebuild) on Linux
tools/linux-build/linux-build.sh make        # plain build
tools/linux-build/linux-build.sh make clean  # any make target
tools/linux-build/linux-build.sh bash        # interactive Linux shell in the repo
```
