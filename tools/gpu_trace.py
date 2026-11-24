"""Summarise per-draw GPU timestamp traces: gpu_trace.py <log> [top]

Needs vulkan_gpu_timestamp_per_draw + vulkan_gpu_timestamp_trace_frames (see tests/input/perf_gputrace.txt).
Per traced frame: total span, time per bucket, and the draws grouped by (bucket, vs, ps) sorted by
GPU time. Per-draw timestamps stop the GPU overlapping draws, so absolute times are inflated: use the
shares, not the milliseconds.
"""
import re
import sys

FRAME = re.compile(r'GPU timestamp trace: frame=(\d+) queries=(\d+)/(\d+) span_us=(\d+)')
DRAW = re.compile(r'\[(\d+)\] (\S+) start=(-?\d+) end=(-?\d+) dur=(-?\d+) vs=([0-9A-F]+) '
                  r'ps=([0-9A-F]+) verts=(\d+)')

path = sys.argv[1]
top = int(sys.argv[2]) if len(sys.argv) > 2 else 15
frames = []
for line in open(path, encoding='utf-8', errors='replace'):
    m = FRAME.search(line)
    if m:
        frames.append({'frame': m[1], 'span': int(m[4]), 'draws': []})
        continue
    m = DRAW.search(line)
    if m and frames:
        frames[-1]['draws'].append((m[2], int(m[5]), m[6], m[7], int(m[8]), int(m[1])))

for f in frames:
    total = sum(max(d[1], 0) for d in f['draws']) or 1
    print(f"frame {f['frame']}: span {f['span'] / 1000:.1f} ms, {len(f['draws'])} intervals, "
          f"sum {total / 1000:.1f} ms")
    groups = {}
    for bucket, dur, vs, ps, verts, idx in f['draws']:
        g = groups.setdefault((bucket, vs, ps), [0, 0, 0, idx])
        g[0] += max(dur, 0)
        g[1] += 1
        g[2] += verts
    for (bucket, vs, ps), (dur, n, verts, first) in sorted(groups.items(), key=lambda x: -x[1][0])[:top]:
        print(f"  {dur / total:5.1%} {dur / 1000:7.2f} ms  {bucket:18} vs {vs} ps {ps}  "
              f"x{n:<4} verts {verts:<7} first #{first}")
