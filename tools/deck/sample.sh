#!/bin/bash
# On the Deck: sample the running fifastreet3.exe after a delay - busiest threads, GPU clock and busy,
# APU power. Usage: tools/deck/sample.sh [delay_s] [samples]
DELAY="${1:-85}"; N="${2:-3}"
sleep "$DELAY"
P=$(ps -eo pid,comm,rss --sort=-rss | awk '$2 ~ /fifastreet3/ {print $1; exit}')
[ -n "$P" ] || { echo "game not running"; exit 1; }
D=$(for d in /sys/class/drm/card*/device; do [ -f "$d/pp_dpm_sclk" ] && echo "$d" && break; done)
H=$(ls -d "$D"/hwmon/hwmon* | head -1)
echo "pid $P"
top -H -b -d 5 -n 2 -p "$P" | awk '/^top -/{n++} n==2' | sed -n '7,14p'
for i in $(seq 1 "$N"); do
  echo "gpu $(grep '\*' "$D/pp_dpm_sclk" | awk '{print $2}') busy $(cat "$D/gpu_busy_percent")% apu $(( $(cat "$H/power1_average") / 1000000 )) W cpu $(awk '/MHz/{s+=$4;n++} END{printf "%.0f", s/n}' /proc/cpuinfo) MHz"
  sleep 2
done
