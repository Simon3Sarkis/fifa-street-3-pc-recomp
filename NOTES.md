# FIFA Street 3 PC port (ReXGlue recompilation): notes

Plan and rules: [PLAN.md](PLAN.md). Stage A (1:1) is in progress.

## The disc

| | |
|---|---|
| Image | `FIFA Street 3/FIFA Street 3.iso` (7.84 GB), extracted to `game/` (71 files, 2.18 GiB) |
| Title ID / media ID | 4541083B / 1B770E2C, disc 1 of 1, NTSC (`fifastreet_ntsc.pe`) |
| Version | 0.0.0.3 (base 0.0.0.3); no title update in use |
| `default.xex` SHA-1 | 3BD3F52E4091CB1FD3A900E5D249FDA6B0682970 |
| XDK | 2.0.6274 (D3D9, XAUD, XMP, XONLINE, XHV, XNET, ...) |
| Image | 0x82000000, size 0x8A0000, entry 0x825194C8; encrypted, LZX-compressed |
| .text | 82110000-825F7408 (4.9 MB, a third of Skate 1's) |
| .pdata | 14,375 entries |
| Data | 27 `.big` (+ `.bh` headers), 9 VP6 movies (`data/movies/*.vp6`) |
| Code libraries | EASTL (241 refs), Lua (`lua` 53), RenderWare (4 strings) |

Decrypted image: `rexgluerd dump-xex game\default.xex work\xexdump` -> `work/xexdump/default_82000000_008A0000.bin`.

## Imports

277 kernel/XAM imports. Xenia Canary flags 27 as unimplemented: Xbox Live networking and UI
(`NetDll_XNet*`, `XamShow*UI`, `XNetLogonGetTitleID`), `RtlUnwind` / `__C_specific_handler` (SEH: codegen
needs `generate_exception_handlers`), STFS device calls, `XeKeysConsoleSignatureVerification`, and the
`Io*Share*` / `IoDismountVolume*` group. Still to check against the SDK's table.

## Codegen

`config/fifastreet3_functions.toml`. Codegen takes ~3 s (Skate 1: ~9 min).

| Pass | Functions | Problems | Fix |
|---|---|---|---|
| 1 | 16,925 | 3 unresolved calls, 1 function that could not seal | see below |
| 2 | 16,928 | 0x8218AD68 still unsealed | switch table |
| 3 | 16,928 | **none, 0 fatal stubs** | |

- setjmp 0x82580FD0 (size 0x2CC, no .pdata) / longjmp 0x82580CB0 (size 0x314): found by their
  f14-f31 save/restore; same layout and sizes as PGR4's CRT routines.
- 0x8229EF98 and 0x823F0080: leaf functions without .pdata, reached by tail branches.
- 0x824DB6F0: `addi r3,r3,4 ; b` this-adjust thunk.
- 0x8218AD68: byte-offset jump table at 0x820D6B08 without its own bounds check. The analyzer guessed 33
  entries and pulled the next function's code in. The callers bound the index to 0-9
  (0x82181D58 `cmplwi r25,9`), so it is pinned as a 10-entry `[[switch_tables]]`.
  **Watch for more of these**: a byte table with no bounds check in the function itself.

## Reference

Xenia Canary (`Xenia Canary/`, build canary_experimental@445ce5406, 2026-11-23) runs the game. Its
settings for the reference: vsync on, resolution scale 1, `readback_resolve = none`,
`clear_memory_page_state = false`, `depth_float24_* = false`.

Missed functions: `tools/scan_data_ptrs.py` + `tools/scan_code_ptrs.py` (code range set to this game's
.text), then `tools/pick_missed.py` writes `config/fifastreet3_scanned.toml` (112 functions: C++ static
initializers from the CRT table at 0x826012CC, vtable and callback targets with no .pdata). Rules: data
pointers marked START with a valid opcode; lis/addi pointers whose first instruction touches no r14-r31;
never inside a .pdata function. A second scan after adding them found nothing new.

## SDK changes (local branch `fifastreet3` in third_party/rexglue-sdk, on top of a667bdb)

1. `298eacc` codegen: the generated PCH includes `rex/platform/exceptions.h`. `SEH_TRY` moved there in
   e4bd1b7 and nothing included it, so any title with SEH funclets failed to compile.
2. `caed883` platform: `<excpt.h>` at file scope for `GetExceptionCode` / `GetExceptionInformation`.
   (Including it inside `namespace rex` breaks winnt.h.)
3. `4b379a5` gpu: `PM4_CONTEXT_UPDATE` logged at debug, not info. This game sends ~5,000 a second; the log
   rotated every few seconds and lost everything else. Logging only.
4. `70cb34f` input: **test input scripts** (`input_script` setting, `include/rex/input/input_script.h` has
   the format). Timed pad events merged into the guest pad after the window-focus gate (runs work in the
   background); the XInput packet number advances on every scripted change. Off by default.
6. `d6d5bdb` audio: the XMA next-buffer boundary logged at debug (was a false error).
7. `3c5ccf5` input: `snap <name>` in input scripts, saving the presenter's guest output as a BMP
   (`input_script_snap_dir`); a same-tick `quit` runs after the snaps.
5. input: `set <setting> <value>` in input scripts (changes a setting at a time, e.g. turn on
   `vulkan_debug_log_frame_summaries_remaining` for match frames; see `tests/input/match_gpu_log.txt`).

## Boot log

1. Exe built (30 MB). Crashed at once: unregistered call to 0x825ED818 from the CRT initializer loop
   (lr 0x8251F560), a static initializer with no .pdata.
2. After the scanned functions: runs. Log seemed to show nothing but a flood of "GPU context update"
   (SDK change 3).
3. **2026-11-23: boots to the title screen (FIFA STREET 3 logo, three players, (c) 2008 EA) and the
   attract movie ("DEMO / PRESS START", VP6 video, clean picture, no green blocks).** D3D12 on the RTX 4090,
   audio endpoint opened, log clean apart from two `IoDismountVolumeByFileHandle - stub` warnings.
   Screenshots: `work/snaps/boot5_6s.png`, `boot5_60s.png`.

4. **2026-11-23, scripted input: front end and a full match start.** `tests/input/play_now.txt` on a fresh
   profile: legal screen -> title -> PRESS START -> create profile -> enable autosave -> save-icon notice
   -> lead-profile notice -> autosave-on notice -> **main menu** (~54 s) -> PLAY NOW -> Select Teams
   (Brazil v England) -> Line-ups -> Match Settings (Samba, first to 5 goals) -> arena fly-in ->
   **live match**, AI playing for 50 s, HUD and player names right. No crash, no new log warnings. EA Trax
   song cards show (menu music plays). Profile saves land in the run's user-data folder.
   Sheet: `work/snaps/play1_sheet.png`; main menu flow: `work/snaps/menu4_sheet.png`.

5. **2026-11-23, picture vs Xbox 360 footage** (IGN "Gamebreaker Action",
   youtube.com/watch?v=2hWaR0zAdfk, 720p, same Samba court). Route: `tests/input/match_quick.txt`, compare the
   88-106 s screenshots (`work/snaps/ab_*`, side-by-side `work/snaps/shadows_compare.png`).

   | Config | Player/fence/ball shadows | Player shading |
   |---|---|---|
   | D3D12, host render targets (old default) | missing | washed out, flat |
   | D3D12 + float24 depth options | missing (+3 pipeline errors) | washed out |
   | D3D12, ROV | missing | washed out |
   | Vulkan, host render targets | missing | correct |
   | **Vulkan, FSI** (`render_target_path_vulkan = 'fsi'`) | **present, as on 360** | **correct** |

   Now the default in `config/fifastreet3.runtime.toml`. A match frame has 60-85 draws and 9 resolves: 8
   screen tiles (1280x368 / 1280x336, predicated tiling) + the 1280x720 frontbuffer, no shadow-map
   texture, so the shadows are drawn in the main pass (stencil/EDRAM-dependent), which only the exact
   EDRAM path gets right. Xenia's own reports agree: Canary #259 (D3D12 players too bright, fine on
   Vulkan; no long shadows in its screenshots), Xenia #2154 (picture a bit brighter than hardware).
   Still to check: FSI cost on weaker GPUs (fine on the RTX 4090), overall brightness vs hardware, the
   Gamebreaker greyscale effect, other arenas.

   Also from those reports, for later: the game renders **native 1080p** when the console is set to 1080p
   (Xenia: `internal_display_resolution`); ours renders 1280x720 like a 720p console. Audio crackles in
   the first minutes are fixed in Xenia by `xma_decoder = "old"`; our log shows
   `XmaContext N old: input offset 16416 exceeds buffer size 16384` in match runs - to check by ear.

