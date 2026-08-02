# A site built on the servant base image, showing what extending it looks like:
# a document root and config of your own, plus an interpreter the base image
# does not ship. Self-contained — this directory is the whole build context:
#
#   cd example
#   docker build -t servant-example .
#   docker run --rm -p 8080:8080 servant-example
#
# Then open http://localhost:8080 — every link on the page is a different
# feature of servant.conf.

# Overridable so CI can build against a base image built from the same commit.
ARG BASE=stepskop/servant:1
FROM ${BASE}

# CGI needs an interpreter on the filesystem. Root is only needed for the
# install; the USER line below restores the unprivileged runtime user.
USER root
RUN apt-get update \
 && apt-get install -y --no-install-recommends python3 \
 && rm -rf /var/lib/apt/lists/*

# Relative paths in a config are taken from the directory holding it, so laying
# this tree down as-is is enough — no working directory to line up, and the same
# file still works for a local `./webserv example/servant.conf` run.
COPY --chown=servant:servant . /site/

USER servant

# Overrides the base image's CMD, which points at /etc/servant/servant.conf.
CMD ["/site/servant.conf"]
