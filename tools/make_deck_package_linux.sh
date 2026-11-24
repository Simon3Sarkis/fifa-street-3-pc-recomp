#!/bin/bash
# Package the native Linux build as a portable folder for the Steam Deck (no Proton).
# Run inside the WSL Ubuntu build tree (/root/fs3, synced from the Windows checkout):
#   bash tools/make_deck_package_linux.sh [build dir] [out dir]
# Contents: fifastreet3 + librexruntime / librexgpu-xenos, fifastreet3.toml (from
# config/fifastreet3.runtime.toml), portable.txt (saves in userdata/ next to the binary), tools/deck
# and the test input scripts. No game files: run_native.sh links the Proton package's game/ folder.
set -eu
cd "$(dirname "$0")/.."
BUILD="${1:-out/build/linux-amd64-relwithdebinfo}"
OUT="${2:-dist/fifastreet3-deck-linux}"
rm -rf "$OUT" && mkdir -p "$OUT"
# The SDK writes its libraries to its own out/ folder; the copies staged next to the game binary are
# only refreshed when the binary relinks, so a plugin-only change would ship stale.
SDK_OUT="${SDK_OUT:-third_party/rexglue-sdk/out/linux-amd64}"
cp "$BUILD/fifastreet3" "$SDK_OUT"/librexruntime*.so "$SDK_OUT"/librexgpu-xenos*.so "$OUT/"
cp config/fifastreet3.runtime.toml "$OUT/fifastreet3.toml"
: > "$OUT/portable.txt"
mkdir -p "$OUT/runs" "$OUT/snaps"
mkdir -p "$OUT/tools/deck" "$OUT/tests"
cp tools/deck/*.sh "$OUT/tools/deck/"
cp -r tests/input "$OUT/tests/"
echo "package: $OUT ($(du -sh "$OUT" | cut -f1))"
