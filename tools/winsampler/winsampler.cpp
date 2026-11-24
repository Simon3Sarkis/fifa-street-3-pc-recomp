// Minimal instruction-pointer sampler for a running Windows process (works under Wine/Proton).
// Usage: winsampler.exe <process name> <seconds> <threads> <out.txt>
// Picks the <threads> busiest threads over 1 s, samples each one's RIP ~1000 times a second
// (SuspendThread / GetThreadContext / ResumeThread) and writes:
//   module <base hex> <size hex> <path>
//   thread <tid> <cpu % over the pick window>
//   sample <tid> <rip hex> <count>
// Symbolize on the PC with tools/winsampler/symbolize.py.
#include <windows.h>
#include <psapi.h>
#include <tlhelp32.h>

#include <algorithm>
#include <cstdio>
#include <cwchar>
#include <map>
#include <string>
#include <vector>

static DWORD FindPid(const wchar_t* name) {
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
  PROCESSENTRY32W pe{sizeof(pe)};
  DWORD pid = 0;
  for (BOOL ok = Process32FirstW(snap, &pe); ok; ok = Process32NextW(snap, &pe)) {
    if (_wcsicmp(pe.szExeFile, name) == 0) pid = pe.th32ProcessID;
  }
  CloseHandle(snap);
  return pid;
}

static ULONGLONG CpuMs(HANDLE h) {
  FILETIME c, e, k, u;
  if (!GetThreadTimes(h, &c, &e, &k, &u)) return 0;
  auto f = [](FILETIME t) { return (ULONGLONG(t.dwHighDateTime) << 32 | t.dwLowDateTime) / 10000; };
  return f(k) + f(u);
}

int wmain(int argc, wchar_t** argv) {
  if (argc < 5) {
    fwprintf(stderr, L"usage: winsampler <process.exe> <seconds> <threads> <out.txt>\n");
    return 1;
  }
  DWORD pid = FindPid(argv[1]);
  if (!pid) {
    fwprintf(stderr, L"process not found\n");
    return 1;
  }
  const double seconds = _wtof(argv[2]);
  const int nthreads = _wtoi(argv[3]);
  FILE* out = _wfopen(argv[4], L"w");

  HANDLE proc = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
  HMODULE mods[1024];
  DWORD needed = 0;
  EnumProcessModules(proc, mods, sizeof(mods), &needed);
  for (DWORD i = 0; i < needed / sizeof(HMODULE); ++i) {
    wchar_t path[MAX_PATH];
    MODULEINFO mi{};
    GetModuleFileNameExW(proc, mods[i], path, MAX_PATH);
    GetModuleInformation(proc, mods[i], &mi, sizeof(mi));
    fwprintf(out, L"module %llx %lx %ls\n", (unsigned long long)mi.lpBaseOfDll, mi.SizeOfImage, path);
  }

  std::map<DWORD, HANDLE> threads;
  HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
  THREADENTRY32 te{sizeof(te)};
  for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
    if (te.th32OwnerProcessID != pid) continue;
    HANDLE h = OpenThread(THREAD_ALL_ACCESS, FALSE, te.th32ThreadID);
    if (h) threads[te.th32ThreadID] = h;
  }
  CloseHandle(snap);

  std::map<DWORD, ULONGLONG> before;
  for (auto& [tid, h] : threads) before[tid] = CpuMs(h);
  Sleep(1000);
  std::vector<std::pair<ULONGLONG, DWORD>> busy;
  for (auto& [tid, h] : threads) busy.push_back({CpuMs(h) - before[tid], tid});
  std::sort(busy.rbegin(), busy.rend());
  busy.resize(std::min<size_t>(busy.size(), nthreads));
  for (auto& [ms, tid] : busy) fwprintf(out, L"thread %lu %llu\n", tid, ms / 10);

  std::map<DWORD, std::map<DWORD64, unsigned>> samples;
  const ULONGLONG end = GetTickCount64() + ULONGLONG(seconds * 1000);
  alignas(16) CONTEXT ctx;
  while (GetTickCount64() < end) {
    for (auto& [ms, tid] : busy) {
      HANDLE h = threads[tid];
      if (SuspendThread(h) == DWORD(-1)) continue;
      ctx.ContextFlags = CONTEXT_CONTROL;
      if (GetThreadContext(h, &ctx)) samples[tid][ctx.Rip]++;
      ResumeThread(h);
    }
    Sleep(1);
  }
  for (auto& [tid, m] : samples)
    for (auto& [rip, n] : m) fwprintf(out, L"sample %lu %llx %u\n", tid, (unsigned long long)rip, n);
  fclose(out);
  return 0;
}
