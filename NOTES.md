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
