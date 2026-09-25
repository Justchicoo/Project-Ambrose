# Project Ambrose by Imjustchico
# Builds Ambrose in a reproducible C++20 stage and copies the runtime servers and their configuration into a small image with time-zone data.

FROM ubuntu:24.04 AS build

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake ninja-build git pkg-config \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src
COPY . .
RUN cmake --preset linux-gcc -S . -B build \
    && cmake --build build --parallel \
    --target supervisor loginserver gameserver patchserver

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates tzdata mariadb-client \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --home-dir /var/lib/ambrose --shell /usr/sbin/nologin ambrose

COPY --from=build /src/build/bin/ /opt/ambrose/bin/
COPY --from=build /src/conf/dist/ /opt/ambrose/conf/
COPY apps/packaging/docker-entrypoint.sh /opt/ambrose/docker-entrypoint.sh

RUN mkdir -p /var/lib/ambrose/{config,data,logs,backups,client} \
    && chown -R ambrose:ambrose /var/lib/ambrose /opt/ambrose

USER ambrose
WORKDIR /var/lib/ambrose
EXPOSE 12020 12080 12343 12500
STOPSIGNAL SIGTERM
HEALTHCHECK --interval=15s --timeout=5s --start-period=30s --retries=5 \
    CMD /opt/ambrose/bin/supervisor --config /var/lib/ambrose/config/supervisor.conf --check
ENTRYPOINT ["/opt/ambrose/docker-entrypoint.sh"]
