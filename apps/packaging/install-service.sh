#!/bin/sh
# Project Ambrose by Imjustchico
# Installs the packaged systemd unit and its dedicated runtime user without starting the service unexpectedly.

set -eu

prefix=${AMBROSE_PREFIX:-/opt/ambrose}
config=${AMBROSE_CONFIG:-/etc/ambrose}

if [ "$(id -u)" -ne 0 ]; then
    echo "install-service.sh must run as root" >&2
    exit 1
fi

if ! id ambrose >/dev/null 2>&1; then
    useradd --system --home-dir /var/lib/ambrose --create-home --shell /usr/sbin/nologin ambrose
fi
install -d -o ambrose -g ambrose /var/lib/ambrose /var/log/ambrose "$config"
install -m 0644 apps/packaging/ambrose.service.txt /etc/systemd/system/ambrose.service
install -m 0644 conf/dist/supervisor.conf.dist "$config/supervisor.conf"
sed -i "s#^ExecStart=.*#ExecStart=$prefix/bin/supervisor --config $config/supervisor.conf#" /etc/systemd/system/ambrose.service
systemctl daemon-reload
systemctl enable ambrose.service
echo "Installed ambrose.service; start it with: systemctl start ambrose"
