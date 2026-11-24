#!/bin/bash
# Run the native Linux build of fifastreet3 on the Steam Deck (no Proton), optionally with a test
# input script. Same interface as run.sh:
#   tools/deck/run_native.sh [name] [input_script.txt] [extra --flags...]
# Game files: game/ next to the binary, or a link to the Proton package's copy (GAME=<dir>, default
# ~/fifastreet3/game). Saves/caches go to userdata-<name>/ (fresh per run unless SAVE=<folder>), snaps
# to snaps/<name>/, the log to runs/<name>.log. Needs a display (DISPLAY=:0).
# CACHE_ROOT=<dir> uses another shader/pipeline cache (default cache/ in the package).
set -u
cd "$(dirname "$0")/../.." || exit 1
PKG="$PWD"
NAME="${1:-play}"
SCRIPT="${2:-}"
shift $(( $# > 2 ? 2 : $# ))
[ -e game ] || ln -s "${GAME:-$HOME/fifastreet3/game}" game
mkdir -p runs snaps
export DISPLAY="${DISPLAY:-:0}"
export LD_LIBRARY_PATH="$PKG${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

ARGS=()
if [ -n "$SCRIPT" ]; then
  UD="$PKG/userdata-$NAME"
  [ -e "$UD" ] && { echo "$UD exists; pick another name"; exit 1; }
  mkdir -p "$UD" "snaps/$NAME"
  [ -n "${SAVE:-}" ] && cp -r "$SAVE"/. "$UD"/
  ARGS+=("--input_script=$PKG/$SCRIPT" "--user_data_root=$UD" "--cache_root=${CACHE_ROOT:-$PKG/cache}"
         "--input_script_snap_dir=$PKG/snaps/$NAME")
fi
echo "run $NAME (native): $(date +%T)"
rm -f "runs/$NAME.mon.txt"
# NO_MONITOR=1: skip the thread monitor (its /proc reads every few s can stall the game).
[ -z "${NO_MONITOR:-}" ] && bash tools/deck/monitor.sh "runs/$NAME.mon.txt" 4 &
# PIN_CP=1: give the GPU command thread a core of its own once the match starts.
[ -n "${PIN_CP:-}" ] && bash tools/deck/pin_threads.sh match > "runs/$NAME.pin.txt" 2>&1 &
START=$(date +%s)
"$PKG/fifastreet3" "${ARGS[@]}" "$@" > "runs/$NAME.stdout.txt" 2>&1
echo "exit $? at $(date +%T)"
LOG=$(find "$PKG" "$PKG/userdata-$NAME" -maxdepth 2 -name "*.log" -newermt "@$START" 2>/dev/null | grep -v "/runs/" | head -1)
[ -n "$LOG" ] && cp "$LOG" "runs/$NAME.log" && echo "log: runs/$NAME.log ($LOG)"
grep -E "input_script:.*(phase|quit)|critical|FATAL|frame stats" "runs/$NAME.log" 2>/dev/null | tail -5
