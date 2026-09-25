#!/usr/bin/env bash
# Project Ambrose by Imjustchico
# Installs, configures and runs an Ambrose checkout on Linux.
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

deps() {
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
    cmake --install "$BUILD_DIR" --config "$BUILD_TYPE" --prefix "$PREFIX"
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
    exec "$BIN_DIR/$1" --config "$BIN_DIR/$1.conf"
}

case "${1:-}" in
    deps) deps ;;
    compile) compile ;;
    conf) conf ;;
    db) db ;;
    run) [[ $# -eq 2 ]] || fail "run expects one app"; run_app "$2" ;;
    *) fail "usage: ambrose.sh {deps|compile|conf|db|run <app>}" ;;
esac
