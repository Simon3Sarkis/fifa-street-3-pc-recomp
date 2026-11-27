# FIFA Street 3 PC port - barebone port

<a href="https://recomplauncher.com"><picture><source media="(prefers-color-scheme: dark)" srcset="https://recomplauncher.com/assets/press/launcher/badge-light.svg"><img alt="Get it on RecompLauncher" src="https://recomplauncher.com/assets/press/launcher/badge-dark.svg" height="48"></picture></a>

A static recompilation of the Xbox 360 version of FIFA Street 3 (NTSC, title 4541083B) to native
Windows and Linux / Steam Deck builds, with the [ReXGlue SDK](https://github.com/Simon3Sarkis/fifa-street-3-rexglue-sdk).

**This is a barebone port, released for testing.** The goal right now is to play exactly like the
Xbox 360 version (1:1) at a stable 60 FPS. It will improve over time. Please report anything that
doesn't match the Xbox 360 version: crashes, picture or sound differences, slowdowns. **Feature
requests come later**, once the port plays 1:1.

**No game files are in this repository or its releases.** You need your own copy of the game; everything
derived from it (`game/`, `generated/`, `work/`, `out/`) stays local.

## Playing (release 0.0.1)

1. Download the [latest release](https://github.com/Simon3Sarkis/fifa-street-3-pc-recomp/releases):
   `windows-x64.zip` for Windows, `linux-x86_64.tar.gz` for Linux and the Steam Deck. Unpack it anywhere.
2. Start `fifastreet3.exe` (Windows) or `./fifastreet3` (Linux / Steam Deck).
3. The first time, pick your FIFA Street 3 disc image (.iso) with **Select ISO...**: it's read as it
   is, nothing is extracted. Choose the display options and press **Play**.

Or with **RecompLauncher**: every release carries a `recomplauncher.json`. RecompLauncher asks for your
disc image (it checks it's FIFA Street 3), shows the display settings on the game's page and starts
the game straight in. Your saves and shader cache then live in the game's `recompdata` folder, which
the launcher backs up (saves only).

**Options** in the game: **F1**, **Esc**, or **Start + Back** together on the controller: display,
screen mode (windowed / borderless / exclusive), refresh rate, vsync, game resolution (720p / 1080p)
and render scale (1x-4x). The frame rate stays at 60, as on the console.

**Steam Deck:** the native Linux build runs at 60 FPS at 720p, with 99.9% of frames on time at a 15 W
TDP (a short hitch about 3 times a match in the heaviest scenes) and 99.6% at 10 W. Keep render scale at 1x. Measurements: [docs/STEAM_DECK.md in the SDK](https://github.com/Simon3Sarkis/fifa-street-3-rexglue-sdk/blob/fifastreet3/docs/STEAM_DECK.md).

Your settings (`fifastreet3.user.toml`), saves (`userdata/`) and logs (`logs/`) stay next to the
program; unpacking a new version over the old folder keeps them.

**Updating:** with RecompLauncher, updates are automatic. Otherwise the launcher screen checks
GitHub once at start for a newer full release and, if there is one, shows its version and a button
that opens the release page (nothing is downloaded or installed; turn it off with "Check for
updates" in the Display tab, or `update_check = false` in `fifastreet3.user.toml`). To update,
unzip the new version over the old folder: settings, saves, shader cache and logs are kept.

FIFA Street 3 is (c) Electronic Arts. This project is not affiliated with or endorsed by EA or Microsoft.

## Development

Plan and rules: [PLAN.md](PLAN.md). Progress, findings and the test loop: [NOTES.md](NOTES.md).

## Build (Windows)

Needs Visual Studio 2022 Build Tools (C++), LLVM/Clang 20+ in `C:\Program Files\LLVM`, CMake 3.25+,
Ninja (the VS Build Tools ones work), Python 3 with `capstone` and `pillow` for the tools.

```powershell
git clone --recursive https://github.com/Simon3Sarkis/fifa-street-3-pc-recomp
cd fifa-street-3-pc-recomp

# 1. Extract your ISO into game/
powershell -ExecutionPolicy Bypass -File tools\extract_iso.ps1 -Iso "<path>\FIFA Street 3.iso" -Out game

# 2. Configure (SDK only on a fresh clone), build the recompiler, generate the code
.\devenv.bat cmake --preset win-amd64-relwithdebinfo
.\devenv.bat cmake --build out/build/win-amd64-relwithdebinfo --target rexglue
third_party\rexglue-sdk\out\win-amd64\rexgluerd.exe --force codegen fifastreet3_manifest.toml

# 3. Reconfigure with the generated sources and build
.\devenv.bat cmake --preset win-amd64-relwithdebinfo
.\devenv.bat cmake --build out/build/win-amd64-relwithdebinfo

# 4. Run (copies DLLs and config next to the exe, links game/)
& .\tools\iterate.ps1 -SkipCodegen -SkipBuild -RunSeconds 60
```

## Steam Deck (native Linux)

The baseline target is the Steam Deck at 1280x720, 60 FPS, and the Deck runs the **native Linux
build** (no Proton): faster than the Windows build under Proton (59.9 vs 52.2 FPS in full matches,
measured interleaved). It's built on the PC in WSL (Ubuntu 24.04, Clang 20, preset
`linux-amd64-relwithdebinfo`), from the same sources. `config/fifastreet3.runtime.toml` carries the Deck
settings, each with what it fixes and how it was measured.

```bash
# In WSL, in the Linux build tree (sources synced from this checkout): build and package the release
cmake --build out/build/linux-amd64-relwithdebinfo
bash tools/make_release_linux.sh 0.0.1      # dist/fifastreet3-0.0.1-linux-x86_64.tar.gz
```
```bash
# From Git Bash on the PC: run a test script on the Deck over SSH (DECK=deck@<host> or tools/deck/deck.local)
RUN=run_native.sh DEST=fifastreet3-native SAVE=1 bash tools/deck/remote_run.sh m1 tests/input/perf_match.txt
# Benchmarks: interleaved A/B with paired differences
SCRIPT=tests/input/perf_match_frames.txt A_ENV="RUN=run_native.sh DEST=fifastreet3-native"   B_ENV="RUN=run_native.sh DEST=fifastreet3-native" bash tools/deck/bench_ab.sh ab 3 "" "--some_option=true"
```

The Windows build also still runs on the Deck under Proton (`tools/make_deck_package.ps1`,
`tools/deck/push.sh`); settings that only help there apply only under Wine / Proton.

Status and findings: NOTES.md entries 15 (Proton), 16 (native build), 17-18 (10 W, audio).

The SDK (`third_party/rexglue-sdk`) is the public repo `Simon3Sarkis/fifa-street-3-rexglue-sdk`, branch
`fifastreet3`: the rexglue-skate3 fork (v0.10.0) plus this port's changes, listed in NOTES.md.

## License

GPL-3.0 (see LICENSE), as the Skate1Recompiled files this project started from. The SDK keeps its own
license. FIFA Street 3 is a trademark of Electronic Arts; this is an unofficial, non-commercial project.
