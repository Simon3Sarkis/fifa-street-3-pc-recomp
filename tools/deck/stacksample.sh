#!/bin/bash
# On the Deck: sample all thread stacks of the running native game with eu-stack (needs the game
# started with FS3_ALLOW_PTRACE=1). Usage: tools/deck/stacksample.sh <out> [phase] [samples] [interval_s]
#   phase: wait until the newest game log shows "phase: <phase>" (default pause), then 3 s.
# Each sample briefly stops the process (tens of ms). Thread names go to <out>.threads.
OUT="$1"; PH="${2:-pause}"; N="${3:-25}"; IV="${4:-0.4}"
cd "$(dirname "$0")/../.." || exit 1
START=$(date +%s)
for i in $(seq 1 300); do
  L=$(ls -1t logs/*.log 2>/dev/null | head -1)
  [ -n "$L" ] && [ "$(stat -c %Y "$L")" -ge "$START" ] && grep -q "phase: $PH" "$L" && break
  sleep 1
done
sleep 3
P=$(pgrep -x fifastreet3 | head -1)
[ -n "$P" ] || { echo "game not running"; exit 1; }
for t in /proc/$P/task/*; do echo "${t##*/} $(cat "$t/comm")"; done > "$OUT.threads"
: > "$OUT"
for i in $(seq 1 "$N"); do
  echo "=== sample $i" >> "$OUT"
  eu-stack -p "$P" -n 12 >> "$OUT" 2>&1
  sleep "$IV"
done
echo "stacksample: $N samples -> $OUT"
