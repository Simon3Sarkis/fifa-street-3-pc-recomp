#!/bin/bash
# On the Deck: give the native game's GPU command thread a core of its own. Waits until the newest
# game log shows "phase: <phase>" (default match), then pins the thread named "GPU Commands" to CPU
# <cp_cpu> and every other thread of the game to the CPUs outside that core (CPUs 2k and 2k+1 are SMT
# siblings of core k). Only changes the affinity of the game's own threads.
# Usage: tools/deck/pin_threads.sh [phase] [cp_cpu]   (started by run_native.sh with PIN_CP=1)
PH="${1:-match}"; CP_CPU="${2:-0}"
cd "$(dirname "$0")/../.." || exit 1
START=$(date +%s)
for i in $(seq 1 300); do
  L=$(ls -1t logs/*.log 2>/dev/null | head -1)
  [ -n "$L" ] && [ "$(stat -c %Y "$L")" -ge "$START" ] && grep -q "phase: $PH" "$L" && break
  sleep 1
done
P=$(pgrep -x fifastreet3 | head -1)
[ -n "$P" ] || exit 0
SIBLING=$(( CP_CPU ^ 1 ))
NCPU=$(nproc)
OTHERS=$(seq 0 $(( NCPU - 1 )) | grep -vx "$CP_CPU" | grep -vx "$SIBLING" | paste -sd,)
n=0
for t in /proc/$P/task/*; do
  tid=${t##*/}
  if [[ "$(cat "$t/comm" 2>/dev/null)" == "GPU Commands"* ]]; then
    taskset -p -c "$CP_CPU" "$tid" > /dev/null 2>&1 && echo "pinned GPU Commands ($tid) to CPU $CP_CPU"
  else
    taskset -p -c "$OTHERS" "$tid" > /dev/null 2>&1 && n=$((n + 1))
  fi
done
echo "pinned $n other threads to CPUs $OTHERS"
