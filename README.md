# FIFA Street 3 PC port (private)

A static recompilation of the Xbox 360 version of FIFA Street 3 (NTSC, title 4541083B) to a native
Windows exe with ReXGlue. Stage A is a 1:1 port with no changes; features come after. Plan and rules:
[PLAN.md](PLAN.md). Progress, findings and the test loop: [NOTES.md](NOTES.md).

**No game files are in this repository.** You need your own disc image; everything derived from it
(`game/`, `generated/`, `work/`, `out/`) stays local.

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

The SDK (`third_party/rexglue-sdk`) is the private repo `fifa-street-3-rexglue-sdk`, branch
`fifastreet3`: the rexglue-skate3 fork (v0.10.0) plus this port's changes, listed in NOTES.md.

## License

GPL-3.0 (see LICENSE), as the Skate1Recompiled files this project started from. The SDK keeps its own
license. FIFA Street 3 is a trademark of Electronic Arts; this is an unofficial, non-commercial project.
