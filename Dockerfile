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

RUN make -j"$(nproc)"

FROM debian:bookworm-slim

ARG VERSION=dev
LABEL org.opencontainers.image.title="servant" \
      org.opencontainers.image.description="A single-threaded, poll-driven HTTP server written in C++." \
      org.opencontainers.image.source="https://github.com/stepskop/servant" \
      org.opencontainers.image.url="https://github.com/stepskop/servant" \
      org.opencontainers.image.documentation="https://github.com/stepskop/servant#docker" \
      org.opencontainers.image.licenses="MIT" \
      org.opencontainers.image.version="${VERSION}"

RUN apt-get update \
 && apt-get install -y --no-install-recommends libstdc++6 \
 && rm -rf /var/lib/apt/lists/*

# Unprivileged runtime user. The default config listens on 8080 because a
# non-root process cannot bind below 1024.
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

USER servant
WORKDIR /var/www/html
EXPOSE 8080

# No HEALTHCHECK: the runtime image ships no HTTP client to probe with. Add one
# in your own image, or use an orchestrator-side probe against GET /.
ENTRYPOINT ["servant"]
CMD ["/etc/servant/servant.conf"]
