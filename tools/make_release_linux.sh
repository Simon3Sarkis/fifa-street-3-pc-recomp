#!/bin/bash
# Build the Linux / Steam Deck release: dist/fifastreet3-<version>-linux-x86_64.tar.gz
# Run inside the WSL Ubuntu build tree (/root/fs3, synced from the Windows checkout):
#   bash tools/make_release_linux.sh [version]
# Contents: fifastreet3 (finds its libraries next to itself, RUNPATH $ORIGIN), librexruntime /
# librexgpu-xenos, fifastreet3.toml (the shipped settings), recomplauncher.json, portable.txt (saves
# and logs next to the binary), README.txt, the project licence and every bundled library's licence.
# No game files, no test tools. Built on Ubuntu 24.04: needs glibc 2.39 or newer (SteamOS 3.9 has 2.43).
set -eu
cd "$(dirname "$0")/.."
VERSION="${1:-0.0.1}"
BUILD="${BUILD:-out/build/linux-amd64-relwithdebinfo}"
SDK_OUT="${SDK_OUT:-third_party/rexglue-sdk/out/linux-amd64}"
NAME="fifastreet3-$VERSION-linux-x86_64"
OUT="dist/$NAME"
rm -rf "$OUT" && mkdir -p "$OUT/licenses"
cp "$BUILD/fifastreet3" "$SDK_OUT"/librexruntime*.so "$SDK_OUT"/librexgpu-xenos*.so "$OUT/"
strip --strip-debug "$OUT/fifastreet3" "$OUT"/*.so
cp config/fifastreet3.runtime.toml "$OUT/fifastreet3.toml"
cp packaging/recomplauncher.json packaging/README.txt "$OUT/"
printf '%s' "$VERSION" > "$OUT/VERSION"
: > "$OUT/portable.txt"
cp LICENSE "$OUT/licenses/fifastreet3-pc-port-GPL-3.0.txt"
cp third_party/rexglue-sdk/LICENSE "$OUT/licenses/rexglue-sdk.txt"
for dir in third_party/rexglue-sdk/thirdparty/*/; do
  lib=$(basename "$dir")
  found=$(find "$dir" -maxdepth 1 -type f -iregex '.*/\(license\|copying\|copyright\|notice\|unlicense\)[^/]*' | head -3)
  [ -n "$found" ] || found=$(find "$dir" -maxdepth 2 -type f -iregex '.*/\(license\|copying\|copyright\|unlicense\)[^/]*' | head -3)
  for f in $found; do cp "$f" "$OUT/licenses/$lib-$(basename "$f")"; done
done
tar -C dist -czf "dist/$NAME.tar.gz" "$NAME"
echo "release: dist/$NAME.tar.gz ($(du -h "dist/$NAME.tar.gz" | cut -f1))"
