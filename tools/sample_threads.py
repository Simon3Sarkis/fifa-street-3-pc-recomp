"""Poor man's sampling profiler for a running fifastreet3.exe (Windows, x64).

    python tools/sample_threads.py [--seconds 8] [--threads 3] [--top 25] [--pid N]

Finds the busiest threads (CPU time over a 1 s window), then samples each one's instruction pointer
~1000 times a second (SuspendThread + GetThreadContext + ResumeThread) and prints the hottest
functions per thread, symbolized with llvm-symbolizer against the exe's PDB (recompiled game code
shows up as sub_XXXXXXXX). Sampling pauses each thread for microseconds; fine for finding hot spots,
not for exact timing.
"""
import argparse
import collections
import ctypes
import ctypes.wintypes as wt
import os
import subprocess
import sys
import time

k32 = ctypes.WinDLL('kernel32', use_last_error=True)
THREAD_ALL = 0x001F03FF
PROCESS_QUERY = 0x0400 | 0x0010
TH32CS_SNAPTHREAD = 0x4
CONTEXT_AMD64 = 0x00100000
CONTEXT_CONTROL = CONTEXT_AMD64 | 0x1


class THREADENTRY32(ctypes.Structure):
    _fields_ = [('dwSize', wt.DWORD), ('cntUsage', wt.DWORD), ('th32ThreadID', wt.DWORD),
                ('th32OwnerProcessID', wt.DWORD), ('tpBasePri', wt.LONG), ('tpDeltaPri', wt.LONG),
                ('dwFlags', wt.DWORD)]


class CONTEXT(ctypes.Structure):
    # x64 CONTEXT; only the fields up to Rip are used. Must be 16-byte aligned.
    _fields_ = [('P1Home', ctypes.c_uint64 * 6), ('ContextFlags', wt.DWORD), ('MxCsr', wt.DWORD),
                ('Seg', wt.WORD * 6), ('EFlags', wt.DWORD), ('Dr', ctypes.c_uint64 * 6),
                ('Regs', ctypes.c_uint64 * 16), ('Rip', ctypes.c_uint64),
                ('Rest', ctypes.c_byte * 1024)]


k32.OpenThread.restype = wt.HANDLE
k32.OpenProcess.restype = wt.HANDLE
k32.CreateToolhelp32Snapshot.restype = wt.HANDLE
for fn in ('SuspendThread', 'ResumeThread', 'GetThreadContext', 'GetThreadTimes', 'CloseHandle'):
    getattr(k32, fn).argtypes = None
k32.SuspendThread.argtypes = [wt.HANDLE]
k32.ResumeThread.argtypes = [wt.HANDLE]
k32.SuspendThread.restype = wt.DWORD
k32.GetThreadContext.argtypes = [wt.HANDLE, ctypes.c_void_p]
k32.GetThreadTimes.argtypes = [wt.HANDLE] + [ctypes.c_void_p] * 4
k32.CloseHandle.argtypes = [wt.HANDLE]
psapi = ctypes.WinDLL('psapi')
psapi.EnumProcessModules.argtypes = [wt.HANDLE, ctypes.c_void_p, wt.DWORD, ctypes.POINTER(wt.DWORD)]
psapi.GetModuleFileNameExW.argtypes = [wt.HANDLE, ctypes.c_void_p, wt.LPWSTR, wt.DWORD]


class MODULEINFO(ctypes.Structure):
    _fields_ = [('lpBaseOfDll', ctypes.c_void_p), ('SizeOfImage', wt.DWORD),
                ('EntryPoint', ctypes.c_void_p)]


psapi.GetModuleInformation.argtypes = [wt.HANDLE, ctypes.c_void_p, ctypes.c_void_p, wt.DWORD]


def find_pid(name):
    out = subprocess.run(['tasklist', '/FI', f'IMAGENAME eq {name}', '/FO', 'CSV', '/NH'],
                         capture_output=True, text=True).stdout
    for line in out.splitlines():
        parts = [p.strip('"') for p in line.split('","')]
        if len(parts) > 1 and parts[0].lower() == name.lower():
            return int(parts[1])
    return None


def threads_of(pid):
    snap = k32.CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0)
    te = THREADENTRY32()
    te.dwSize = ctypes.sizeof(te)
    tids = []
    ok = k32.Thread32First(snap, ctypes.byref(te))
    while ok:
        if te.th32OwnerProcessID == pid:
            tids.append(te.th32ThreadID)
        ok = k32.Thread32Next(snap, ctypes.byref(te))
    k32.CloseHandle(snap)
    return tids


def thread_cpu_ms(h):
    c, e, kt, ut = (wt.FILETIME() for _ in range(4))
    if not k32.GetThreadTimes(h, ctypes.byref(c), ctypes.byref(e), ctypes.byref(kt), ctypes.byref(ut)):
        return None
    f = lambda ft: (ft.dwHighDateTime << 32 | ft.dwLowDateTime) / 10000.0
    return f(kt) + f(ut)


