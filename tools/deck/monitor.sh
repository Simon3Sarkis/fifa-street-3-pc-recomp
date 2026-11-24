#!/bin/bash
# On the Deck: while fifastreet3.exe runs, every INTERVAL s log the busiest threads (% of a core over the
# interval, and the CPU each is on - CPUs 2k/2k+1 are SMT siblings of core k), mean CPU MHz, GPU clock
# and APU power. Usage: tools/deck/monitor.sh <out> [interval_s]   (started by run.sh in the background)
OUT="$1"; IV="${2:-4}"
HZ=$(getconf CLK_TCK)
D=$(for d in /sys/class/drm/card*/device; do [ -f "$d/pp_dpm_sclk" ] && echo "$d" && break; done)
H=$(ls -d "$D"/hwmon/hwmon* 2>/dev/null | head -1)
CT=$(for h in /sys/class/hwmon/hwmon*; do [ "$(cat "$h/name" 2>/dev/null)" = k10temp ] && echo "$h/temp1_input" && break; done)
for i in $(seq 1 30); do P=$(pgrep -x 'fifastreet3(\.exe)?' | head -1); [ -n "$P" ] && break; sleep 2; done
[ -n "$P" ] || exit 0
snap() {  # tid ticks cpu name
  for t in /proc/$P/task/*; do
    s=$(cat "$t/stat" 2>/dev/null) || continue
    n=$(cat "$t/comm" 2>/dev/null)
    r=${s##*) }; set -- $r
    echo "${t##*/} $(( ${12} + ${13} )) ${37} ${n// /_}"
  done
}
A=$(snap)
while kill -0 "$P" 2>/dev/null; do
  sleep "$IV"
  B=$(snap)
  {
    echo "t $(date +%T.%3N) cpu $(awk '/MHz/{s+=$4;n++} END{printf "%.0f", s/n}' /proc/cpuinfo) MHz gpu $(grep '\*' "$D/pp_dpm_sclk" 2>/dev/null | awk '{print $2}') busy $(cat "$D/gpu_busy_percent" 2>/dev/null)% apu $(( $(cat "$H/power1_average" 2>/dev/null || echo 0) / 1000000 )) W temp cpu $(( $(cat "$CT" 2>/dev/null || echo 0) / 1000 )) gpu $(( $(cat "$H/temp1_input" 2>/dev/null || echo 0) / 1000 )) C"
    awk -v hz="$HZ" -v iv="$IV" 'NR==FNR{a[$1]=$2; next} ($1 in a){p=100*($2-a[$1])/hz/iv; if (p>=4) printf "  %5.1f%% cpu%s %s\n", p, $3, $4}' <(echo "$A") <(echo "$B") | sort -rn | head -8
  } >> "$OUT"
  A="$B"
done
