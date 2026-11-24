// fifastreet3 - game-specific hooks on recompiled functions.
//
// Rule for this file (Stage A, 1:1): a hook may change how the host spends time, never what the game
// computes. Each hook says what it replaces, why, and how it was measured.

#include <atomic>
#include <chrono>
#include <cstdint>
#include <map>
#include <mutex>
#include <string>

#include <fmt/format.h>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/thread.h>

// Generated declarations and the REX_LOAD/STORE and indirect-call macros used by hooks that
// reproduce recompiled code.
#include "generated/default/fifastreet3_init.h"

#if defined(_WIN32)
#include <windows.h>
#endif

#if defined(__linux__)
#include <sys/prctl.h>
#include <cstdlib>
namespace {
// Profiling aid for the native Linux build: with FS3_ALLOW_PTRACE=1 the process lets any process of
// the same user attach (SteamOS has kernel.yama.ptrace_scope = 1, which otherwise allows only an
// ancestor), so eu-stack / gdb can sample a running game. Off unless the variable is set.
struct AllowPtraceIfRequested {
  AllowPtraceIfRequested() {
    const char* value = std::getenv("FS3_ALLOW_PTRACE");
    if (value && value[0] == '1') {
      prctl(PR_SET_PTRACER, PR_SET_PTRACER_ANY, 0, 0, 0);
    }
  }
} allow_ptrace_if_requested;
}  // namespace
#endif

REXCVAR_DEFINE_INT32(fs3_worker_idle_spin_us, 50, "FIFA Street 3",
                     "Job worker threads: keep polling an empty job list this long before sleeping");
REXCVAR_DEFINE_INT32(fs3_worker_idle_sleep_us, 100, "FIFA Street 3",
                     "Job worker threads: sleep between polls of an empty job list after the spin "
                     "(0 = always spin, the unhooked behaviour)");
REXCVAR_DEFINE_INT32(fs3_yield_spin_us, 100, "FIFA Street 3",
                     "Game spin-wait helper (SwitchToThread path): yield this long before sleeping");
REXCVAR_DEFINE_INT32(fs3_yield_sleep_us, 100, "FIFA Street 3",
                     "Game spin-wait helper: sleep before each further yield once a wait lasted "
                     "fs3_yield_spin_us (0 = always just yield, the unhooked behaviour)");
REXCVAR_DEFINE_BOOL(fs3_log_yield_sites, false, "FIFA Street 3",
                    "Every 2 s, log how often each call site used the game's spin-wait helper");
REXCVAR_DEFINE_INT32(fs3_fence_wait_spin_us, 200, "FIFA Street 3",
                     "D3D fence wait: spin this long before sleeping between fence checks");
REXCVAR_DEFINE_INT32(fs3_fence_wait_sleep_us, 250, "FIFA Street 3",
                     "D3D fence wait: sleep between fence checks after the spin (0 = always spin, "
                     "the unhooked behaviour)");

namespace {

// Sub-millisecond sleep. Win32 Sleep() has 1 ms granularity (and the SDK's Sleep turns < 1 ms into
// Sleep(0), a yield that keeps the core busy); NtDelayExecution takes 100 ns units and Wine/Proton
// honours fine values.
void SleepMicroseconds(int64_t us) {
#if defined(_WIN32)
  using NtDelayExecutionFn = LONG(NTAPI*)(BOOLEAN, PLARGE_INTEGER);
  static const auto nt_delay = reinterpret_cast<NtDelayExecutionFn>(
      GetProcAddress(GetModuleHandleW(L"ntdll.dll"), "NtDelayExecution"));
  if (nt_delay) {
    LARGE_INTEGER interval;
    interval.QuadPart = -us * 10;  // relative, 100 ns units
    nt_delay(FALSE, &interval);
    return;
  }
#endif
  rex::thread::Sleep(std::chrono::microseconds(us));
}

}  // namespace

// Each hooked function is a weak alias in the generated code; the originals stay callable as
// __imp__<name> (declared by the generated headers included above).

// sub_82505678: one step of the D3D runtime's BlockOnFence loop (called from sub_824FEBC0 until the
// GPU-written fence at [device+0x2A90] passes the target). It burns time with 4 x 8 `or r31,r31,r31`
// (db8cyc) hints - on Xenon those hand the core to the other hardware thread - then re-reads the fence
// and runs a 5000-tick hang check. Recompiled, the hints do nothing and the wait spins a host core at
// 100% (PC sampling: 89% of a thread in this function and its callees; Steam Deck: the "STREET" guest
// thread at 92% while the 15 W budget held the GPU at ~1040 MHz).
//
// The hook keeps the step exactly as the game wrote it (the original runs every time, so the fence
// compare, hang check and return value are untouched) and only adds a host sleep between steps once a
// wait has lasted fs3_fence_wait_spin_us: short waits keep their latency, long ones stop burning power.
REX_HOOK_RAW(sub_82505678) {
  thread_local std::chrono::steady_clock::time_point wait_start;
  thread_local std::chrono::steady_clock::time_point last_step;
  const auto now = std::chrono::steady_clock::now();
  // A gap of more than 2 ms since the previous step means this is a new wait.
  if (now - last_step > std::chrono::milliseconds(2)) {
    wait_start = now;
  }
  last_step = now;
  const int32_t sleep_us = REXCVAR_GET(fs3_fence_wait_sleep_us);
  if (sleep_us > 0 &&
      now - wait_start > std::chrono::microseconds(REXCVAR_GET(fs3_fence_wait_spin_us))) {
    SleepMicroseconds(sleep_us);
    last_step = std::chrono::steady_clock::now();
  }
  __imp__sub_82505678(ctx, base);
}

