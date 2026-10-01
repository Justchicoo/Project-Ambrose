#!/usr/bin/env bash
# Project Ambrose by Imjustchico
# Follows doc/INSTALL.md on a clean Ubuntu 24.04: from the host it starts a fresh ubuntu:24.04 container holding a copy of this checkout and the user's own Wizard101 install, read-only, and inside it runs deps --install --with-database, compile, conf and db, starts loginserver, gameserver and patchserver, and passes only when each logs '<app> ready'. AMBROSE_CLIENT_DIR names the install, AMBROSE_TYPE_DUMP_PATH may name its type dump so the first start need not extract one, and AMBROSE_CLEAN_TIMEOUT bounds the wait for ready in seconds. compile, conf and db each keep a log, and the first to fail prints the end of its own and of compile's.
set -euo pipefail

READY_APPS=(loginserver gameserver patchserver)

step() {
    AMBROSE_BUILD_TYPE=Release apps/installer/ambrose.sh "$1" > "/tmp/$1.log" 2>&1 && return
    printf '%s failed; the end of its log:\n' "$1"
    tail -60 "/tmp/$1.log"
    [[ "$1" == compile ]] || { printf 'the end of the compile log:\n'; tail -30 /tmp/compile.log; }
    exit 1
}

inside() {
    mkdir -p /work
    tar -C /source --exclude=./build --exclude=./node_modules --exclude=./env --exclude=./.git -cf - . | tar -C /work -xf -
    cd /work
    apps/installer/ambrose.sh deps --install --with-database
    export VCPKG_ROOT="$HOME/vcpkg"
    apps/installer/ambrose.sh deps
    step compile
    printf 'compile installed: %s\n' "$(ls env/dist/bin 2>/dev/null | tr '\n' ' ')"
    step conf
    step db
    export AMBROSE_CLIENT_DIR=/client
    [[ -f /dump.json ]] && export AMBROSE_TYPE_DUMP_PATH=/dump.json
    for app in "${READY_APPS[@]}"; do
        AMBROSE_BUILD_TYPE=Release apps/installer/ambrose.sh run "$app" < /dev/null > "/tmp/$app.log" 2>&1 &
    done
    local until=$((SECONDS + ${AMBROSE_CLEAN_TIMEOUT:-900}))
    local waiting=("${READY_APPS[@]}")
    while [[ ${#waiting[@]} -gt 0 && $SECONDS -lt $until ]]; do
        local still=()
        for app in "${waiting[@]}"; do
            grep -q "$app ready" "/tmp/$app.log" || still+=("$app")
        done
        waiting=("${still[@]+"${still[@]}"}")
        [[ ${#waiting[@]} -eq 0 ]] || sleep 5
    done
    for app in "${READY_APPS[@]}"; do
        if grep -q "$app ready" "/tmp/$app.log"; then
            printf '%s: %s\n' "$app" "$(grep -m1 "$app ready" "/tmp/$app.log")"
        else
            printf '%s never logged ready:\n' "$app"
            tail -20 "/tmp/$app.log"
        fi
    done
    if [[ ${#waiting[@]} -ne 0 ]]; then
        printf 'clean Ubuntu did not reach ready on %s\n' "${waiting[*]}"
        exit 1
    fi
    printf 'clean Ubuntu reached ready on loginserver, gameserver and patchserver\n'
}

if [[ "${1:-}" == "--inside" ]]; then
    inside
    exit
fi

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../../.." && pwd)"
[[ -n "${AMBROSE_CLIENT_DIR:-}" ]] || { printf 'clean_ubuntu.sh: set AMBROSE_CLIENT_DIR to your own Wizard101 install\n' >&2; exit 2; }
command -v docker >/dev/null 2>&1 || { printf 'clean_ubuntu.sh: needs Docker\n' >&2; exit 2; }
HOST_ROOT="$(cd "$ROOT" && { pwd -W 2>/dev/null || pwd; })"
export MSYS_NO_PATHCONV=1
mounts=(-v "$HOST_ROOT:/source:ro" -v "$AMBROSE_CLIENT_DIR:/client:ro")
[[ -n "${AMBROSE_TYPE_DUMP_PATH:-}" ]] && mounts+=(-v "$AMBROSE_TYPE_DUMP_PATH:/dump.json:ro")
exec docker run --rm "${mounts[@]}" -e AMBROSE_CLEAN_TIMEOUT ubuntu:24.04 bash /source/apps/installer/tests/clean_ubuntu.sh --inside
