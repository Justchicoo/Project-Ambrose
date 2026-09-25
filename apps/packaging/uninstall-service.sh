#!/bin/sh
# Project Ambrose by Imjustchico
# Stops and removes the Ambrose systemd unit while preserving its state and configuration.

set -eu

if [ "$(id -u)" -ne 0 ]; then
    echo "uninstall-service.sh must run as root" >&2
    exit 1
fi

systemctl disable --now ambrose.service 2>/dev/null || true
rm -f /etc/systemd/system/ambrose.service
systemctl daemon-reload
echo "Removed ambrose.service; /var/lib/ambrose and /etc/ambrose were preserved"
