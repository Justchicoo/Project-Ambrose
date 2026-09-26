# Project Ambrose by Imjustchico
# Builds Ambrose in a reproducible C++20 stage and copies the runtime servers and their configuration into a small image with time-zone data.

FROM ubuntu:24.04 AS build

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends build-essential cmake ninja-build git pkg-config curl zip unzip tar \
    && git clone --depth 1 https://github.com/microsoft/vcpkg.git /opt/vcpkg \
    && /opt/vcpkg/bootstrap-vcpkg.sh -disableMetrics \
    && rm -rf /var/lib/apt/lists/*
ENV VCPKG_ROOT=/opt/vcpkg

WORKDIR /src
COPY . .
RUN cmake --preset linux-gcc \
    && cmake --build build/linux-gcc --config RelWithDebInfo --parallel \
    --target supervisor loginserver gameserver patchserver \
    && mkdir -p /runtime/lib \
    && find build/linux-gcc/vcpkg_installed/x64-linux/lib -maxdepth 1 -name '*.so*' -exec cp -a {} /runtime/lib/ \;
RUN g++ -std=c++20 -O2 apps/packaging/check-timezones.cpp -o /runtime/check-timezones

FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive
RUN apt-get update \
    && apt-get install -y --no-install-recommends ca-certificates tzdata mariadb-client \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --create-home --home-dir /var/lib/ambrose --shell /usr/sbin/nologin ambrose

COPY --from=build /src/build/linux-gcc/bin/RelWithDebInfo/ /opt/ambrose/bin/
COPY --from=build /runtime/lib/ /opt/ambrose/lib/
COPY apps/packaging/docker-entrypoint.sh /opt/ambrose/docker-entrypoint.sh
COPY --from=build /runtime/check-timezones /opt/ambrose/check-timezones

RUN mkdir -p /var/lib/ambrose/config /var/lib/ambrose/data /var/lib/ambrose/logs /var/lib/ambrose/backups /var/lib/ambrose/client \
    && chmod +x /opt/ambrose/docker-entrypoint.sh \
    && /opt/ambrose/check-timezones \
    && chown -R ambrose:ambrose /var/lib/ambrose /opt/ambrose

USER ambrose
WORKDIR /var/lib/ambrose
ENV PATH="/opt/ambrose/bin:${PATH}"
ENV LD_LIBRARY_PATH="/opt/ambrose/lib:${LD_LIBRARY_PATH}"
EXPOSE 12000 12333 12500
STOPSIGNAL SIGTERM
HEALTHCHECK --interval=15s --timeout=5s --start-period=30s --retries=5 \
    CMD /opt/ambrose/bin/supervisor --config /var/lib/ambrose/config/supervisor.conf --check
ENTRYPOINT ["/opt/ambrose/docker-entrypoint.sh"]