6. **2026-11-23, follow-up checks** (all Vulkan + FSI):
   - **Brightness**: matches the IGN footage in two views of the Samba court. Fence-side view: luminance
     91 vs 91-96 (wall 68 vs 59-66); goal-end view (IGN Brazil v USA): 49-51 vs 51-53. Green a few %
     lower in ours in both, within YouTube's colour-conversion error. No "brighter than hardware" problem
     (Xenia's remark was likely about its D3D12 path).
   - **All 7 arenas** (`tests/input/arenas/arena_N.txt`, RIGHT N times on Match Settings): Samba, Beach,
     Riverside, Mediterranean, Shipyard, Oil Rig, Rooftop. All load and play; the day arenas all have the
     long sun shadows; Rooftop matches IGN's Mexico v England / Juggling goal clips (teal floodlit court,
     soft shadows). Sheet: `work/snaps/arena1c_snaps.png`. Only Samba and Rooftop have hardware footage.
   - **Gamebreaker** (`tests/input/gamebreaker_try.txt`, run with `-Save work\saves\fresh_profile`):
     England filled theirs from our trick spam; greyscale arena, players in colour, foot glow, goal light
     beams, instant replay, red score bar: same as IGN's Gamebreaker clip
     (`work/snaps/gamebreaker_compare.png`). Goals 0-1, 0-2, replays, tricks all worked.
   - **Audio**: the `XmaContext N old: input offset 16416 exceeds buffer size 16384` errors were a
     legitimate buffer boundary (offset = buffer end + the next packet's 32-bit header); the decoder
     already handles it like every other buffer switch. Now logged at debug (SDK `d6d5bdb`). Crackle in
     the first minutes (Xenia's report) still needs a listen.
   - Reference footage (real hardware, IGN 2008, 720p): 2hWaR0zAdfk (Gamebreaker Action, Samba),
     0D8-gIXXK_A (Brazil v USA, Samba goal end), Bsa9VGKAhc8 (Mexico v England, Rooftop), eKe51Xp9Bzc
     (Juggling goal, Rooftop). 4K/60 uploads are emulator footage: don't use them.

7. **2026-11-23, 1080p + iGPU baseline** (owner's decision: 1080p is the 1:1 resolution; 60 FPS is
   measured on the integrated AMD GPU first, then the Steam Deck). Config: `resolution = '1080p'` (the
   video mode the game reads; it renders a native 1920x1080 frontbuffer), `vulkan_device = 1`,
   `log_frame_stats = true` (new SDK line, `[frame stats]` every 2 s, measured at guest swaps; perf
   counters stay off, they cost ~6% CPU). Picture at 1080p checked: shadows and shading intact
   (`work/snaps/gputrace1/traced.png`).

   The iGPU is the **Ryzen 9 9950X3D's 2-CU RDNA 2 Radeon** (about a third of a Steam Deck's 8 CUs), with
   no display attached, so every frame is also copied to the 4090's screen.

   `tests/input/perf_match.txt` with `-Save work\saves\fresh_profile`, summarised by
   `python tools/frame_stats.py work/runs/<run>.log`:

   | iGPU run | main menu | match | match windows at 60 |
   |---|---|---|---|
   | **1080p, FSI (current, 1:1)** | 39.3 | **37.9** | 0% |
   | 720p, FSI | 38.7 | 59.2 | 93% |
   | 1080p, host render targets | 31.2 | 17.4 | 0% |
   | 1080p, FSI, vsync off | 35.5 | 5.4 | 0% |

   - The match is **GPU pixel-bound**: 720p holds 60, 1080p (2.25x the pixels) takes ~26.6 ms a frame.
     Needed: ~40% less GPU time per frame at 1080p.
   - FSI is the fast path here: host render targets are 2x slower (EDRAM ownership copies around the
     game's predicated tiling).
   - vsync off is not a measurement: the 1000 Hz vblank breaks the game's pacing (5 FPS).
   - The **main menu is 39 FPS at both resolutions**: a fixed cost, separate from the match.
   - GPU timestamp trace (`tests/input/perf_gputrace.txt`, `vulkan_gpu_timestamp_trace_frames`): per-draw
     queries inflate a frame to ~155 ms, so only shares count: nearly all time is "Main" (FSI colour/depth
     draws); one interval is ~1/3 of the frame. The pause menu adds full-size resolves (CopyResolveFull32).

8. **2026-11-23, where the frame time goes** (1080p FSI unless noted; `vulkan_gpu_timestamp_buckets` gives a
   120-frame average with few queries, so it barely disturbs the GPU; per-draw tracing does).

   | Run | match FPS | GPU frame span | biggest parts |
   |---|---|---|---|
   | AMD iGPU 1080p | 39 | 24.9 ms | FSI draws (Main), Copy, resolves, SMUpload ~8 ms; ResolveGap only 4.9 ms |
   | RTX 4090 1080p | 28 | 28.3 ms | **ResolveGap 14.9 ms**, Main 10.1 ms |
   | RTX 4090 1080p host RTs | 29 | 28.3 ms | Main 12 ms (+ gaps) |
   | RTX 4090 720p | 31 | 26.8 ms | **ResolveGap 17.7 ms**, Main 6.2 ms |

   - **iGPU: really GPU-bound** (~20 ms of the 25 are work). 60 FPS needs a third less GPU time.
   - **4090: idle, not busy.** After resolves the GPU waits ~15-18 ms a frame for the next commands, at any
     resolution and on both render-target paths: a CPU/GPU round trip somewhere in the resolve handling. It
     would hit every NVIDIA player, and probably part of the iGPU's time too. Top suspect to fix first.
   - CPU (`tools/sample_threads.py`, IP sampling + llvm-symbolizer on the PDB): the GPU command thread is ~5%
     busy. A guest thread sits at 100% in the game's D3D "wait for GPU fence" spin loop (sub_82505678,
     `mr r31,r31` spin hints) - it waits, it doesn't work. The game logic thread is the other 100% thread.
   - Dead ends (iGPU): `native_2x_msaa=false` (5 FPS), `occlusion_query_enable=false` (4.5 FPS),
     `vsync=false` (5 FPS) - all three push the game into some very heavy path; not 1:1 anyway.
   - Per-draw trace (`vulkan_gpu_timestamp_per_draw`, `tools/gpu_trace.py`): the arena (vs 997005999EE7A298,
     4 pixel shaders, ~680 draws across the screen tiles) is ~75% of the draw time; shares only, because
     per-draw timestamps overlap.

9. **2026-11-23, CORRECTION + the idle gap.** Four earlier "match" baselines never reached the match: the
   route's single Start at 20 s missed the title (PRESS START shows at 19-25 s) and they measured the
   title screen. Invalid: `perf_amd_1080_fsi` (the "38 FPS" iGPU baseline), `perf_amd_720_fsi`,
   `gpuspan_amd`, `perf_4090_1080_fsi`, `pq_4090_fs`. The "random 5 FPS mode" was simply the real match.
   Now: `perf_match.txt` presses Start three times, and `python tools/check_match.py <run>` checks the
   snaps for the match scoreboard - **run it for every perf number**.

   Verified (all snaps in a match), 1080p:

   | Run | match FPS | GPU frame span |
   |---|---|---|
   | RTX 4090, window | 29.4 | 27.6 ms; CP blocked 21.7 ms/frame in submits |
   | **RTX 4090, fullscreen** | **60.0 (100% of windows at 60)** | 15.4 ms |
   | AMD iGPU, fullscreen, FSI | 5.2 | 164 ms (Main 160) |
   | AMD iGPU, fullscreen, host render targets | 19.5 | 49.8 ms |

   **Idle gap (fixed):** CP waits instrumented into `[frame stats]` (wait_reg_mem / no commands / gpu fence /
   swap / submit [replay, queue lock, vkQueueSubmit]). In a window, the presenter's vkQueuePresentKHR
   blocks one display refresh (~6 ms at 165 Hz, compositor) and presents every refresh; the CP waited
   ~20 ms/frame for the queue lock (then, with a separate present queue, inside vkQueueSubmit - NVIDIA
   serialises across queues during present). Exclusive fullscreen: present 0.08 ms, submit 0.2 ms, 60 FPS.
   **`fullscreen = true` is now the default.** SDK keeps: present from a separate queue
   (`vulkan_present_separate_queue`, a 2nd graphics queue or another present family), WAIT_REG_MEM polls
   for `wait_reg_mem_spin_us` (2 ms) before sleeping. Windowed is still slow on NVIDIA (to do: present only
   new guest frames, not every display refresh).

   **AMD:** FSI is unusable on RDNA 2 (5 FPS; the driver emulates fragment-shader interlock - the Steam Deck
   is RDNA 2 too). The 1:1 shadows must work on the host-render-target path, which then needs ~3x (49.8 ->
   16.7 ms GPU). Next.

10. **2026-11-23, how the player shadows are drawn** (`tools/draw_states.py` on a `debug_log_draws_ps=*` log,
    `tests/input/draw_state_log.txt`). Per player, at the start of the frame: clear a small 1x target
    (pitch 320, 8888 + D24FS8) by drawing a quad on a **4x MSAA alias of the same EDRAM at pitch 160**
    (ps 2E372EA2, stencil replace), draw the player into it (ps 8DF5463F, ~10 draws), resolve it to a
    256x256 texture (the 14 x 0x40000 resolves). Later the court draws project those textures. Suspect for
    the host-render-target path losing the shadows: the 4x-over-1x alias clear needs an ownership transfer
    between two host render targets with different sample counts. Resolve dumps of both paths for checking
    this are in `work/dumps/fsi` and `work/dumps/host` (tiled 256x256 8888; to untile and compare).
    Skipping 457C0FC6 = untextured players (player shader); F2768D26/4B43E140/A51BF2F8 no visible change.
    **Parked:** the owner moved the baseline to the Steam Deck; whether FSI is slow there (RADV) decides
    whether this is needed.

11. **2026-11-23, baseline is now the Steam Deck, 1280x720, locked 60 FPS** (owner; the PC iGPU was a poor
    stand-in). Config: `resolution = '720p'`, GPU automatic. Deck route (as in Super Odyssey): the Windows
    build under Proton. `tools/make_deck_package.ps1` -> `dist/fifastreet3-deck` (exe, DLLs, app-local VC++
    runtime, config, portable.txt, tools/deck, tests/input); `tools/deck/push.sh [--with-game]` copies it
    over SSH; on the Deck `tools/deck/run.sh <name> [script]` runs it under Proton Experimental with
    per-run userdata/snaps/logs. Not tested yet: the Deck (deck@192.168.1.82 in Super Odyssey) did not
    answer SSH on 2026-11-23.

12. **2026-11-23, Steam Deck running + first verified baseline.** Deck OLED (Galileo, RADV VANGOGH),
    SteamOS 3.9.5, Gaming Mode, stock power settings (perf level auto, 15 W cap, GPU up to 1600 MHz),
    Windows build under Proton Experimental (11.0-100), docked to a 1080p screen (1920x1080 swapchain).
    - Tools: `tools/deck/remote_run.sh <name> <script> [flags]` (SAVE=1 for the saved profile) runs a
      script as a `systemd-run --user` unit (plain `setsid nohup` over SSH got killed) and fetches the log
      and snaps. Copying the package + game took 27 s.
    - Route fixes: the Deck's load is slower, so fixed-time Start presses missed the title. New script
      commands (SDK): `waitpixel X Y R G B tol ms` and `pressuntil BUTTON period X Y R G B tol ms`
      (pixel of the game's own output, 1280x720 coordinates). Title marker (410,446)=231,231,231 (FIFA
      logo), main-menu marker (296,104)=230,230,230 ("MAIN MENU" heading). PC: title 13.7 s, menu 19.8 s.
    - `portable.txt` used to override `--user_data_root`, so Deck runs ignored the test save; fixed in
      `src/fifastreet3_app.h` (portable is only the default now).

    | Deck, 720p, verified match | match FPS | GPU frame span | notes |
    |---|---|---|---|
    | FSI (1:1 picture) | **34.4** | 30.9 ms | Main (FSI draws) ~26 ms, ResolveGap 2.3 |
    | host render targets (no shadows) | 37.4 | 31.8 ms | Main 11.4, ResolveGap 12.5, RtTransfer 3.8, CopyDump 3.1 |

    During the host-path match: **GPU 99% busy at 1040-1100 MHz** (not 1600): the 15 W budget is shared,
    and the CPU used ~3.3 cores (a guest thread "STREET:..." 92% - likely the D3D fence spin loop seen on
    the PC; "Main XThread" 63%; GPU command processor 57%; UI/presenter 39%; audio 22%). As in Super
    Odyssey: CPU time saved = GPU clock on the Deck. Next: cut the spinning (guest D3D spin loop, the new
    WAIT_REG_MEM spin), then GPU work per frame (FSI draw cost; host path's transfers/copies).

13. **2026-11-23, fence-wait hook (CPU -> GPU power on the Deck).** `src/fifastreet3_hooks.cpp` overrides
    sub_82505678 (one step of D3D's BlockOnFence spin, 4 x 8 `or r31,r31,r31` db8cyc hints that are no-ops
    when recompiled). The original still runs every step (fence compare, hang check, return value
    unchanged); after `fs3_fence_wait_spin_us` (200) of waiting it sleeps `fs3_fence_wait_sleep_us` (250)
    via NtDelayExecution (Win32 Sleep and the SDK's Sleep round to 1 ms / yield). Deck results (720p,
    verified matches, `tools/deck/sample.sh` mid-match):

    | Deck run | match FPS | GPU clock / busy | spinning "STREET" thread |
    |---|---|---|---|
    | FSI, before | 34.4 | (not sampled) | 92% (host run) |
    | FSI + hook | 34.3 | 1600 MHz, 99-100% | 29% |
    | host RTs, before | 37.4 | 1040 MHz, 99% | 92% |
    | **host RTs + hook** | **44.9** | 1040 MHz, 84-100% (15-18 W) | 25% |
    | host RTs + hook, `wait_reg_mem_spin_us=0` | 42.5 | 1040 MHz | 30% |

    FSI is purely GPU-bound on the Deck (~29 ms GPU at full clock): it can't reach 60. The host path can
    get much closer but needs the shadows fixed (entry 10) and ~25% more. Its GPU profile:
    Main 9.6 ms, ResolveGap 10.4, PassEntry 4.2, RtTransfer 3.1, CopyDump 2.8, SMUpload 2.3 (per frame,
    overlapping). Remaining CPU: Main XThread 65%, GPU CP 50%, UI/presenter 39%, 2 XThreads ~30%, audio 20%.
    WAIT_REG_MEM spin stays on (slightly faster).

14. **2026-11-23, CORRECTION: Vulkan host render targets do draw the shadows.** Entry 5's table said
    "Vulkan, host render targets: shadows missing" - wrong: those frames had the players in shade. With
    `tests/input/shadow_probe.txt` (snaps later in the match, players in the sun), host and FSI both show
    the long player, ball and fence shadows (`work/snaps/sh_detail.png`); the Deck's host run too.
    How they're drawn (draw-state log): per-player shadow textures (entry 10), then the court is re-drawn
    with **depth EQUAL** and a multiply blend sampling them - so the shadows depend on depth matching
    exactly between passes. Resolved shadow textures (`tools/untile.py`, `work/dumps`) match on both paths.
    All 7 arenas host vs FSI (PC, `tests/input/arenas`, sheet `work/snaps/arenas_fsi_vs_host.png`):
    visually equivalent. Pixel diffs need the same frame (camera moved between captures) - a deterministic
    frame capture is a to-do. **Default renderer is now `render_target_path_vulkan = 'host'`.**
    (The D3D12 washed-out players is real and separate; D3D12 isn't used.)

15. **2026-11-23/24, Steam Deck to ~60 FPS: what actually limited it.** Method first, because the early
    numbers misled:
    - **Benchmark:** `tests/input/perf_pause.txt` pauses a match and measures the frozen scene (the game
      keeps rendering it). The frozen scene still differs per run (camera and AI depend on load timing;
      no stick input doesn't help), from 33 to 148 draws, so `tools/deck/bench.sh` prints each run's
      draw / render-pass count next to its FPS. Full matches (`perf_match.txt`) for final checks.
    - **1:1 check for GPU options:** freeze the frame under the pause menu, snap, switch the option on
      with `set`, snap again, `tools/imgdiff.py` (`tests/input/pause_ab*.txt`). 0 px differ for barrier
      elision and the three resolve/descriptor options.
    - **Thread monitor** (`tools/deck/monitor.sh`, started by run.sh): per-thread CPU and core, CPU MHz,
      GPU clock, APU W every 4 s. **winsampler** (`tools/winsampler`, `tools/deck/winsample.sh
      phase:pause`) samples thread IPs inside Proton; Wine's ntdll by export (`ZwDelayExecution` etc. are
      blocked threads, the rest is real work).

    Findings, in the order they mattered:
    - **Render-pass churn:** every shared-memory barrier ends the render pass, and the game's GPU
      skinning (one vertex-shader memexport draw per character part, interleaved with normal draws)
      made ~200 such pass ends a frame. SDK `vulkan_memexport_barrier_elision` tracks exact read/write
      ranges and barriers only on a real hazard: passes per frame 182 -> 82, pixel-identical. The other
      ~60 pass ends are resolves and RT transfers (inherent).
    - **Controller polling:** every guest input call re-enumerated XInput devices; under Proton each is a
      blocking device I/O round trip. Main game thread 65% blocked in `NtDeviceIoControlFile`.
      `input_device_refresh_ms = 1000`.
    - **The game's spin-wait:** with input fast, the main loop's frame wait called SwitchToThread
      ~500,000 times a second (`debug_log_guest_yields`, `fs3_log_yield_sites`: one call site,
      0x8220BD58). Hook on the helper `sub_824765D0` adds a short sleep after 100 us (hooks file).
    - **CPU clock trap (the 30 FPS runs):** the Deck CPU (amd-pstate-epp, balance_performance) clocks
      each core by its own activity. Once a frame slips to 30 FPS every thread idles half the time, the
      CPU drops to ~2.35 GHz and stays there - half of the runs locked at exactly 30.0. Letting the CP
      thread poll through its per-frame wait (`wait_reg_mem_spin_us = 50000`) keeps the clock at 3.3 GHz:
      6/6 runs 57-60 FPS. (Lesson: on the Deck, removing a spin can make it slower.)
    - **UI tick thread:** Proton emulates `WaitForVBlank` by polling (~40% of a core);
      `dxgi_ui_tick_timer` ticks from a waitable timer instead (~5%).
    - Smaller: vectorised `vrsqrtefp` (exhaustively bit-exact, ~2x), job-worker idle backoff, fence-wait
      hook (entry 13), resolve/descriptor options.

    Paused-scene benchmark, 6 runs each: 49.5 +- 15 FPS (bimodal 30/60) -> 59.6 +- 0.6, CPU 3.3 GHz,
    APU 13-14 W. Scenes above ~130 draws still dip to ~58.

    Full matches (`perf_match.txt`, ~60 s of play): 58.7 +- 0.3 in the first batch of 5, but the same
    build gave 56.2 +- 1.4 an hour later. **Batch-to-batch drift (2-3 FPS) is bigger than the spread
    inside a batch**, so separate batches can't decide an A/B: `tools/deck/bench_ab.sh` alternates A and
    B run by run and reports the paired difference. The dips are single missed frames (33 ms) while the
    GPU is 100% busy at ~850-1040 MHz (power-limited: APU 13-15 W) - a per-draw GPU trace of a paused
    frame (`tests/input/perf_pause_trace.txt`) shows no idle gaps; the game's own drawing dominates
    (one skinned-character vertex shader ~1M vertices a frame over ~290 draws). What's left is GPU clock,
    i.e. CPU power.

    Interleaved A/B results (4 pairs of full matches each, paired difference B - A):
    | B (vs the defaults)                                        | B - A          | kept? |
    |------------------------------------------------------------|----------------|-------|
    | `wait_reg_mem_spin_pause` (pause-hint spin instead of yield) | -4.3 +- 1.2 FPS | no (GPU clock lower) |
    | `vblank_wine_precise_sleep` (precise vblank sleep, 250 us margin) | +1.3 +- 3.2 | no (noise) |
    | `vulkan_skip_inert_no_pixel_draws` + `vulkan_reuse_unchanged_draw_bindings` | -0.7 +- 1.2 | no |

    **The frozen-frame pixel test is blind to memexport.** `vulkan_skip_inert_no_pixel_draws` passed it
    (0 px) while skipping the GPU-skinning draws (no pixel shader, depth off): a paused pose never
    changes, so stale skinned vertices look identical. Fixed in the SDK (memexport draws are never
    inert); check the memexport draw count too (`debug_log_memexport_remaining`) - 96 a frame before and
    after. Barrier elision was checked by construction and counters (one read-after-write barrier per
    skinning draw remains) plus live-play snaps.

    **PC check:** the long CP spin made the 4090 PC drop to 40 FPS (`vkQueueSubmit` blocking ~12 ms a
    frame on NVIDIA); it now applies under Proton only (`wait_reg_mem_spin_wine_us`), PC locked at 60
    over a whole match again. Known issue: an occasional hang at quit under Proton (one in ~40 runs
    tonight, also seen before these changes): the process stays until killed.
    `wait_reg_mem_spin_wine_adaptive` (long spin only after a slow frame): +0.5 +- 3.4, not kept.

    **All arenas on the Deck** (final config, 2026-11-24 03:10, `tests/input/arenas`, ~20 s of play each;
    Samba from `perf_match`):

    | Arena (RIGHT x N from Samba) | match FPS | 60 held |
    |---|---|---|
    | Samba (0) | 55-59 | 45-79% |
    | 1 | 58.7 | 33% |
    | 2 | 56.9 | 44% |
    | 3 | **31.2** | 0% |
    | 4 | **45.5** | 22% |
    | 5 | 55.2 | 44% |
    | 6 | 59.3 | 80% |

    Arena 3 is GPU-bound, not a clock trap: CPU at 3.35 GHz, the GPU at its usual ~1040 MHz while busy
    (the 4 s samples alternate idle 200 MHz / busy 1040 MHz), and the game's own drawing alone takes
    11.2 ms a frame there (6.8 on Samba) before the emulation overheads (transfers, pass entries,
    skinning drains, uploads: ~2-3 ms each). A locked 60 there needs ~20%+ less GPU work or more GPU
    clock (the 15 W budget; the owner's TDP / GPU clock settings) - next steps: GPU-side profiling
    (RADV thread trace) of the resolve -> texture reload path and RT transfers.

16. **2026-11-24, native Linux build for the Steam Deck (no Proton).** Built in WSL2 Ubuntu 24.04 (Clang
    20.1.8, preset `linux-amd64-relwithdebinfo` with `-march=x86-64-v3`), sources synced to `/root/fs3`;
    `tools/make_deck_package_linux.sh` -> `dist/fifastreet3-deck-linux`, on the Deck in
    `~/fifastreet3-native` (uses the Proton package's `game/` and saves). Run with
    `RUN=run_native.sh DEST=fifastreet3-native tools/deck/remote_run.sh ...`; `bench_ab.sh` takes
    `B_ENV=...` to compare against Proton. Runs on SteamOS 3.9 as is (glibc 2.43, all libraries present);
    codegen on Linux gives byte-identical C++; boots, menus, matches, quits cleanly.

    Native-only problems found and fixed in the SDK:
    - **UI paints unthrottled** (no vblank tick source off Windows): UI drawers re-request a paint after
      every paint, the UI thread re-presented flat out (~52% of a core). `ui_tick_rate_hz` (60).
    - **POSIX multi-object waits spun:** the poll sleep was truncated to whole milliseconds, so in the
      1 ms alertable slices it was always `sleep_for(0)` - the audio worker sat at ~70% of a core. Fixed
      (sub-ms sleep, alertable slice rounded up). Audio still exactly real time (187.5 frames/s).
    - **CPU clock trap** natively too: the long CP spin now applies on Linux as under Proton.
    - Packaging took a stale GPU plugin (staged copy only refreshed when the binary relinks): the
      package now takes the SDK's own `out/linux-amd64` libraries.

    - **The big one - write-watch faults parsed `/proc/self/maps`:** natively the game ran at ~28-30
      FPS (interleaved full matches: 27.6 vs Proton 55.1). Ruled out on the way: shaders (SPIR-V
      identical apart from constant order), device features, memory types, Mesa environment, present
      mode, the compositor (a Steam launch didn't help), per-draw GPU cost (main menu identical). The
      GPU timeline showed ~10 ms "stalls inside draws" - really GPU idle across a submission gap.
      Stack samples (`tools/deck/stacksample.sh`, eu-stack; the game opts in to being traced with
      `FS3_ALLOW_PTRACE=1` because SteamOS has `ptrace_scope = 1`) showed the render thread a third
      of the time in `MMIOHandler::ExceptionCallback -> QueryProtect -> FindEntryForAddress ->
      getline(/proc/self/maps)`: every guest write to a GPU-watched page parsed the whole maps file.
      Fixed in the SDK with an interval map of the protections the memory layer sets. Native paused
      scene 30 -> 59.8 FPS, match 60 held 100%, main draws 17.7 -> 7.1 ms (= Proton).

    **Result (interleaved, full matches with play, 3 pairs):** native 59.9 FPS (60 held 97-100% of
    2 s windows) vs Proton 52.2 - paired +7.7 +- 0.6. Heavy arenas natively: arena 3 59.2 FPS (Proton
    31.2), arena 4 59.8 (Proton 45.5). **CORRECTION to entry 15:** arena 3 was not simply GPU-bound -
    the frame-time difference was host overhead around write-watch faults (Wine's exception path under
    Proton); natively it holds ~60. The native build is now the faster way to run on the Deck.

    **Steam launch:** the native build is in the Deck's Steam library as a non-Steam game
    (`tools/deck/steam_launch.sh`, added via `steam://addnonsteamgame`, app id 3747935355); launched
    from Steam it starts the game, gamescope enables its WSI layer (1280x720 mailbox swapchain). For a
    scripted run through Steam, write `steam_next_run` (see the script) and open
    `steam://rungameid/16097259777280704512`. Launched outside Steam, `ENABLE_GAMESCOPE_WSI=1` hangs.

17. **2026-11-24, Steam Deck at 10 W and an audio check (native build).** TDP set to 10 W by the owner
    (`power1_cap` 10 W). Interleaved full matches: **59.6 FPS** (60 held 62-83% of windows) at ~9.2 W
    APU; the long CP spin still helps there (off: 59.1, paired -0.47 +- 0.12, no watts saved).

    **Audio, measured** (`tools/deck/audiorecord.sh` records the output sink's *monitor* during the
    match + PipeWire's `pw-top`; `tools/audio_check.py` finds gaps >= 4 ms inside audible sound, clicks,
    clipping, level, balance, spectrum, and plots; self-tested on a synthetic signal). The native build
    had two problems the Proton build didn't:
    - **Downmix:** a docked Deck's stereo HDMI sink reports 6 ch to SDL, so the SDK passed 5.1 through
      and PipeWire folded it (LFE in, not normalised): ~5 dB hotter, clipping, bass-heavy. New
      `audio_force_stereo` (config: true) folds with the standard weights; levels now match Proton.
    - **~100 empty 5 ms frames a minute** (audible crackle): PipeWire ran the native stream at a 1024
      quantum, so each callback drained four guest frames at once and the XMA decoder (polling every
      2 ms) fell behind inside the burst - the *game* produced silent frames (new `audio_stats` counter
      `guest_zero_frames`, 0 for Proton). `audio_device_sample_frames = 256` (one guest frame): 0 empty
      frames, 0 gaps, quantum 256 with 0 xruns; also 0 on the PC (Windows), still 60 FPS on both.
    Final native recording (60 s of play, shipped config only): 0 gaps, 0 clipping, the 2 click
    candidates are kick-sound onsets (checked up close), audio exactly real time.

18. **2026-11-24, toward 100% at 10 W (native).** Per-frame timing (`debug_log_frame_timing`,
    `tests/input/perf_match_frames.txt`, `tools/frame_timing.py`): at 10 W **98.9-99.7% of frames are
    on time**, 11-38 single 33 ms frames a minute. First reading "the GPU command thread is saturated
    (16.3 ms CPU per 16.7 ms frame)" was **wrong**: its CPU time includes the frame-boundary spin, and
    stays at 16.3 ms whatever its workload. Two cuts to its per-draw work changed nothing (interleaved,
    3 pairs each): inert no-PS draw skip + binding reuse -0.03 FPS; new
    `gpu_skip_unchanged_fetch_constant_writes` (the game writes ~21k fetch-constant dwords a frame,
    ~60% unchanged; pixel-identical, off by default) -0.03 FPS. Kept: a known-register bitset instead
    of `GetRegisterInfo`'s big switch on every register write (bookkeeping only). The misses most
    likely come from the GPU at 10 W clocks (~700-850 MHz); next: measure GPU busy per frame
    (per-submission timestamps, excluding idle between submissions) before cutting GPU work.
    Follow-up: GPU busy per frame is the same in missed and on-time frames; the missed ones are the
    command thread's own work taking twice as long (preempted or sharing its core). New
    `gpu_command_thread_cpu` (Linux) gives it a core: missed frames per match 10/11/9 -> 5/2/2, config
    CPU 0. CPU 0 vs CPU 6 (another core, in case CPU 0's housekeeping/interrupts were the cause),
    interleaved, 3 pairs: 5/9/4 vs 7/5/12 missed per ~3600 frames, no difference; interrupts per CPU
    are even. The rest still come every ~5 s (multiples of 299 frames) with the thread off-CPU ~20 ms.

Next: Phase 4 list in PLAN.md: a whole match to the final whistle, goals and replays, the other modes,
audio check by ear, saves reloaded, 2-player.

## Test loop

- `tools/iterate.ps1 [-RunSeconds N] [-SkipCodegen] [-SkipBuild]`: codegen, build, copy DLLs + config,
  junction `game`, run, print the log tail.
- **Scripted runs**: `& .\tools\run_script.ps1 -Script tests\input\<x>.txt -At 10,20,30 [-Name n]
  [-Save work\saves\<folder>]`. Launches with `--input_script`, captures the window at those seconds
  (`work/snaps/<n>_<t>s.png`), waits for the script's `quit`, copies the log to `work/runs/<n>.log`.
  Each run gets a fresh user-data folder `work/userdata/<n>` (or a copy of `-Save`), so runs repeat and
  your Documents\fifastreet3 save is not touched; shader cache shared in `work/cache`.
  `python tools/sheet.py <n>` makes a contact sheet of a run's screenshots.
- Scripts: `tests/input/press_start.txt`, `to_main_menu.txt`, `play_now.txt`. Starting saves:
  `work/saves/fresh_profile` (profile created, autosave on).
- **Timing is wall-clock ms.** About 1 run in 5 of the fresh-profile route ended back on PRESS START:
  the legal screen lasts to ~16 s and PRESS START appears at ~19 s, so the old first Start (13 s) always
  lands on the legal screen and everything depends on later presses. **Prefer `-Save
  work\saves\fresh_profile` and one Start at 20 s** (title -> main menu, no profile dialogs; see
  `gamebreaker_try.txt`). Leave ~5-6 s between menu presses.
- **Frames: use `snap <name>` in scripts** (SDK `3c5ccf5`): saves the game's own output to
  `work/snaps/<run>/<name>.png`. Screen copies (`-At`) break whenever another window covers the game.
  `python tools/snapsheet.py <run> [<run>...]` makes a sheet.
- Help cards pause the match until A; to turn them off: B, UP (to DISABLE), A.
- Screenshots of an already running game: `& .\tools\snap.ps1 -NoLaunch -At 5,20`.

## Tried and wrong

- Naming a tool script `dis.py`: it shadows Python's `dis` module and breaks capstone's import. It's
  `tools/ppcdis.py`.
- Capturing the game window with `PrintWindow` or `SetForegroundWindow`: both block (they wait on the
  game's window thread). Use an async `SetWindowPos` + screen copy, DPI-aware.
- `snap.ps1` launching the game itself with `Start-Process -PassThru` and polling it: the script stalls
  before its first capture. Launching separately and capturing with `-NoLaunch` works.
- `TaskStop` on a task that launched the game also kills the game.
- Passing `--input_script=<path>` through `Start-Process -ArgumentList` without quotes: the path splits at
  the space in "Fifa Street 3".
