#!/bin/bash
# Steam shortcut target for the native Linux build (added as a non-Steam game, so gamescope treats it
# as the focused game). Launched from Steam it just starts the game. For test runs driven over SSH:
# write "<name> <script.txt> [extra --flags...]" to steam_next_run in the package folder (and SAVE=1
# on its own first line to start from save_fresh_profile), then launch the shortcut; the file is used
# once and removed, and the run goes through run_native.sh (log, snaps, monitor).
cd "$(dirname "$(readlink -f "$0")")/../.." || exit 1
PKG="$PWD"
if [ -f steam_next_run ]; then
  mapfile -t LINES < steam_next_run
  rm -f steam_next_run
  if [ "${LINES[0]}" = "SAVE=1" ]; then
    export SAVE="$HOME/fifastreet3/save_fresh_profile"
    LINES=("${LINES[@]:1}")
  fi
  read -r -a ARGS <<< "${LINES[0]}"
  exec bash tools/deck/run_native.sh "${ARGS[@]}" > "runs/${ARGS[0]}.out" 2>&1
fi
[ -e game ] || ln -s "$HOME/fifastreet3/game" game
export LD_LIBRARY_PATH="$PKG${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
exec "$PKG/fifastreet3" "$@"
