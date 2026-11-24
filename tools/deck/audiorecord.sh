#!/bin/bash
# On the Deck: record what the game plays, from the default output sink's *monitor* (the mixed signal
# going to the speakers / HDMI - never a microphone), once the newest game log shows "phase: <phase>".
# Usage: tools/deck/audiorecord.sh <out.wav> [phase] [seconds]
#   Run from the package folder whose logs/ the game writes (fifastreet3 or fifastreet3-native).
OUT="$1"; PH="${2:-match}"; SECS="${3:-40}"
cd "$(dirname "$0")/../.." || exit 1
START=$(date +%s)
for i in $(seq 1 300); do
  L=$(ls -1t logs/*.log 2>/dev/null | head -1)
  [ -n "$L" ] && [ "$(stat -c %Y "$L")" -ge "$START" ] && grep -q "phase: $PH" "$L" && break
  sleep 1
done
SINK=$(pactl get-default-sink)
# PipeWire's per-node view (quantum, rate, busy, ERR = xruns) alongside, to tell a late audio client
# from a capture glitch.
timeout "$SECS" pw-top -b > "$OUT.pwtop" 2>&1 &
timeout "$SECS" parec -d "$SINK.monitor" --rate=48000 --channels=2 --format=s16le --file-format=wav "$OUT"
wait
echo "audiorecord: $SECS s of $SINK.monitor -> $OUT ($(stat -c %s "$OUT" 2>/dev/null) bytes)"