def module_base(pid, name):
    # {load address: (path, image size)} of every module, via EnumProcessModules.
    hp = k32.OpenProcess(PROCESS_QUERY, False, pid)
    mods = (ctypes.c_void_p * 1024)()
    needed = wt.DWORD()
    psapi.EnumProcessModules(hp, mods, ctypes.sizeof(mods), ctypes.byref(needed))
    result = {}
    buf = ctypes.create_unicode_buffer(260)
    info = MODULEINFO()
    for i in range(min(needed.value // ctypes.sizeof(ctypes.c_void_p), len(mods))):
        psapi.GetModuleFileNameExW(hp, mods[i], buf, 260)
        size = info.SizeOfImage if psapi.GetModuleInformation(
            hp, mods[i], ctypes.byref(info), ctypes.sizeof(info)) else 0
        result[mods[i] or 0] = (buf.value, size)
    k32.CloseHandle(hp)
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--seconds', type=float, default=8)
    ap.add_argument('--threads', type=int, default=3)
    ap.add_argument('--top', type=int, default=25)
    ap.add_argument('--pid', type=int)
    args = ap.parse_args()
    pid = args.pid or find_pid('fifastreet3.exe')
    if not pid:
        sys.exit('fifastreet3.exe is not running')

    handles = {}
    for tid in threads_of(pid):
        h = k32.OpenThread(THREAD_ALL, False, tid)
        if h:
            handles[tid] = h
    before = {t: thread_cpu_ms(h) for t, h in handles.items()}
    time.sleep(1.0)
    busy = sorted(((thread_cpu_ms(h) - before[t]) / 10.0, t) for t, h in handles.items()
                  if before[t] is not None)[::-1][:args.threads]
    print('busiest threads (% of a core):', ', '.join(f'{t} {p:.0f}%' for p, t in busy))

    samples = {t: collections.Counter() for _, t in busy}
    ctx = (ctypes.c_byte * (ctypes.sizeof(CONTEXT) + 16))()
    addr = (ctypes.addressof(ctx) + 15) & ~15
    pctx = ctypes.cast(addr, ctypes.POINTER(CONTEXT))
    end = time.perf_counter() + args.seconds
    n = 0
    while time.perf_counter() < end:
        for _, t in busy:
            h = handles[t]
            if k32.SuspendThread(h) == 0xFFFFFFFF:
                continue
            try:  # Ctrl+C here must not leave the game thread suspended.
                pctx.contents.ContextFlags = CONTEXT_CONTROL
                if k32.GetThreadContext(h, pctx):
                    samples[t][pctx.contents.Rip] += 1
            finally:
                k32.ResumeThread(h)
        n += 1
        time.sleep(0.001)
    print(f'{n} sampling rounds over {args.seconds:.0f} s')
    import json
    work = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), 'work')
    os.makedirs(work, exist_ok=True)
    with open(os.path.join(work, 'samples_raw.json'), 'w') as f:
        json.dump({str(t): {str(k): v for k, v in c.items()} for t, c in samples.items()}, f)

    mods = module_base(pid, 'fifastreet3.exe')
    bases = sorted(mods)

    def module_for(rip):
        base = max((b for b in bases if b <= rip), default=None)
        if base is None or rip >= base + mods[base][1]:
            return None, '?'  # not inside any loaded module
        return base, mods[base][0]

    llvm = r'C:\Program Files\LLVM\bin\llvm-symbolizer.exe'
    for pct, t in busy:
        c = samples[t]
        total = sum(c.values()) or 1
        # Group by module + symbolized function.
        by_module = collections.defaultdict(list)
        for rip, k in c.items():
            base, path = module_for(rip)
            by_module[(base, path)].append((rip - base if base is not None else rip, k))
        funcs = collections.Counter()
        for (base, path), items in by_module.items():
            mod = os.path.basename(path)
            if path.lower().endswith(('.exe', '.dll')) and os.path.isfile(llvm) \
                    and ('fifastreet3' in mod.lower() or 'rex' in mod.lower()):
                # Addresses go in on stdin: thousands of them overflow a Windows command line.
                offs = ''.join(f'0x{o:x}\n' for o, _ in items)
                out = subprocess.run([llvm, f'--obj={path}', '--functions=short', '--no-inlines',
                                      '--relative-address'], input=offs, capture_output=True,
                                     text=True).stdout
                names = [l for l in out.split('\n\n') if l.strip()]
                if len(names) != len(items):
                    print(f'warning: {mod}: {len(items)} addresses, {len(names)} symbolized; '
                          f'left unsymbolized', file=sys.stderr)
                    funcs[mod] += sum(k for _, k in items)
                    continue
                for (o, k), block in zip(items, names):
                    fn = block.strip().split('\n')[0]
                    funcs[f'{mod}!{fn}'] += k
            else:
                for o, k in items:
                    funcs[f'{mod}'] += k
        print(f'\nthread {t} ({pct:.0f}% of a core), {total} samples:')
        for fn, k in funcs.most_common(args.top):
            print(f'  {100 * k / total:5.1f}%  {fn}')


if __name__ == '__main__':
    main()
