#!/usr/bin/env bash
# Render the horizon wallpapers (same scenes as the startdesk splash) into
# dots/home/.local/share/backgrounds/theDesk/. Needs g++ and Qt6 dev files.
#   scripts/render-wallpapers.sh [WIDTHxHEIGHT]   (default 1920x1080)
set -euo pipefail
ROOT=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
SIZE=${1:-1920x1080}
OUT=$ROOT/dots/home/.local/share/backgrounds/theDesk
BIN=$ROOT/build/tools/horizon-wallpaper
mkdir -p "$(dirname "$BIN")"
g++ -std=c++17 -O2 -fPIC -DLIBTHEDESK_LIBRARY -I"$ROOT/libthedesk" -I"$ROOT/libthedesk/horizon" \
    "$ROOT/tools/horizon-wallpaper/main.cpp" "$ROOT/libthedesk/horizon/horizonscene.cpp" \
    $(pkg-config --cflags --libs Qt6Gui) -o "$BIN"
"$BIN" "$OUT" "$SIZE"
