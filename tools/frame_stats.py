"""Summarise [frame stats] lines per script phase: frame_stats.py <log> [<log> ...]

Phases come from input-script `log phase: <name>` markers. The first stats line after each marker is
skipped (it straddles the change). Prints mean fps, mean of the avg frame times, worst frame, and the
share of 2-s windows that held 60 FPS (>= 59.5).
"""
import re
import sys

STAT = re.compile(r'\[frame stats\] ([\d.]+) fps, avg ([\d.]+) ms, worst ([\d.]+) ms, >33ms (\d+), '
                  r'frontbuffer (\d+)x(\d+)')
WAITS = re.compile(r'wait_reg_mem ([\d.]+) ms \(([\d.]+)x\), no commands ([\d.]+) ms \(([\d.]+)x\), '
                   r'gpu fence ([\d.]+) ms \(([\d.]+)x\)(?:, swap ([\d.]+) ms, submit ([\d.]+) ms \(([\d.]+)x\))?')
PHASE = re.compile(r'input_script: .*: phase: (\S+)')

for path in sys.argv[1:]:
    phases = {}
    phase, skip = 'boot', True
    size = ''
    for line in open(path, encoding='utf-8', errors='replace'):
        m = PHASE.search(line)
        if m:
            phase, skip = m[1], True
            continue
        m = STAT.search(line)
        if not m:
            continue
        size = f'{m[5]}x{m[6]}'
        if skip:
            skip = False
            continue
        w = WAITS.search(line)
        waits = tuple(float(x or 0) for x in w.groups()) if w else None
        phases.setdefault(phase, []).append((float(m[1]), float(m[2]), float(m[3]), waits))
    print(f'{path}  ({size})')
    for name, rows in phases.items():
        if name in ('boot', 'end') or not rows:
            continue
        fps = sum(r[0] for r in rows) / len(rows)
        avg = sum(r[1] for r in rows) / len(rows)
        worst = max(r[2] for r in rows)
        held = sum(1 for r in rows if r[0] >= 59.5) / len(rows)
        print(f'  {name:10} {fps:5.1f} fps  avg {avg:5.2f} ms  worst {worst:6.1f} ms  '
              f'60-held {held:4.0%}  ({len(rows)} x 2 s)')
        wr = [r[3] for r in rows if r[3]]
        if wr:
            mean = [sum(x[i] for x in wr) / len(wr) for i in range(9)]
            print(f'  {"":10} CP waits per frame: wait_reg_mem {mean[0]:.2f} ms ({mean[1]:.1f}x), '
                  f'no commands {mean[2]:.2f} ms ({mean[3]:.1f}x), gpu fence {mean[4]:.2f} ms ({mean[5]:.1f}x), '
                  f'swap {mean[6]:.2f} ms, submit {mean[7]:.2f} ms ({mean[8]:.1f}x)')
