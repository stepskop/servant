# syntax=docker/dockerfile:1

# servant — Debian variant. See Dockerfile.alpine for the smaller musl build.
#
# Extend it by overwriting the two paths that make up the image API:
#
#   FROM stepskop/servant:1
#   COPY site.conf /etc/servant/servant.conf
#   COPY public/   /var/www/html/

FROM debian:bookworm-slim AS build

RUN apt-get update \
 && apt-get install -y --no-install-recommends g++ make \
 && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY Makefile ./
COPY include/ include/
COPY src/     src/

# The version the banner reports. Only the sources are copied in, so the
# Makefile's `git describe` finds no checkout to read — release.yml passes the
# number here as a build arg, and a plain `docker build` gets "dev".
ARG VERSION=dev
RUN make -j"$(nproc)" VERSION="$VERSION"

FROM debian:bookworm-slim

ARG VERSION=dev
# `maintainer` is the conventional label docker inspect surfaces; `authors` is
# the OCI-native spelling of the same thing.
LABEL maintainer="Stepan Skopek (https://github.com/stepskop)" \
      org.opencontainers.image.authors="Stepan Skopek (https://github.com/stepskop)" \
      org.opencontainers.image.title="servant" \
      org.opencontainers.image.description="A single-threaded, poll-driven HTTP server written in C++." \
      org.opencontainers.image.source="https://github.com/stepskop/servant" \
      org.opencontainers.image.url="https://github.com/stepskop/servant" \
      org.opencontainers.image.documentation="https://github.com/stepskop/servant#docker" \
      org.opencontainers.image.licenses="MIT" \
      org.opencontainers.image.version="${VERSION}"

RUN apt-get update \
 && apt-get install -y --no-install-recommends libstdc++6 \
 && rm -rf /var/lib/apt/lists/*

# The user the -unprivileged variant runs as. Created in both variants so the
# document root has a consistent owner either way. 8080 is the listen port for
# both, since this user cannot bind below 1024.
RUN groupadd --system --gid 10001 servant \
 && useradd --system --uid 10001 --gid servant \
            --no-create-home --shell /usr/sbin/nologin servant

COPY --from=build /src/webserv    /usr/local/bin/servant
COPY default.conf                 /etc/servant/servant.conf

# Placeholder document root: a landing page, its stylesheet and the 404 the
# default config points at. Not declared as a VOLUME on purpose — that would
# shadow whatever a downstream image copies underneath it.
COPY --chown=servant:servant www/ /var/www/html/

RUN chown -R servant:servant /etc/servant

# Which user the server runs as. Two images are published from this one file:
# the default runs as root, so a bind-mounted directory is writable whatever the
# host owns it as; the -unprivileged tags are built with RUNTIME_USER=servant for
# platforms that reject root images. Both listen on 8080.
ARG RUNTIME_USER=root
USER ${RUNTIME_USER}
WORKDIR /var/www/html
EXPOSE 8080

# No HEALTHCHECK: the runtime image ships no HTTP client to probe with. Add one
# in your own image, or use an orchestrator-side probe against GET /.
ENTRYPOINT ["servant"]
CMD ["/etc/servant/servant.conf"]
