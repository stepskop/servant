# Linux build environment
# Debian stable ships glibc + a real g++, so Mac-only APIs (kqueue,
# <sys/event.h>, etc.) fail to compile here exactly as they would on Linux.
FROM debian:stable-slim

RUN apt-get update && apt-get install -y --no-install-recommends \
        g++ \
        make \
        valgrind \
        ca-certificates \
        python3 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /webserv

CMD ["make", "re"]
