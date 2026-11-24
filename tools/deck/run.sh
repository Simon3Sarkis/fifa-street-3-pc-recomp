#!/bin/bash
# Run fifastreet3.exe on the Steam Deck under Proton, optionally with a test input script.
# Usage (on the Deck, from the package folder):
#   tools/deck/run.sh [name] [input_script.txt] [extra --flags...]
# Without an input script it just starts the game. With one, saves/caches go to userdata-<name>/ (fresh
# per run unless SAVE=<folder> is given), in-game snaps to snaps/<name>/, and the log is copied to
# runs/<name>.log. Needs a display: run from Desktop Mode (DISPLAY=:0) or via a Steam shortcut.
# Proton: PROTON=<path to the proton script>, default Proton - Experimental.
set -u
cd "$(dirname "$0")/../.." || exit 1
PKG="$PWD"
NAME="${1:-play}"
SCRIPT="${2:-}"
shift $(( $# > 2 ? 2 : $# ))

STEAM_ROOT="${STEAM_ROOT:-$HOME/.local/share/Steam}"
PROTON="${PROTON:-$STEAM_ROOT/steamapps/common/Proton - Experimental/proton}"
[ -x "$PROTON" ] || { echo "Proton not found at $PROTON (set PROTON=...)"; exit 1; }
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$STEAM_ROOT"
export STEAM_COMPAT_DATA_PATH="${STEAM_COMPAT_DATA_PATH:-$HOME/.local/share/fifastreet3-proton}"
mkdir -p "$STEAM_COMPAT_DATA_PATH" runs snaps
export DISPLAY="${DISPLAY:-:0}"

winpath() { echo "Z:$(echo "$1" | sed 's|/|\\|g')"; }

ARGS=()
if [ -n "$SCRIPT" ]; then
  UD="$PKG/userdata-$NAME"
  [ -e "$UD" ] && { echo "$UD exists; pick another name"; exit 1; }
  mkdir -p "$UD" "snaps/$NAME"
  [ -n "${SAVE:-}" ] && cp -r "$SAVE"/. "$UD"/
  ARGS+=("--input_script=$(winpath "$PKG/$SCRIPT")" "--user_data_root=$(winpath "$UD")"
         "--cache_root=$(winpath "$PKG/cache")" "--input_script_snap_dir=$(winpath "$PKG/snaps/$NAME")")
fi
BEFORE=$(ls -1 logs 2>/dev/null)
echo "run $NAME: $(date +%T)"
rm -f "runs/$NAME.mon.txt"
# NO_MONITOR=1: skip the thread monitor (its /proc reads every few s can stall the game).
[ -z "${NO_MONITOR:-}" ] && bash tools/deck/monitor.sh "runs/$NAME.mon.txt" 4 &
"$PROTON" run "$PKG/fifastreet3.exe" "${ARGS[@]}" "$@" > "runs/$NAME.proton.txt" 2>&1
echo "exit $? at $(date +%T)"
LOG=$(ls -1t logs/*.log 2>/dev/null | head -1)
[ -n "$LOG" ] && cp "$LOG" "runs/$NAME.log" && echo "log: runs/$NAME.log"
grep -E "input_script:.*(phase|quit)|critical|FATAL|frame stats" "runs/$NAME.log" 2>/dev/null | tail -5
