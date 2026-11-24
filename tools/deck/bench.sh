#!/bin/bash
# Repeat a Deck match benchmark and summarise (run on the PC from Git Bash).
# Usage: tools/deck/bench.sh <label> <repeats> [extra --flags...]
# Runs tests/input/perf_match.txt from the saved profile <repeats> times as <label>_1.., checks each
# reached the match, and prints match FPS per run plus mean and spread. Runs that never reached the
# match are excluded (and reported).
set -u
cd "$(dirname "$0")/../.."
LABEL="$1"; N="$2"; shift 2
SCRIPT="${SCRIPT:-tests/input/perf_match.txt}"
PHASE="${PHASE:-match}"
for i in $(seq 1 "$N"); do
  SAVE=1 bash tools/deck/remote_run.sh "${LABEL}_$i" "$SCRIPT" "$@" > "work/${LABEL}_$i.out" 2>&1
done
python - "$LABEL" "$N" "$PHASE" <<'EOF'
import re, statistics, subprocess, sys
label, n, phase = sys.argv[1], int(sys.argv[2]), sys.argv[3]
vals = []
for i in range(1, n + 1):
    run = f'{label}_{i}'
    ok = subprocess.run([sys.executable, 'tools/check_match.py', run], capture_output=True).returncode == 0
    out = subprocess.run([sys.executable, 'tools/frame_stats.py', f'work/runs/{run}.log'], capture_output=True, text=True).stdout
    m = re.search(phase + r'\s+([\d.]+) fps.*60-held\s+(\d+)%', out)
    if not ok or not m:
        print(f'{run}: not in a match / no stats - excluded'); continue
    # The frozen scene differs per run (camera/AI depend on load timing): report its size so runs
    # can be compared like with like (last GPU profile line, needs --vulkan_gpu_timestamp_buckets).
    log = open(f'work/runs/{run}.log', encoding='utf-8', errors='replace').read()
    d = re.findall(r'Main=\d+\((\d+)\).*?PassEntry=\d+\((\d+)\)', log)
    scene = f', scene {d[-1][0]} draws / {d[-1][1]} passes' if d else ''
    vals.append(float(m[1])); print(f'{run}: {m[1]} fps, 60 held {m[2]}%{scene}')
if vals:
    sd = statistics.stdev(vals) if len(vals) > 1 else 0
    print(f'{label}: mean {statistics.mean(vals):.1f} fps, spread +-{sd:.1f} over {len(vals)} runs')
EOF
