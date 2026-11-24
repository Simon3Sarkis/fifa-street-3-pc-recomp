#!/bin/bash
# Copy the Deck package to the Steam Deck over SSH (run on the PC from Git Bash).
# Usage: tools/deck/push.sh [--with-game]
#   DECK=deck@<ip>      the Deck (default from tools/deck/deck.local, git-ignored, or deck@192.168.1.82)
#   DEST=<dir on Deck>  default ~/fifastreet3
# Make the package first: powershell -File tools/make_deck_package.ps1
# --with-game also copies game/ (2.2 GB, needed once; the Deck keeps its own copy).
set -eu
cd "$(dirname "$0")/../.."
[ -f tools/deck/deck.local ] && . tools/deck/deck.local
DECK="${DECK:-deck@192.168.1.82}"
DEST="${DEST:-fifastreet3}"
PKG=dist/fifastreet3-deck
[ -f "$PKG/fifastreet3.exe" ] || { echo "no package in $PKG"; exit 1; }
ssh -o ConnectTimeout=8 "$DECK" "mkdir -p '$DEST'"
echo "copying package to $DECK:$DEST"
scp -q -r "$PKG"/. "$DECK:$DEST/"
ssh "$DECK" "chmod +x '$DEST'/tools/deck/*.sh"
if [ "${1:-}" = "--with-game" ]; then
  echo "copying game/ (2.2 GB) to $DECK:$DEST/game"
  scp -q -r game "$DECK:$DEST/"
fi
ssh "$DECK" "cd '$DEST' && ls -1 | head -20 && du -sh . 2>/dev/null"
