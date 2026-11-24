#!/bin/bash
# On the Deck: after a delay, run winsampler.exe inside the game's Proton prefix to sample the busiest
# threads of fifastreet3.exe. Usage: tools/deck/winsample.sh [delay_s] [seconds] [threads] [out]
# delay_s may also be "phase:<name>": wait until the newest game log shows "phase: <name>", then 3 s.
set -u
cd "$(dirname "$0")/../.." || exit 1
case "${1:-90}" in
  phase:*)
    PH="${1#phase:}"; START=$(date +%s)
    while :; do
      L=$(ls -1t logs/*.log 2>/dev/null | head -1)
      [ -n "$L" ] && [ "$(stat -c %Y "$L")" -ge "$START" ] && grep -q "phase: $PH" "$L" && break
      sleep 1
    done
    sleep 3 ;;
  *) sleep "${1:-90}" ;;
esac
STEAM_ROOT="${STEAM_ROOT:-$HOME/.local/share/Steam}"
PROTON="${PROTON:-$STEAM_ROOT/steamapps/common/Proton - Experimental/proton}"
export STEAM_COMPAT_CLIENT_INSTALL_PATH="$STEAM_ROOT"
export STEAM_COMPAT_DATA_PATH="${STEAM_COMPAT_DATA_PATH:-$HOME/.local/share/fifastreet3-proton}"
OUT="${4:-runs/winsample.txt}"
mkdir -p runs
winpath() { echo "Z:$(echo "$1" | tr '/' '\\')"; }
WOUT=$(winpath "$PWD/$OUT")
"$PROTON" run "$PWD/tools/deck/winsampler.exe" fifastreet3.exe "${2:-8}" "${3:-5}" "$WOUT" > runs/winsample.proton.txt 2>&1
echo "winsampler exit $? -> $OUT ($(wc -l < "$OUT" 2>/dev/null) lines)"
