# FIFA Street 3 (Xbox 360) PC port: plan

Written 2026-11-23. Two stages, in this order and never mixed:

1. **Stage A: 1:1.** A Windows exe that runs the Xbox 360 game exactly as the console does. No changes, no extras.
2. **Stage B: features.** Only after Stage A passes its gate (section 6).

Method: static recompilation of the game's PowerPC `default.xex` to C++ with the ReXGlue SDK, linked against its
Xbox 360 runtime (kernel, XAM, Xenos GPU on D3D12/Vulkan, XMA audio). Same method as all five reference projects.

---

## 1. Where we start (checked on this PC, 2026-11-23)

| Item | State |
|---|---|
| `F:\Fifa Street 3` | Empty, not a git repo. **No game files here yet.** |
| Git, GitHub CLI | Installed |
| Visual Studio 2022 | A `Program Files (x86)\Microsoft Visual Studio\2022` folder exists (likely Build Tools); C++ workload not verified |
| LLVM/Clang, CMake, Ninja | Not found on PATH or in the default install folders |
| Python | Only the Windows Store alias found; a real install not verified |
| Drive F: free space | Not checked yet (need roughly 30 GB: ISO, extracted disc, SDK build, generated code) |

## 2. What I need from you

1. **The game**: the Xbox 360 ISO or extracted folder, and where it is. Region and whether a title update exists.
2. **The 1:1 reference**: what we compare against. Best is a real Xbox 360 capture; otherwise Xenia Canary
   screenshots/video, plus public 360 footage. Tell me which you have.
3. **OK to install** LLVM/Clang 20+, CMake 3.25+, Ninja, Python 3 and the VS C++ workload (or install them yourself).
4. **Play testing**: you play, I measure. Same split as Super Odyssey.

## 3. What each reference project gives us

