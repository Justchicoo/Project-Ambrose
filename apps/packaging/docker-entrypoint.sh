#!/bin/sh
# Project Ambrose by Imjustchico
# Creates first-start configuration from the mounted client and starts the supervisor in the foreground for Docker lifecycle handling.

set -eu
umask 027

config_dir=/var/lib/ambrose/config
db_user=ambrose
db_password=${MARIADB_PASSWORD:?Set MARIADB_PASSWORD for the Ambrose database user}
case "$db_user$db_password" in
    *[!A-Za-z0-9._-]*)
        echo "MARIADB_USER and MARIADB_PASSWORD may contain only letters, digits, dot, underscore, and hyphen" >&2
        exit 1
        ;;
esac
mkdir -p "$config_dir" /var/lib/ambrose/data /var/lib/ambrose/logs /var/lib/ambrose/backups

if [ ! -f "$config_dir/supervisor.conf" ]; then
    cp /opt/ambrose/bin/supervisor.conf.dist "$config_dir/supervisor.conf"
    sed -i \
        -e 's#^LogsDir = .*#LogsDir = /var/lib/ambrose/logs#' \
        -e 's#^Supervisor.StateFile =.*#Supervisor.StateFile = /var/lib/ambrose/data/supervisor-state.json#' \
        -e 's#^Supervisor.OutputDir =.*#Supervisor.OutputDir = /var/lib/ambrose/logs/apps#' \
        -e 's#^Supervisor.HistoryFile =.*#Supervisor.HistoryFile = /var/lib/ambrose/data/supervisor-history.json#' \
        -e 's#^Panel.StoreFile =.*#Panel.StoreFile = /var/lib/ambrose/data/panel.sqlite#' \
        "$config_dir/supervisor.conf"
fi

for app in loginserver gameserver patchserver; do
    if [ ! -f "$config_dir/$app.conf" ]; then
        cp "/opt/ambrose/bin/$app.conf.dist" "$config_dir/$app.conf"
        sed -i "s#^ClientDir =.*#ClientDir = /var/lib/ambrose/client#" "$config_dir/$app.conf"
    fi
done

for config in "$config_dir"/loginserver.conf "$config_dir"/gameserver.conf; do
    sed -i \
        -e "s#127\.0\.0\.1;3306;ambrose;ambrose;ambrose_login#database;3306;$db_user;$db_password;ambrose_login#" \
        -e "s#127\.0\.0\.1;3306;ambrose;ambrose;ambrose_characters#database;3306;$db_user;$db_password;ambrose_characters#" \
        -e "s#127\.0\.0\.1;3306;ambrose;ambrose;ambrose_world#database;3306;$db_user;$db_password;ambrose_world#" \
        "$config"
done
sed -i \
    -e "s#^App.loginserver.Config =.*#App.loginserver.Config = $config_dir/loginserver.conf#" \
    -e "s#^App.gameserver.Config =.*#App.gameserver.Config = $config_dir/gameserver.conf#" \
    -e "s#^App.patchserver.Config =.*#App.patchserver.Config = $config_dir/patchserver.conf#" \
    -e 's#^App.loginserver.WorkingDirectory =.*#App.loginserver.WorkingDirectory = /var/lib/ambrose#' \
    -e 's#^App.gameserver.WorkingDirectory =.*#App.gameserver.WorkingDirectory = /var/lib/ambrose#' \
    -e 's#^App.patchserver.WorkingDirectory =.*#App.patchserver.WorkingDirectory = /var/lib/ambrose#' \
    "$config_dir/supervisor.conf"

chmod 0640 "$config_dir/supervisor.conf" "$config_dir/loginserver.conf" "$config_dir/gameserver.conf" "$config_dir/patchserver.conf"

exec /opt/ambrose/bin/supervisor --config "$config_dir/supervisor.conf"
