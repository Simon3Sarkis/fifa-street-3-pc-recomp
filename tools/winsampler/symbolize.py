"""Symbolize winsampler.exe output: symbolize.py <samples.txt> [top] [local_dir]

Modules whose Deck path ends in a file present in local_dir (default dist/fifastreet3-deck) are
symbolized with llvm-symbolizer against those local copies (same build, so the PDBs match); other
modules (Wine builtins, drivers) are reported by name only.
"""
import collections
import os
import subprocess
import sys

LLVM = r'C:\Program Files\LLVM\bin\llvm-symbolizer.exe'
path = sys.argv[1]
top = int(sys.argv[2]) if len(sys.argv) > 2 else 20
local_dir = sys.argv[3] if len(sys.argv) > 3 else 'dist/fifastreet3-deck'

modules, threads, samples = [], {}, collections.defaultdict(collections.Counter)
for line in open(path, encoding='utf-16' if open(path, 'rb').read(2) in (b'\xff\xfe', b'\xfe\xff') else 'utf-8', errors='replace'):
    parts = line.split(None, 3)
    if not parts:
        continue
    if parts[0] == 'module':
        modules.append((int(parts[1], 16), int(parts[2], 16), parts[3].strip()))
    elif parts[0] == 'thread':
        threads[int(parts[1])] = int(parts[2])
    elif parts[0] == 'sample':
        samples[int(parts[1])][int(parts[2], 16)] += int(parts[3])
modules.sort()


def module_of(rip):
    for base, size, p in modules:
        if base <= rip < base + size:
            return base, p
    return None, '?'


for tid, counter in sorted(samples.items(), key=lambda kv: -threads.get(kv[0], 0)):
    total = sum(counter.values())
    by_mod = collections.defaultdict(list)
    for rip, n in counter.items():
        base, p = module_of(rip)
        by_mod[(base, p)].append((rip - base if base else rip, n))
    funcs = collections.Counter()
    for (base, p), items in by_mod.items():
        name = p.replace('\\', '/').split('/')[-1]
        local = os.path.join(local_dir, name)
        if base and os.path.isfile(local) and os.path.isfile(LLVM):
            out = subprocess.run([LLVM, f'--obj={local}', '--functions=short', '--no-inlines',
                                  '--relative-address'] + [f'0x{o:x}' for o, _ in items],
                                 capture_output=True, text=True).stdout
            blocks = [b for b in out.split('\n\n') if b.strip()]
            for (o, n), b in zip(items, blocks):
                funcs[f'{name}!{b.strip().splitlines()[0]}'] += n
        else:
            for _, n in items:
                funcs[name] += n
    print(f'\nthread {tid} ({threads.get(tid, "?")}% of a core), {total} samples')
    for f, n in funcs.most_common(top):
        print(f'  {100 * n / total:5.1f}%  {f}')
