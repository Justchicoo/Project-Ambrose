#!/bin/sh
# Project Ambrose by Imjustchico
# Creates first-start configuration from the mounted client and starts the supervisor in the foreground for Docker lifecycle handling.

set -eu

config_dir=/var/lib/ambrose/config
mkdir -p "$config_dir" /var/lib/ambrose/data /var/lib/ambrose/logs /var/lib/ambrose/backups

if [ ! -f "$config_dir/supervisor.conf" ]; then
    cp /opt/ambrose/conf/supervisor.conf.dist "$config_dir/supervisor.conf"
    sed -i \
        -e 's#^LogsDir = .*#LogsDir = /var/lib/ambrose/logs#' \
        -e 's#^Supervisor.StateFile =.*#Supervisor.StateFile = /var/lib/ambrose/data/supervisor-state.json#' \
        -e 's#^Supervisor.OutputDir =.*#Supervisor.OutputDir = /var/lib/ambrose/logs/apps#' \
        -e 's#^Supervisor.HistoryFile =.*#Supervisor.HistoryFile = /var/lib/ambrose/data/supervisor-history.json#' \
        -e 's#^Panel.StoreFile =.*#Panel.StoreFile = /var/lib/ambrose/data/panel.sqlite#' \
        "$config_dir/supervisor.conf"
fi

exec /opt/ambrose/bin/supervisor --config "$config_dir/supervisor.conf"
