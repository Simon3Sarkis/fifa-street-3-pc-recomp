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

REXCVAR_DEFINE_BOOL(pause_in_options_menu, true, "FIFA Street 3",
                    "Hold the game while the in-game options menu is open (guest threads, guest "
                    "clock, vblank and audio stop; it continues where it was on close). The "
                    "console has no such menu, so this changes nothing while the menu is closed.");

REXCVAR_DEFINE_BOOL(options_menu_open, false, "FIFA Street 3",
                    "Testing: set to true (input-script `set`) to open the in-game options menu "
                    "without a button press; it follows the menu's state.")
    .lifecycle(rex::cvar::Lifecycle::kHotReload);

namespace fs3 {

namespace {

// The game executable this port was recompiled from (default.xex on the NTSC disc, title
// 4541083B): its size and FNV-1a 64 hash. It is the only version tested; other regions and
// versions are allowed, with a notice, since their code may differ from the recompiled one.
constexpr uint64_t kXexSize = 2613248;
constexpr uint64_t kXexFnv1a64 = 0x65248770d412c24aull;
constexpr uint32_t kTitleId = 0x4541083B;

uint64_t Fnv1a64(std::span<const uint8_t> data) {
  uint64_t h = 0xcbf29ce484222325ull;
  for (uint8_t b : data) {
    h = (h ^ b) * 0x100000001b3ull;
  }
  return h;
}

uint32_t ReadBe32(const std::vector<uint8_t>& d, size_t at) {
  return at + 4 <= d.size() ? uint32_t(d[at]) << 24 | uint32_t(d[at + 1]) << 16 |
                                  uint32_t(d[at + 2]) << 8 | uint32_t(d[at + 3])
                            : 0;
}

// Title ID and version from the XEX2 execution info header (key 0x00040006); 0 if absent.
void ReadExecutionInfo(const std::vector<uint8_t>& xex, uint32_t& title_id, uint32_t& version) {
  title_id = version = 0;
  const uint32_t count = ReadBe32(xex, 0x14);
  for (uint32_t i = 0; i < count && i < 64; ++i) {
    const size_t entry = 0x18 + size_t(i) * 8;
    if (ReadBe32(xex, entry) == 0x00040006) {
      const size_t info = ReadBe32(xex, entry + 4);
      version = ReadBe32(xex, info + 4);
      title_id = ReadBe32(xex, info + 12);
      return;
    }
  }
}

rex::ui::LauncherDialog::GameCheck CheckXex(const std::vector<uint8_t>& xex) {
  if (xex.size() < 4 || std::memcmp(xex.data(), "XEX2", 4) != 0) {
    return {false, "This has no Xbox 360 game executable (default.xex)."};
  }
  const uint64_t hash = Fnv1a64(xex);
  if (xex.size() == kXexSize && hash == kXexFnv1a64) {
    return {true, "Ready: FIFA Street 3 (NTSC), the version this port was made and tested with."};
  }
  uint32_t title_id, version;
  ReadExecutionInfo(xex, title_id, version);
  // Logged so a report of how it ran says which version it was.
  REXLOG_WARN("Game check: untested version: title {:08X}, version {:08X}, default.xex {} bytes, "
              "FNV-1a 64 {:016x}",
              title_id, version, xex.size(), hash);
  const std::string id = fmt::format("title {:08X}, version {:08X}", title_id, version);
  if (title_id != kTitleId) {
    return {true,
            fmt::format("Untested: this isn't the FIFA Street 3 this port was tested with ({}). "
                        "Only the NTSC version is tested; another one may not start or may not "
                        "work correctly. You can still play it.",
                        id),
            true};
  }
  return {true,
          fmt::format("Untested version of FIFA Street 3 ({}). Only the NTSC version is tested; "
                      "this one may not start or may not work correctly. You can still play it.",
                      id),
          true};
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
    return {false,
            "The game can't be found. Select your FIFA Street 3 disc image (.iso) or extracted "
            "game folder."};
  }
  std::vector<uint8_t> xex;
  if (std::filesystem::is_directory(path, ec)) {
    const auto xex_path = path / "default.xex";
    FILE* f = rex::filesystem::OpenFile(xex_path, "rb");
    if (!f) {
      return {false,
              "This folder has no default.xex. Select the folder the game was extracted to, or "
              "the disc image."};
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
