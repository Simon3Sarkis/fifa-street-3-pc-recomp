# FIFA Street 3 PC port - barebone port

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
   `windows-x64.zip` for Windows, `linux-x64.tar.gz` for Linux and the Steam Deck. Unpack it anywhere.
2. Start `fifastreet3.exe` (Windows) or `./fifastreet3` (Linux / Steam Deck).
3. The first time, pick your FIFA Street 3 disc image (.iso) with **Select ISO...**: it's read as it
   is, nothing is extracted. Choose the display options and press **Play**.

Or with **RecompLauncher**: the release folder holds a `recomplauncher.json`; RecompLauncher asks for
the game and starts it.

**Options** in the game: **F1**, **Esc**, or **Start + Back** together on the controller: display,
screen mode (windowed / borderless / exclusive), refresh rate, vsync, and resolution: 1280 x 720,
1920 x 1080, 2560 x 1440 or 3840 x 2160 (the game's own modes are 720p and 1080p; 1440p and 4K draw
them larger). The frame rate stays at 60, as on the console.

**Steam Deck:** the native Linux build runs at 60 FPS at 720p, with 99.9% of frames on time at a 15 W
TDP (a short hitch about 3 times a match in the heaviest scenes) and 99.6% at 10 W. Keep the resolution at 1280 x 720. Measurements: [docs/STEAM_DECK.md in the SDK](https://github.com/Simon3Sarkis/fifa-street-3-rexglue-sdk/blob/fifastreet3/docs/STEAM_DECK.md).

Your settings (`fifastreet3.user.toml`), saves (`userdata/`) and logs (`logs/`) stay next to the
program; unpacking a new version over the old folder keeps them.

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

## Steam Deck (Proton)

The baseline target is the Steam Deck at 1280x720, 60 FPS. The Windows build runs there under Proton
Experimental; `config/fifastreet3.runtime.toml` carries the Deck settings (each with what it fixes and
how it was measured), and settings that only help under Proton apply only there.

```powershell
# On the PC: portable folder with the exe, DLLs, VC++ runtime and config
& .\tools\make_deck_package.ps1
```
```bash
# From Git Bash: copy it (and once, the game files) to the Deck over SSH, then run a test script there
bash tools/deck/push.sh --with-game
SAVE=1 bash tools/deck/remote_run.sh m1 tests/input/perf_match.txt
# Benchmarks: repeated runs, or interleaved A/B with paired differences
SCRIPT=tests/input/perf_match.txt PHASE=match bash tools/deck/bench.sh m 5
bash tools/deck/bench_ab.sh ab 4 "" "--some_option=true"
```

Status and what limited it: NOTES.md entry 15.

The SDK (`third_party/rexglue-sdk`) is the public repo `Simon3Sarkis/fifa-street-3-rexglue-sdk`, branch
`fifastreet3`: the rexglue-skate3 fork (v0.10.0) plus this port's changes, listed in NOTES.md.

## License

GPL-3.0 (see LICENSE), as the Skate1Recompiled files this project started from. The SDK keeps its own
license. FIFA Street 3 is a trademark of Electronic Arts; this is an unofficial, non-commercial project.
