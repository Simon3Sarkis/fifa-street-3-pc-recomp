// fifastreet3 - game-specific hooks on recompiled functions.
//
// Rule for this file (Stage A, 1:1): a hook may change how the host spends time, never what the game
// computes. Each hook says what it replaces, why, and how it was measured.

#include <chrono>
#include <cstdint>

#include <rex/cvar.h>
#include <rex/hook.h>
#include <rex/logging.h>
#include <rex/thread.h>

#if defined(_WIN32)
#include <windows.h>
#endif

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

// The recompiled original (sub_82505678 itself is a weak alias that this file overrides).
REX_EXTERN(__imp__sub_82505678);

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
