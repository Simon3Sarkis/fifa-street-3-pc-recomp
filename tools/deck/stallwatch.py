"""On the Deck: watch one thread of the running native game and report every stretch it spends off the
CPU for longer than a threshold - which system call it was blocked in (with its arguments: a futex
address identifies the lock) and the kernel wait channel. Needs the game started with
FS3_ALLOW_PTRACE=1 (/proc/<pid>/task/<tid>/syscall is ptrace-guarded on SteamOS).

    python3 stallwatch.py <out.txt> [thread-name-prefix] [min_ms] [phase] [package_dir]

Polls every ~1 ms from the moment the newest game log shows "phase: <phase>" until the game exits.
Also writes the game's memory map next to the output, to resolve futex addresses to libraries.
"""
import glob
import os
import sys
import time

out_path = sys.argv[1]
prefix = sys.argv[2] if len(sys.argv) > 2 else 'GPU Commands'
min_ms = float(sys.argv[3]) if len(sys.argv) > 3 else 8.0
phase = sys.argv[4] if len(sys.argv) > 4 else 'match'
pkg = sys.argv[5] if len(sys.argv) > 5 else os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

start = time.time()
pid = None
while time.time() - start < 600:
    logs = sorted(glob.glob(os.path.join(pkg, 'logs', '*.log')), key=os.path.getmtime)
    if logs and os.path.getmtime(logs[-1]) >= start:
        try:
            if ('phase: ' + phase) in open(logs[-1], errors='replace').read():
                break
        except OSError:
            pass
    time.sleep(0.5)
for p in os.listdir('/proc'):
    if p.isdigit():
        try:
            if open(f'/proc/{p}/comm').read().strip() == 'fifastreet3':
                pid = p
                break
        except OSError:
            pass
if not pid:
    sys.exit('game not running')
tid = None
for t in os.listdir(f'/proc/{pid}/task'):
    try:
        if open(f'/proc/{pid}/task/{t}/comm').read().startswith(prefix):
            tid = t
            break
    except OSError:
        pass
if not tid:
    sys.exit(f'no thread {prefix!r}')
try:
    open(out_path + '.maps', 'w').write(open(f'/proc/{pid}/maps').read())
except OSError as e:
    print('maps:', e)

base = f'/proc/{pid}/task/{tid}'
events = []
off_since = None
info = None
while os.path.exists(base):
    try:
        stat = open(base + '/stat').read()
        state = stat[stat.rindex(')') + 2]
    except (OSError, ValueError):
        break
    now = time.monotonic()
    if state in 'SD':
        if off_since is None:
            off_since = now
            info = None
        if info is None or now - off_since > 0.003:
            try:
                info = (open(base + '/syscall').read().strip(), open(base + '/wchan').read().strip())
            except OSError:
                pass
    else:
        if off_since is not None:
            ms = (now - off_since) * 1000
            if ms >= min_ms:
                events.append((time.strftime('%H:%M:%S'), round(ms, 1), info))
            off_since = None
    time.sleep(0.0008)

with open(out_path, 'w') as f:
    f.write(f'thread {prefix} tid {tid}: {len(events)} off-CPU stretches >= {min_ms} ms\n')
    for t, ms, inf in events:
        f.write(f'{t} {ms} ms  syscall: {inf[0] if inf else "?"}  wchan: {inf[1] if inf else "?"}\n')
print(f'stallwatch: {len(events)} stretches -> {out_path}')