// sub_824765D0: the game's "pause briefly" helper, used by its spin-waits (dozens of call sites that
// loop on a flag): with a zero delay value it tail-calls SwitchToThread (sub_8251CDE0 ->
// NtYieldExecution), otherwise SleepEx(delay, TRUE). The main game thread spends its idle time in such
// a loop: ~500,000 yields a second, paused or playing (debug_log_guest_yields). On Xenon a yield hands
// the hardware thread over; on the host it is a sched_yield that returns at once with a free core, so
// the loop burns a core at full speed - on the Steam Deck, power the GPU lacks under the 15 W cap.
//
// The original still runs on every call (same yield, same return value); a long run of zero-delay
// calls on one thread (a gap under 2 ms counts as the same wait) just gets a host sleep before each
// further yield once it has lasted fs3_yield_spin_us.
REX_HOOK_RAW(sub_824765D0) {
  if (REX_LOAD_U32(ctx.r3.u32) == 0) {
    thread_local std::chrono::steady_clock::time_point wait_start;
    thread_local std::chrono::steady_clock::time_point last_call;
    const auto now = std::chrono::steady_clock::now();
    if (now - last_call > std::chrono::milliseconds(2)) {
      wait_start = now;
    }
    last_call = now;
    const int32_t sleep_us = REXCVAR_GET(fs3_yield_sleep_us);
    if (sleep_us > 0 &&
        now - wait_start > std::chrono::microseconds(REXCVAR_GET(fs3_yield_spin_us))) {
      SleepMicroseconds(sleep_us);
      last_call = std::chrono::steady_clock::now();
    }
  }
  if (REXCVAR_GET(fs3_log_yield_sites)) {
    static std::mutex mutex;
    static std::map<uint32_t, uint64_t> counts;
    static auto last = std::chrono::steady_clock::now();
    std::lock_guard<std::mutex> lock(mutex);
    counts[ctx.lr]++;
    const auto now = std::chrono::steady_clock::now();
    if (now - last >= std::chrono::seconds(2)) {
      std::string line;
      for (const auto& [lr, n] : counts) {
        line += fmt::format(" {:08X}:{}", lr, n);
      }
      REXLOG_INFO("[fs3 yield sites / 2 s]{}", line);
      counts.clear();
      last = now;
    }
  }
  __imp__sub_824765D0(ctx, base);
}

// sub_821A2A10: main loop of the game's job worker threads (two of them). It pops a job off a lock-free
// list with InterlockedPopEntrySList and, when the list is empty, retries at once with no pause, so each
// worker burns a host core whenever there's no work (PC sampling: ~20% of a core each, 81% of it inside
// InterlockedPopEntrySList; Steam Deck: ~25-30% each, power the GPU then lacks under the 15 W cap).
//
// This is the recompiled loop verbatim (same pop, same loads of the job's function and five arguments,
// same indirect call, same clear of the job's busy byte); the only addition is a backoff while the list
// stays empty: poll for fs3_worker_idle_spin_us, then sleep fs3_worker_idle_sleep_us between polls.
// The thread never returns, like the original.
REX_HOOK_RAW(sub_821A2A10) {
  REX_FUNC_PROLOGUE();
  uint32_t ea{};
  ctx.r12.u64 = ctx.lr;
  REX_STORE_U32(ctx.r1.u32 + -8, ctx.r12.u32);
  REX_STORE_U64(ctx.r1.u32 + -24, ctx.r30.u64);
  REX_STORE_U64(ctx.r1.u32 + -16, ctx.r31.u64);
  ea = -112 + ctx.r1.u32;
  REX_STORE_U32(ea, ctx.r1.u32);
  ctx.r1.u32 = ea;
  ctx.r30.u64 = ctx.r3.u64;
  std::chrono::steady_clock::time_point empty_since{};
  bool idle = false;
  for (;;) {
    ctx.r3.u64 = ctx.r30.u64;
    ctx.lr = 0x821A2A30;
    __imp__InterlockedPopEntrySList(ctx, base);
    ctx.r31.u64 = ctx.r3.u64;
    if (ctx.r31.u32 == 0) {
      const auto now = std::chrono::steady_clock::now();
      if (!idle) {
        idle = true;
        empty_since = now;
      }
      const int32_t sleep_us = REXCVAR_GET(fs3_worker_idle_sleep_us);
      if (sleep_us > 0 &&
          now - empty_since > std::chrono::microseconds(REXCVAR_GET(fs3_worker_idle_spin_us))) {
        SleepMicroseconds(sleep_us);
      }
      continue;
    }
    idle = false;
    std::atomic_thread_fence(std::memory_order_seq_cst);
    ctx.r7.u64 = REX_LOAD_U32(ctx.r31.u32 + 20);
    ctx.r6.u64 = REX_LOAD_U32(ctx.r31.u32 + 24);
    ctx.r5.u64 = REX_LOAD_U32(ctx.r31.u32 + 36);
    ctx.r4.u64 = REX_LOAD_U32(ctx.r31.u32 + 32);
    ctx.r3.u64 = REX_LOAD_U32(ctx.r31.u32 + 28);
    ctx.r11.u64 = REX_LOAD_U32(ctx.r31.u32 + 40);
    ctx.ctr.u64 = ctx.r11.u64;
    ctx.lr = 0x821A2A5C;
    REX_CALL_INDIRECT_FUNC(ctx.ctr.u32);
    ctx.r11.s64 = 0;
    REX_STORE_U8(ctx.r31.u32 + 4, ctx.r11.u8);
  }
}
