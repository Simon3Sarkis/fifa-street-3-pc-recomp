"""Classify slow frames from debug_log_frame_timing logs: frame_timing.py <run.log> [...]

Joins '[frame cpu]' (swap interval, command-processor waits) and '[frame gpu]' (GPU span) lines by
frame number. A frame counts as slow when its swap interval is over 25 ms (a missed vblank). For each
slow frame it reports the GPU span of the frame, the command processor's waits on the game
(wait_reg_mem + no_commands) and its own busy time (interval - waits), and classifies it:
  gpu   - the GPU span itself exceeded 16.7 ms
  game  - the command processor spent most of the frame waiting for the game (CPU-side guest work)
  cp    - the command processor was busy (translating / submitting) for over 16.7 ms
Also prints the GPU span distribution of all frames.
"""
import re
import statistics
import sys

for path in sys.argv[1:]:
    cpu, gpu = {}, {}
    for line in open(path, encoding='utf-8', errors='replace'):
        m = re.search(r'\[frame cpu\] (\d+) interval_us=(\d+) wait_reg_mem_us=(\d+) no_commands_us=(\d+) '
                      r'gpu_fence_us=(\d+) swap_us=(\d+) submit_us=(\d+)(?: thread_cpu_us=(\d+))?', line)
        if m:
            cpu[int(m[1])] = [int(v) if v is not None else -1 for v in m.groups()[1:]]
            continue
        m = re.search(r'\[frame gpu\] (\d+) span_us=(\d+)', line)
        if m:
            gpu[int(m[1])] = int(m[2])
    frames = sorted(cpu)
    # The GPU record may be numbered one off from the CPU record: pick the offset that best matches
    # (slow intervals should line up with long spans more often).
    best = 0
    spans = sorted(gpu.values())
    print(f'{path}: {len(frames)} frames; GPU span p50 {statistics.median(spans)/1000:.1f} ms, '
          f'p90 {spans[int(len(spans)*0.9)]/1000:.1f}, p99 {spans[int(len(spans)*0.99)]/1000:.1f}, '
          f'max {spans[-1]/1000:.1f}; frames with span > 16.7 ms: {sum(s > 16700 for s in spans)}')
    slow = [f for f in frames if cpu[f][0] > 25000]
    print(f'  slow frames (interval > 25 ms): {len(slow)}')
    kinds = {'gpu': 0, 'game': 0, 'cp': 0, '?': 0}
    rows = []
    for f in slow:
        interval, regmem, nocmd, fence, swap, submit, tcpu = cpu[f]
        waits = regmem + nocmd + fence
        busy = interval - waits
        spans_near = [gpu.get(f + d) for d in (-1, 0, 1)]
        g = max(s for s in spans_near if s is not None) if any(spans_near) else 0
        if g > 16700:
            k = 'gpu'
        elif busy > 16700:
            k = 'cp'
        elif waits > interval / 2:
            k = 'game'
        else:
            k = '?'
        kinds[k] += 1
        rows.append((f, interval, g, regmem, nocmd, busy, submit, k, tcpu))
    print('  classified: ' + ', '.join(f'{k} {v}' for k, v in kinds.items()))
    print('  frame  interval  gpu_span(max of f-1..f+1)  wait_reg_mem  no_cmds  cp_busy  submit  kind  cp_thread_cpu')
    for r in rows[:14]:
        print('  {:6d} {:7.1f} {:10.1f} {:16.1f} {:12.1f} {:8.1f} {:7.1f}  {:5s} {:7.1f}'.format(
            r[0], r[1] / 1000, r[2] / 1000, r[3] / 1000, r[4] / 1000, r[5] / 1000, r[6] / 1000, r[7],
            r[8] / 1000))
    normal = [f for f in frames if cpu[f][0] <= 25000]
    if normal:
        b = sorted(cpu[f][0] - cpu[f][1] - cpu[f][2] - cpu[f][3] for f in normal)
        print(f'  normal frames: command-processor busy p50 {b[len(b)//2]/1000:.1f} ms, '
              f'p99 {b[int(len(b)*0.99)]/1000:.1f} ms')
        c = sorted(cpu[f][6] for f in normal if cpu[f][6] >= 0)
        if c:
            print(f'  normal frames: command-processor thread CPU p50 {c[len(c)//2]/1000:.1f} ms, '
                  f'p90 {c[int(len(c)*0.9)]/1000:.1f} ms')
