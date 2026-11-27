// fifastreet3 - launcher: where the game is, the user's settings file, and the ISO check.

#pragma once

#include <filesystem>

#include <rex/ui/overlay/launcher_overlay.h>

namespace fs3 {

// fifastreet3.user.toml next to the exe: the player's choices, never shipped in a release.
std::filesystem::path UserSettingsPath();

// Runs driven by an input script (tests, benchmarks) use the shipped settings only and no launcher.
bool IsScriptedRun();

// The game location: game_data_root (command line or the user's settings), else an extracted
// game folder next to the exe, else a disc image next to the exe. Empty if none.
std::filesystem::path FindGame(const std::filesystem::path& exe_dir);

// Whether a disc image or extracted folder holds an Xbox 360 game. Any version is allowed; one
// other than the NTSC version this port was made from gets an "untested" warning.
rex::ui::LauncherDialog::GameCheck CheckGame(const std::filesystem::path& path);

}  // namespace fs3
