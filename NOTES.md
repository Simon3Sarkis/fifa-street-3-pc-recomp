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
