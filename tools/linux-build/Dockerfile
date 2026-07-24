# Linux build environment
# Debian stable ships glibc + a real g++, so Mac-only APIs (kqueue,
# <sys/event.h>, etc.) fail to compile here exactly as they would on Linux.
FROM debian:stable-slim

# Enable multi-arch
RUN dpkg --add-architecture amd64

RUN apt update && apt install -y --no-install-recommends \
        g++ \
        make \
        valgrind \
        siege \
        ca-certificates \
        python3 \
        netcat-traditional \
        curl \
        libc6:amd64 \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /webserv

CMD ["make", "re"]
