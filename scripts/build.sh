#!/usr/bin/env bash
# Build and install theDesk together with its vendored dependencies (deps/).
#
# Usage:
#   scripts/build.sh                 # build + install everything, in order
#   scripts/build.sh --from thedesk  # start at a component (skip earlier ones)
#   scripts/build.sh --only libtdesktopenvironment
#   scripts/build.sh --no-install    # configure + build only (needs deps installed)
#   scripts/build.sh --uninstall     # remove everything installed by this script
#   scripts/build.sh --list          # show component order
#
# Environment:
#   PREFIX=/usr  BUILD_TYPE=RelWithDebInfo  BUILD_DIR=<repo>/build  JOBS=$(nproc)
#   BUILD_WAYLAND=OFF   (libtdesktopenvironment's Wayland backend; X11 session only)
#
# Each component is installed (with sudo) before the next one is configured,
# because later components find the earlier ones with find_package().
# CMake writes build/<component>/install_manifest.txt, used by --uninstall.
# FORCE_STABLE: libcontemporary otherwise builds "Blueprint" (dev) variants,
# with -blueprint binary names, whenever the git HEAD has no release tag.

set -euo pipefail

ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
BUILD_DIR=${BUILD_DIR:-$ROOT/build}
PREFIX=${PREFIX:-/usr}
BUILD_TYPE=${BUILD_TYPE:-RelWithDebInfo}
JOBS=${JOBS:-$(nproc)}

# name:source-dir, in dependency order
COMPONENTS=(
    "libcontemporary:deps/libcontemporary"
    "contemporary-theme:deps/contemporary-theme"
    "libtdesktopenvironment:deps/libtdesktopenvironment"
    "libthefrisbee:deps/libthefrisbee"
    "thefile:deps/thefile"
    "thedesk-xdg-utils:deps/thedesk-xdg-utils"
    "thedesk:."
    "thecalculator:apps/thecalculator"
    "theheartbeat:apps/theheartbeat"
)

log()  { printf '\033[1;33m==>\033[0m %s\n' "$*"; }
warn() { printf '\033[1;31m!!\033[0m %s\n' "$*" >&2; }

install_step=true
only=""
from=""
mode=build

while (($#)); do
    case $1 in
        --no-install) install_step=false ;;
        --only) only=${2:?--only needs a component}; shift ;;
        --from) from=${2:?--from needs a component}; shift ;;
        --uninstall) mode=uninstall ;;
        --list) for c in "${COMPONENTS[@]}"; do echo "${c%%:*}"; done; exit 0 ;;
        -h|--help) sed -n '2,19p' "$0"; exit 0 ;;
        *) warn "Unknown option: $1"; exit 2 ;;
    esac
    shift
done

known() { local c; for c in "${COMPONENTS[@]}"; do [[ ${c%%:*} == "$1" ]] && return 0; done; return 1; }
for c in "$only" "$from"; do
    if [[ -n $c ]] && ! known "$c"; then warn "Unknown component: $c (see --list)"; exit 2; fi
done

build_component() {
    local name=$1 src=$ROOT/$2 bdir=$BUILD_DIR/$1
    log "[$name] configure"
    cmake -S "$src" -B "$bdir" -G Ninja \
        -DCMAKE_BUILD_TYPE="$BUILD_TYPE" \
        -DCMAKE_INSTALL_PREFIX="$PREFIX" \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
        -DQT_VERSION_MAJOR=6 \
        -DKF_VERSION_MAJOR=6 \
        -DQT_FIND_PRIVATE_MODULES=ON \
        -DFORCE_STABLE=ON \
        -DTDESKTOPENVIRONMENT_BUILD_WAYLAND="${BUILD_WAYLAND:-OFF}"
    log "[$name] build"
    cmake --build "$bdir" -j "$JOBS"
    if $install_step; then
        log "[$name] install to $PREFIX"
        sudo cmake --install "$bdir"
        sudo ldconfig
    fi
}

uninstall_component() {
    local name=$1 manifest=$BUILD_DIR/$1/install_manifest.txt
    if [[ ! -f $manifest ]]; then
        warn "[$name] no install manifest, skipping"
        return
    fi
    log "[$name] uninstall ($(wc -l <"$manifest") files)"
    xargs -d '\n' -r sudo rm -f -- <"$manifest"
}

if [[ $mode == uninstall ]]; then
    # Reverse order so dependents go first
    for ((i = ${#COMPONENTS[@]} - 1; i >= 0; i--)); do
        name=${COMPONENTS[i]%%:*}
        [[ -n $only && $name != "$only" ]] && continue
        uninstall_component "$name"
    done
    sudo ldconfig
    exit 0
fi

started=false
[[ -z $from ]] && started=true
for c in "${COMPONENTS[@]}"; do
    name=${c%%:*} dir=${c#*:}
    [[ $name == "$from" ]] && started=true
    $started || continue
    [[ -n $only && $name != "$only" ]] && continue
    build_component "$name" "$dir"
done

log "Done."
