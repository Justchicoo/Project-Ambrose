#!/bin/sh
# Project Ambrose by Imjustchico
# Delegates Linux service removal to the supervisor's native service installer.

set -eu

prefix=${AMBROSE_PREFIX:-/opt/ambrose}
exec "$prefix/bin/supervisor" --uninstall-service
