#!/bin/sh
# Project Ambrose by Imjustchico
# Delegates Linux service registration to the supervisor's native service installer.

set -eu

prefix=${AMBROSE_PREFIX:-/opt/ambrose}
exec "$prefix/bin/supervisor" --install-service
