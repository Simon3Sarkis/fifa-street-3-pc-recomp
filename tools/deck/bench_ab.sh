#!/bin/bash
# Interleaved A/B benchmark on the Deck (run on the PC from Git Bash): A, B, A, B, ... so slow drift
# between batches (it was 2-3 FPS) hits both sides equally.
# Usage: tools/deck/bench_ab.sh <label> <pairs> "<A flags>" "<B flags>"
#   SCRIPT=tests/input/perf_match.txt PHASE=match (defaults), like bench.sh.
# Prints each run (FPS, 60 held, scene size, mean CPU MHz / GPU MHz / APU W from the monitor), then
# per side the mean and spread, and the mean of the paired differences B - A.
set -u
cd "$(dirname "$0")/../.."
LABEL="$1"; PAIRS="$2"; A_FLAGS="$3"; B_FLAGS="$4"
SCRIPT="${SCRIPT:-tests/input/perf_match.txt}"
PHASE="${PHASE:-match}"
for i in $(seq 1 "$PAIRS"); do
  # shellcheck disable=SC2086
  SAVE=1 bash tools/deck/remote_run.sh "${LABEL}_a$i" "$SCRIPT" --vulkan_gpu_timestamp_buckets=true $A_FLAGS > "work/${LABEL}_a$i.out" 2>&1
  # shellcheck disable=SC2086
  SAVE=1 bash tools/deck/remote_run.sh "${LABEL}_b$i" "$SCRIPT" --vulkan_gpu_timestamp_buckets=true $B_FLAGS > "work/${LABEL}_b$i.out" 2>&1
done
python - "$LABEL" "$PAIRS" "$PHASE" <<'EOF'
import re, statistics, subprocess, sys
label, pairs, phase = sys.argv[1], int(sys.argv[2]), sys.argv[3]
def run_stats(run):
    if subprocess.run([sys.executable, 'tools/check_match.py', run], capture_output=True).returncode:
        return None
    out = subprocess.run([sys.executable, 'tools/frame_stats.py', f'work/runs/{run}.log'], capture_output=True, text=True).stdout
    m = re.search(phase + r'\s+([\d.]+) fps.*60-held\s+(\d+)%', out)
    if not m:
        return None
    log = open(f'work/runs/{run}.log', encoding='utf-8', errors='replace').read()
    start = re.search(r'(\d\d:\d\d:\d\d)\.\d+\].*phase: ' + phase, log)
    end = re.search(r'(\d\d:\d\d:\d\d)\.\d+\].*phase: end', log)
    mon = []
    try:
        for line in open(f'work/runs/{run}.mon.txt'):
            p = line.split()
            if p and p[0] == 't' and start and end and start[1] < p[1][:8] < end[1]:
                mon.append((float(p[3]), float(p[6].rstrip('Mhz') or 0), float(p[10])))
    except OSError:
        pass
    avg = [sum(x[i] for x in mon) / len(mon) for i in range(3)] if mon else [0, 0, 0]
    return float(m[1]), int(m[2]), avg
res = {'a': [], 'b': []}
diffs = []
for i in range(1, pairs + 1):
    row = {}
    for side in 'ab':
        s = run_stats(f'{label}_{side}{i}')
        row[side] = s
        if s:
            res[side].append(s[0])
            print(f'{label}_{side}{i}: {s[0]:.1f} fps, 60 held {s[1]}%, cpu {s[2][0]:.0f} MHz gpu {s[2][1]:.0f} MHz apu {s[2][2]:.1f} W')
        else:
            print(f'{label}_{side}{i}: excluded (not in a match / no stats)')
    if row['a'] and row['b']:
        diffs.append(row['b'][0] - row['a'][0])
for side in 'ab':
    v = res[side]
    if v:
        print(f'{side.upper()}: mean {statistics.mean(v):.2f} fps, spread +-{statistics.stdev(v) if len(v) > 1 else 0:.2f} over {len(v)}')
if diffs:
    print(f'B - A paired: mean {statistics.mean(diffs):+.2f} fps over {len(diffs)} pairs'
          + (f', spread +-{statistics.stdev(diffs):.2f}' if len(diffs) > 1 else ''))
EOF
