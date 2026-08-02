# servant

![servant](https://raw.githubusercontent.com/stepskop/servant/main/assets/banner-light.png)

A single-threaded HTTP server written in C++. Every socket is non-blocking and
multiplexed through one `poll()` loop — no thread per connection, no blocking
I/O. Serves static files, accepts uploads, and runs CGI scripts.

Source and full documentation: **https://github.com/stepskop/servant**

## Quick start

Serve a directory from your machine:

```sh
docker run --rm -p 8080:8080 -v /path/to/site:/var/www/html stepskop/servant
```

Or build your own image on top:

```dockerfile
FROM stepskop/servant

COPY site.conf /etc/servant/servant.conf
COPY public/   /var/www/html/
```

## Tags

| Tag | |
|-----|--|
| `latest` | newest release, Debian bookworm-slim |
| `alpine` | newest release, Alpine — smaller, but musl libc |
| `unprivileged` | `latest`, running as uid 10001 |
| `alpine-unprivileged` | `alpine`, running as uid 10001 |

Every release also publishes pinnable versions of all four — `1.2.3`, `1.2` and
`1`, each with the same suffix. Pin to `1` for patch and minor updates without a
breaking change, or to an exact version for reproducible builds.

Built for `linux/amd64` and `linux/arm64`, and mirrored to
`ghcr.io/stepskop/servant`.

## What the image gives you

| | |
|---|---|
| Config | `/etc/servant/servant.conf` — static `GET` only by default |
| Document root | `/var/www/html` — a placeholder page and a custom 404 |
| Port | `8080`, bound on `0.0.0.0` |
| User | `root`, or `servant` (uid/gid `10001`) on the `-unprivileged` tags |
| Binary | `/usr/local/bin/servant` |
| Entrypoint | `["servant"]`, with the config path as `CMD` |

Overwrite those first two paths and you have your own server. Both variants
listen on 8080, so the port does not change when you switch between them — map
it on the host with `-p 80:8080` if you want port 80.

## Which variant

The default tags run as root, which keeps mounted directories writable whatever
the host owns them as. The `-unprivileged` tags run as uid 10001 instead: reach
for those when the platform requires it — Kubernetes clusters on the restricted
Pod Security Standard reject root images outright — or simply to reduce what a
bug in the server could reach.

The trade-off is writes. A bind mount carries the host's ownership through
unchanged, so on an `-unprivileged` image a directory owned by your own account
is readable but not writable and uploads fail. Either run as yourself, or hand
the directory over:

```sh
docker run --user "$(id -u):$(id -g)" ...    # run as yourself
sudo chown -R 10001:10001 ./uploads          # or give it to the image's user
```

Both also leave files the server writes owned by you rather than by root, which
the default image does not.

## Configuration

nginx-like grammar. A config that serves static files, accepts uploads and runs
Python CGI:

```nginx
server {
    listen 0.0.0.0:8080;
    server_name localhost;

    root /var/www/html;
    index index.html;
    client_max_body_size 1m;

    error_page 404 /errors/404.html;

    location / {
        methods GET;
        autoindex off;
    }

    location /uploads {
        methods GET POST DELETE;
        root /var/www/html;
        autoindex on;
        upload_store /var/www/html/uploads;
        client_max_body_size 10m;
    }

    location /scripts {
        methods GET POST;
        alias /var/www/html/cgi-bin;
        cgi .py /usr/bin/python3;
    }
}
```

No CGI interpreter ships in the image — install the one you need:

```dockerfile
RUN apt-get update \
 && apt-get install -y --no-install-recommends python3 \
 && rm -rf /var/lib/apt/lists/*
```

Two environment variables affect the process itself: `LOG_LEVEL`
(`debug`/`info`/`warn`/`error`) and `NO_COLOR`, which drops the ANSI escapes
from the logs.

The [full directive reference, a worked example, and notes on mounting][docs]
live in the repository.

[docs]: https://github.com/stepskop/servant#docker

## License

MIT. Source at [github.com/stepskop/servant](https://github.com/stepskop/servant).