| Project | Use it for | Notes |
|---|---|---|
| [rexglue-sdk](https://github.com/rexglue/rexglue-sdk) | The recompiler and runtime | Early development, API changes between releases: **pin one version** as a submodule. Docs at docs.rexglue.com. |
| [Skate1Recompiled](https://github.com/cookingwithsimon/Skate1Recompiled) | **Main template.** Project layout, manifest, `tools/` (ISO extractor, iterate loop, missed-function scanners, screenshot and input scripts), NOTES.md boot log | Closest match: an EA 360 game from 2007. FIFA Street 3 (EA Canada, 2008) probably shares the EA tech (BIG archives, VP6 movies, EA audio). To verify in Phase 1. GPL-3. |
| [Condemned2Recomp](https://github.com/psxrestore/Condemned2Recomp) | Function config split into files (base / named / CRT / midhooks), a `_codegen` CMake target, title-update handling | BSD-3. Hooks and launcher are Stage B material. |
| [PGR4_2026](https://github.com/tazzkiller/PGR4_2026) | The best `analysis.toml` example: setjmp/longjmp, SEH funclet splits, `[rexcrt]` heap and memcpy mapping, import audit | No licence file: read for method, don't copy code. Its custom renderer and upscalers are Stage B at most. |
| [fm4-static-recomp](https://github.com/i2kiselev/fm4-static-recomp) | Discipline: supported-disc list with hashes, code-image check, debug/A-B options doc, how to read an FPS overlay honestly, scripted input routes | BSD-3. Uses ShiftGlue (a different SDK fork); we don't switch SDK. |

Fixes already found by those projects that we check first, before debugging anything ourselves:

- `vpkuwus`/`vpkuhus` codegen aliasing bug: green blocks in every VP6 video (Skate 1). Check if our pinned SDK has it.
- `vsync = false` makes the runtime fire vblank at 1000 Hz, so a game that paces on vblank runs uncapped. 1:1 needs `vsync = true`.
- Alternating-frame flicker: GPU page-state bug (`clear_memory_page_state`, fixed in SDK 0.10).
- Shadow acne: `depth_float24_convert_in_pixel_shader`, `depth_float24_round`.
- CPU reads of GPU-drawn targets come back black unless resolve readback is on for that target (Skate 1 photos).
  FIFA Street 3 candidates: replay thumbnails, player/kit previews.
- `gpu_plugin = 'xenos'` must be set since SDK 0.10 or the game starts with no GPU.
- ~690 failed file opens in Skate 1 were normal loose-file probes for data inside `.big` archives: not errors.
- `REXGLUE_ENABLE_PERF_COUNTERS=OFF` saved ~6% CPU and exposed a hidden race: test with it both ways.

## 4. Lessons from Super Odyssey PC Recomp that apply here

1. **One target until it is done.** One disc version, Windows only, one SDK version. No second region, no Linux, no
   Deck until Stage A passes.
2. **Measured, not guessed.** Every claim comes from a run: which build, which place in the game, which numbers.
3. **Compare with the console before "fixing".** Twice in Odyssey (and once in Skate 1, the shadow under the board) a
   "bug" was the original game's look. Check the reference first.
4. **A scripted loop from day one**: build, launch, feed inputs, screenshot at a fixed frame, read the log. A change is
   tested in minutes and the same run can be repeated before and after.
5. **Every deviation has a switch.** Any setting or patch that differs from stock behaviour can be turned off in one
   run, so a regression is confirmed or ruled out quickly.
6. **Keep a "tried and wrong" list** so no session repeats a dead end.
7. **Test copies only.** Never the original ISO, your real saves or another project's files.
8. **Everything local.** No push, release or public post until you say so in that conversation.
9. **Test conditions matter.** Monitors on (a dark screen throttled Odyssey to ~4 FPS and voided tests); watch
   committed memory in long runs (a leak shipped once that only showed there).
10. **Hitch logging is always on** in test builds: a frame over 2x the target is logged with where the time went.
11. **Docs that survive sessions**: `docs/AI-HANDOFF.md` (goals, state, rules, loop), `NOTES.md` (boot log, fixes),
    `docs/BUGS.md`, `docs/PARITY.md` (the 1:1 checklist with evidence).

## 5. Stage A: the 1:1 port

Rules for all of Stage A: stock game code, no hooks that change behaviour, native 1280x720 render, original frame
rate and pacing, `vsync = true`, controller input only, no added UI. Only fixes that make the PC result match the
console are allowed, and each one is written down with its evidence.

### Phase 0: setup (about a day)
- Install the toolchain; `git init` (local only); `.gitignore` for `game/`, `generated/`, `work/`, `out/`.
- Create the project from the Skate 1 layout: `fifastreet3_manifest.toml`, `config/`, `src/`, `tools/`, `docs/`.
- Pin the SDK as `third_party/rexglue-sdk`. Decision at this point: upstream latest stable vs the Skate 1 fork
  (v0.10.0 + EA fixes). **Recommendation: start from the Skate 1 fork**, because its EA video and readback fixes are
  proven; move to upstream only if it has them.
- **Done when:** the `rexglue` tool builds and runs on this PC.

### Phase 1: know the game
- Extract the ISO to `game/` (a copy). Record SHA-1 of the ISO and `default.xex`, title ID, media ID, version, region
  in `config/supported-discs.json`. Refuse other images.
- `rexglue dump-xex`: image base, size, sections, entry point, compression/encryption, import list. Audit every
  kernel/XAM import against the SDK (implemented / stub / missing).
- Find setjmp/longjmp, SEH use (`__C_specific_handler`), extra XEX modules, title update (`default.xexp`).
  Decision: base disc or title update as the 1:1 target. **Recommendation: whatever a patched retail console runs.**
- Inventory the data: archives, movie format, audio format, save format.
- Capture the reference set (section 6) from the console or Xenia Canary.
- **Done when:** `docs/ANALYSIS.md` holds all of the above.

### Phase 2: clean codegen
- Run codegen; fix unresolved calls in `config/*_functions.toml` (missed tail-call targets, shared tail blocks,
  catch funclets, leaf functions without `.pdata`). Run the data-pointer and code-pointer scanners for missed
  functions; drop false positives that split real functions.
- **Done when:** zero unresolved calls, zero fatal stubs, the exe links.

### Phase 3: boot
- Follow crashes one by one with the guest backtrace; keep the numbered boot log in `NOTES.md`.
- Build the scripted loop: `tools/iterate.ps1`, `snap.ps1`, input playback.
- **Done when:** intro movies play and the title screen is reached with no crash.

### Phase 4: everything works
In this order: front end and menus, an exhibition match start to finish, audio (music, crowd, commentary, SFX),
all movies, controller and rumble, profile and saves (create, save, quit, load), every game mode offline,
local multiplayer (2-4 pads), replays. Online play is out of scope (servers are gone); the menu must fail the same
way a console without Xbox Live does.
- **Done when:** every row in `docs/PARITY.md` is "works", with no crash in a 2-hour session.

### Phase 5: prove 1:1
Work through the gate in section 6 area by area. Each difference found is either fixed, or shown to be original
behaviour with reference evidence.

### Phase 6: package
- A portable folder: `fifastreet3.exe`, runtime DLLs, config, a setup script that extracts the user's own ISO.
  No game files inside. Tested from a clean folder on a different working directory.

## 6. The 1:1 gate (Stage A is finished only when all of these hold)

| Area | Check | Evidence |
|---|---|---|
| Picture | Same-spot screenshots match the reference at 1280x720: geometry, textures, lighting, shadows, post effects, HUD, fonts | Side-by-side set: menus, every venue, day/night, replays, cutscenes |
| Movies | Every movie plays, right colours, in sync with audio | List of all movie files, each ticked |
| Audio | Music, crowd, SFX, speech present; no crackle, right pitch and mix, no drift | Recordings vs reference; no XMA errors in the log |
| Timing | Same frame rate and game speed as the console; a timed match half and timed animations match | Stopwatch runs against reference video |
| Input | All buttons, sticks, triggers, rumble; no added latency beyond the display's | Scripted input test, latency measurement |
| Gameplay | Physics, AI, tricks, Gamebreaker behave the same | Recorded-input runs where the game is deterministic; otherwise your play test |
| Saves | Create, save, load; a real console save loads here if you have one | Save round trip |
| Stability | No crash, no hang, no frame over 2x target in a 2-hour session; memory flat | Hitch log, committed-memory log |
| Content | Every team, venue, mode and unlock reachable | Checklist |

You sign off on the gate; I don't declare it.

## 7. Stage B: features (after the gate, each off by default, each with a switch)

Rough order, to be re-decided then:
1. Resolution scaling and the real monitor resolution, MSAA/anisotropic options.
2. Frame-rate options above the original, at real game speed (the hard one: the game's logic is probably tied to
   frame rate, as in Odyssey and Skate).
3. Ultrawide / 16:10 / 4:3 with correct HUD placement.
4. Keyboard and mouse, button-prompt swap, controller remapping.
5. Settings menu or launcher (Condemned 2 and FM4 are the models), shader pre-compile, skippable intros.
6. Later: upscalers, HDR, Steam Deck / Linux, texture packs.

Rule from Odyssey: a feature may not change the 1:1 result when it is off. The Stage A reference set is re-run
before any feature is merged.

## 8. Risks

- **SDK gaps**: an import or GPU feature the game needs may be missing in ReXGlue. Found in Phase 1's import audit;
  fixes go in our SDK fork as small, documented patches.
- **Title update**: if 1:1 means "with the title update", codegen must handle `default.xexp` (PGR4 and Condemned 2
  show how).
- **Timing coupling**: the game likely paces on vblank; wrong settings give a game that runs fast. Fixed by config
  in Stage A, real work only in Stage B.
- **GPU readback** paths (CPU reading render targets) cost frame time; needed for 1:1, optimised later.
- **Nothing here has been run yet.** Every statement about FIFA Street 3's internals is an expectation from similar
  EA games until Phase 1 confirms it.
