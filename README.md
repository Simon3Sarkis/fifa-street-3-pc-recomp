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

The SDK (`third_party/rexglue-sdk`) is the private repo `fifa-street-3-rexglue-sdk`, branch
`fifastreet3`: the rexglue-skate3 fork (v0.10.0) plus this port's changes, listed in NOTES.md.

## License

GPL-3.0 (see LICENSE), as the Skate1Recompiled files this project started from. The SDK keeps its own
license. FIFA Street 3 is a trademark of Electronic Arts; this is an unofficial, non-commercial project.
