// fifastreet3 - launcher: where the game is, the user's settings file, and the ISO check.
//
// The player picks their FIFA Street 3 disc image once; it is read in place (no extraction). The
// choice and the display settings go to fifastreet3.user.toml next to the exe. Releases ship
// fifastreet3.toml only, so unpacking a new version over an old one keeps the user's file.

#include "fifastreet3_launcher.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <span>
#include <vector>

#include <fmt/format.h>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/filesystem/devices/disc_image_device.h>
#include <rex/filesystem/entry.h>
#include <rex/filesystem/file.h>
#include <rex/logging.h>
#include <rex/system/xtypes.h>

REXCVAR_DEFINE_BOOL(launcher, true, "FIFA Street 3",
                    "Show the launcher (game disc image and display settings) before the game "
                    "starts. Scripted runs (input_script) skip it.");

namespace fs3 {

namespace {

// The game executable this port was recompiled from (default.xex on the disc): its size and
// FNV-1a 64 hash. Another region or version would not match the recompiled code.
constexpr uint64_t kXexSize = 2613248;
constexpr uint64_t kXexFnv1a64 = 0x65248770d412c24aull;

uint64_t Fnv1a64(std::span<const uint8_t> data) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (uint8_t b : data) {
    h = (h ^ b) * 0x100000001b3ull;
  }
  return h;
}

rex::ui::LauncherDialog::GameCheck CheckXex(const std::vector<uint8_t>& xex) {
  if (xex.size() < 4 || std::memcmp(xex.data(), "XEX2", 4) != 0) {
    return {false, "This disc image has no Xbox 360 game executable."};
  }
  if (xex.size() != kXexSize || Fnv1a64(xex) != kXexFnv1a64) {
    return {false,
            "This is a different game, or another version of FIFA Street 3 than the one this "
            "port was made from."};
  }
  return {true, "Ready: FIFA Street 3, the version this port was made from."};
}

}  // namespace

std::filesystem::path UserSettingsPath() {
  return rex::filesystem::GetExecutableFolder() / "fifastreet3.user.toml";
}

bool IsScriptedRun() {
  return !rex::cvar::Query<std::string>("input_script").empty();
}

std::filesystem::path FindGame(const std::filesystem::path& exe_dir) {
  // 1. The command line or the user's settings file.
  std::filesystem::path configured = rex::cvar::Query<std::string>("game_data_root");
  if (!configured.empty()) {
    return configured.is_relative() ? exe_dir / configured : configured;
  }
  std::error_code ec;
  // 2. An extracted game folder next to the exe.
  if (std::filesystem::is_regular_file(exe_dir / "game" / "default.xex", ec)) {
    return exe_dir / "game";
  }
  // 3. A disc image next to the exe (the first one in name order).
  std::vector<std::filesystem::path> isos;
  for (const auto& entry : std::filesystem::directory_iterator(exe_dir, ec)) {
    auto ext = entry.path().extension().string();
    for (auto& c : ext) c = char(std::tolower(static_cast<unsigned char>(c)));
    if (ext == ".iso" && entry.is_regular_file(ec)) {
      isos.push_back(entry.path());
    }
  }
  std::sort(isos.begin(), isos.end());
  return isos.empty() ? std::filesystem::path() : isos.front();
}

rex::ui::LauncherDialog::GameCheck CheckGame(const std::filesystem::path& path) {
  std::error_code ec;
  if (path.empty() || !std::filesystem::exists(path, ec)) {
    return {false, "The game file can't be found. Select your FIFA Street 3 disc image (.iso)."};
  }
  std::vector<uint8_t> xex;
  if (std::filesystem::is_directory(path, ec)) {
    const auto xex_path = path / "default.xex";
    FILE* f = rex::filesystem::OpenFile(xex_path, "rb");
    if (!f) {
      return {false, "This folder has no default.xex. Select your FIFA Street 3 disc image."};
    }
    xex.resize(std::filesystem::file_size(xex_path, ec));
    xex.resize(fread(xex.data(), 1, xex.size(), f));
    fclose(f);
    return CheckXex(xex);
  }
  rex::filesystem::DiscImageDevice device("\\Device\\LauncherCheck", path);
  if (!device.Initialize()) {
    return {false, "This file isn't an Xbox 360 game disc image."};
  }
  rex::filesystem::Entry* entry = device.ResolvePath("default.xex");
  if (!entry) {
    return {false, "This disc image has no Xbox 360 game executable."};
  }
  rex::filesystem::File* file = nullptr;
  if (XFAILED(entry->Open(0x80000000 /* GENERIC_READ */, &file)) || !file) {
    return {false, "This disc image couldn't be read."};
  }
  xex.resize(entry->size());
  size_t read = 0;
  const bool ok = !XFAILED(file->ReadSync(std::span<uint8_t>(xex), 0, &read)) && read == xex.size();
  file->Destroy();
  if (!ok) {
    return {false, "This disc image couldn't be read."};
  }
  return CheckXex(xex);
}

}  // namespace fs3
