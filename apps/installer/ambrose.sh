#!/usr/bin/env bash
# Project Ambrose by Imjustchico
# Installs, configures and runs an Ambrose checkout on Linux: deps checks the tools and, with --install on an apt system, installs them and vcpkg, and with --with-database a local MariaDB holding the account the shipped configuration names.
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
ENV_FILE="${AMBROSE_INSTALL_ENV:-$ROOT/conf/dist/env.dist}"
INSTALL_PREFIX_OVERRIDE="${AMBROSE_INSTALL_PREFIX-}"
BUILD_TYPE_OVERRIDE="${AMBROSE_BUILD_TYPE-}"
PRESET_OVERRIDE="${AMBROSE_PRESET-}"
if [[ -f "$ENV_FILE" ]]; then
    set -a
    source "$ENV_FILE"
    set +a
fi

PREFIX="${INSTALL_PREFIX_OVERRIDE:-${AMBROSE_INSTALL_PREFIX:-$ROOT/env/dist}}"
case "$PREFIX" in
    /* | ?:[/\\]*) ;;
    *) PREFIX="$ROOT/$PREFIX" ;;
esac
BUILD_TYPE="${BUILD_TYPE_OVERRIDE:-${AMBROSE_BUILD_TYPE:-Debug}}"
PRESET="${PRESET_OVERRIDE:-${AMBROSE_PRESET:-linux-gcc}}"
BUILD_DIR="$ROOT/build/$PRESET"
BIN_DIR="$PREFIX/bin"
ETC_DIR="$PREFIX/etc"

fail() {
    printf 'ambrose installer: %s\n' "$1" >&2
    exit 1
}

need_command() {
    command -v "$1" >/dev/null 2>&1 || fail "missing '$1'; install it and run deps again"
}

SUDO=""
[[ "$(id -u)" -eq 0 ]] || SUDO="sudo"

install_tools() {
    command -v apt-get >/dev/null 2>&1 || fail "--install knows apt-get only; install GCC, CMake, Ninja, Git and vcpkg yourself, then run deps again"
    $SUDO apt-get update -qq
    DEBIAN_FRONTEND=noninteractive $SUDO apt-get install -y -qq build-essential cmake ninja-build git curl zip unzip tar pkg-config autoconf autoconf-archive automake libtool python3 bison flex linux-libc-dev patchelf >/dev/null
    if [[ -z "${VCPKG_ROOT:-}" ]]; then
        export VCPKG_ROOT="$HOME/vcpkg"
    fi
    if [[ ! -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]]; then
        git clone -q https://github.com/microsoft/vcpkg.git "$VCPKG_ROOT"
    fi
    [[ -x "$VCPKG_ROOT/vcpkg" ]] || "$VCPKG_ROOT/bootstrap-vcpkg.sh" -disableMetrics >/dev/null
    printf 'vcpkg is in %s; export VCPKG_ROOT=%s before compile\n' "$VCPKG_ROOT" "$VCPKG_ROOT"
}

install_database() {
    DEBIAN_FRONTEND=noninteractive $SUDO apt-get install -y -qq mariadb-server >/dev/null
    $SUDO service mariadb start >/dev/null 2>&1 || $SUDO systemctl start mariadb
    $SUDO mariadb -e "CREATE USER IF NOT EXISTS 'ambrose'@'localhost' IDENTIFIED BY 'ambrose'; CREATE USER IF NOT EXISTS 'ambrose'@'127.0.0.1' IDENTIFIED BY 'ambrose'; GRANT ALL PRIVILEGES ON \`ambrose\_%\`.* TO 'ambrose'@'localhost'; GRANT ALL PRIVILEGES ON \`ambrose\_%\`.* TO 'ambrose'@'127.0.0.1';"
    printf 'MariaDB is running with the ambrose account the shipped configuration names\n'
}

deps() {
    local install=0 database=0
    for option in "$@"; do
        case "$option" in
            --install) install=1 ;;
            --with-database) database=1 ;;
            *) fail "deps takes --install and --with-database" ;;
        esac
    done
    [[ "$install" -eq 1 ]] && install_tools
    [[ "$database" -eq 1 ]] && install_database
    need_command cmake
    need_command "$([[ "$PRESET" == *clang* ]] && printf clang++ || printf g++)"
    [[ -n "${VCPKG_ROOT:-}" ]] || fail "VCPKG_ROOT is not set; install vcpkg and export VCPKG_ROOT"
    [[ -f "$VCPKG_ROOT/scripts/buildsystems/vcpkg.cmake" ]] || fail "VCPKG_ROOT does not contain vcpkg"
    printf 'dependencies found: CMake, compiler and vcpkg; every library, such as OpenSSL, Botan and MariaDB, is supplied by vcpkg\n'
}

compile() {
    deps
    cmake --preset "$PRESET" "-DCMAKE_INSTALL_PREFIX=$PREFIX"
    local build_preset
    if [[ "$PRESET" == windows-* ]]; then
        build_preset="windows-$([[ "$BUILD_TYPE" == "Debug" ]] && printf debug || printf release)"
    else
        build_preset="${PRESET}-$([[ "$BUILD_TYPE" == "Debug" ]] && printf debug || printf release)"
    fi
    cmake --build --preset "$build_preset"
    cmake --install "$BUILD_DIR" --config "$([[ "$BUILD_TYPE" == "Debug" ]] && printf Debug || printf RelWithDebInfo)" --prefix "$PREFIX"
}

conf() {
    mkdir -p "$BIN_DIR"
    [[ -d "$ETC_DIR" ]] || fail "no installed configuration templates; run compile first"
    while IFS= read -r -d '' template; do
        target="$BIN_DIR/$(basename "${template%.dist}")"
        [[ -e "$target" ]] || cp "$template" "$target"
    done < <(find "$ETC_DIR" -maxdepth 1 -type f -name '*.conf.dist' -print0)
}

db() {
    conf
    "$BIN_DIR/dbimport" --config "$BIN_DIR/dbimport.conf"
}

run_app() {
    conf
    case "$1" in
        loginserver|gameserver|patchserver|supervisor) ;;
        *) fail "run expects loginserver, gameserver, patchserver or supervisor" ;;
    esac
    cd "$BIN_DIR"
    exec "$BIN_DIR/$1" --config "$BIN_DIR/$1.conf"
}

case "${1:-}" in
    deps) shift; deps "$@" ;;
    compile) compile ;;
    conf) conf ;;
    db) db ;;
    run) [[ $# -eq 2 ]] || fail "run expects one app"; run_app "$2" ;;
    *) fail "usage: ambrose.sh {deps [--install] [--with-database]|compile|conf|db|run <app>}" ;;
esac
