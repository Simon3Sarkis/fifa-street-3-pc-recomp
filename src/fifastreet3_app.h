// fifastreet3 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <chrono>
#include <cstdlib>
#include <memory>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/input/input_system.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/presenter.h>
#include <rex/system/interfaces/graphics.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>

#include "fifastreet3_launcher.h"
#include "fifastreet3_update.h"

REXCVAR_DECLARE(bool, launcher);
REXCVAR_DECLARE(bool, pause_in_options_menu);
REXCVAR_DECLARE(bool, options_menu_open);

class FifaStreet3App : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<FifaStreet3App>(new FifaStreet3App(ctx, "fifastreet3",
        fifastreet3_PPCImageConfig));
  }

  // Portable layout: the game location and a relative game_data_root resolve next to the exe
  // instead of the working directory, and a portable.txt beside the exe keeps saves and caches in
  // the exe folder too.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    const auto exe_dir = rex::filesystem::GetExecutableFolder();
    // The player's settings (fifastreet3.user.toml) sit on top of the shipped fifastreet3.toml; the
    // SDK saves there too. Scripted runs (tests, benchmarks) leave them out so they stay
    // reproducible. The SDK would load this file only after this hook, but the game location is
    // decided here, so load it now (command-line values still win).
    if (!fs3::IsScriptedRun()) {
      paths.config_path = fs3::UserSettingsPath();
      rex::cvar::LoadConfig(paths.config_path);
    }
    paths.game_data_root = fs3::FindGame(exe_dir);
    // portable.txt sets the default only: an explicit user_data_root / cache_root (test runs pass
    // per-run folders) wins. Before 2026-11-23 it overrode them, so Deck test runs ignored -Save.
    if (std::filesystem::exists(exe_dir / "portable.txt")) {
      if (!rex::cvar::HasNonDefaultValue("user_data_root")) {
        paths.user_data_root = exe_dir / "userdata";
      }
      if (!rex::cvar::HasNonDefaultValue("cache_root")) {
        paths.cache_root = paths.user_data_root / "cache";
      }
    }
    // Started by RecompLauncher: its data folder ({install}/recompdata, RECOMPLAUNCHER_DATA) holds the
    // saves, and the shader cache in its cache folder, which the launcher leaves out of save backups.
    // Only when the launcher sets it, and an explicit user_data_root / cache_root still wins.
    if (const char* data = std::getenv("RECOMPLAUNCHER_DATA"); data && *data) {
      const std::filesystem::path data_dir = std::filesystem::u8path(data);
      if (!rex::cvar::HasNonDefaultValue("user_data_root")) {
        paths.user_data_root = data_dir;
      }
      if (!rex::cvar::HasNonDefaultValue("cache_root")) {
        paths.cache_root = data_dir / "cache";
      }
    }
  }

  // The launcher: shown before the game unless turned off or the run is scripted. Without a valid
  // game it is shown anyway, so the player can pick their disc image.
  std::optional<rex::PathConfig> OnFinalizePaths(
      const rex::PathConfig& defaults, std::function<void(rex::PathConfig)> resume) override {
    const bool game_ok = fs3::CheckGame(defaults.game_data_root).ok;
    if (fs3::IsScriptedRun() || (game_ok && !REXCVAR_GET(launcher)) || !imgui_drawer()) {
      return defaults;
    }
    rex::ui::LauncherDialog::Options options;
    options.title = "FIFA Street 3";
    options.subtitle = "PC port  |  0.0.2 pre-release, for testing";
    options.game_path = defaults.game_data_root;
    options.settings_path = fs3::UserSettingsPath();
    options.display_font_path = rex::filesystem::GetExecutableFolder() / "fonts" / "Anton-Regular.ttf";
    options.picker_hint =
        "Select your FIFA Street 3 disc image (.iso), read as it is, or an extracted game folder.";
    // "A new version is available": one request to GitHub on its own thread, never blocking.
    options.update_notice = std::make_shared<rex::ui::LauncherDialog::UpdateNotice>();
    if (fs3::UpdateCheckOffered()) {
      options.update_check_cvar = "update_check";
    }
    fs3::StartUpdateCheck(options.update_notice);
    auto play = [this, defaults, resume](const std::filesystem::path& game) {
      rex::PathConfig paths = defaults;
      paths.game_data_root = game;
      // After the launcher's last frame: the runtime starts outside the UI drawer.
      app_context().CallInUIThreadDeferred([this, paths, resume]() {
        launcher_.reset();
        resume(paths);
      });
    };
    auto quit = [this]() {
      app_context().CallInUIThreadDeferred([this]() {
        launcher_.reset();
        app_context().QuitFromUIThread();
      });
    };
    launcher_ = std::make_unique<rex::ui::LauncherDialog>(imgui_drawer(), window(),
                                                          std::move(options), fs3::CheckGame,
                                                          std::move(play), std::move(quit));
    return std::nullopt;
  }

  // In-game options: F1, Esc, or the controller chord (menu_chord, Start + Back in the config).
  void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {
    (void)drawer;
    rex::ui::RegisterBind("bind_options_menu", "F1", "Open or close the options menu",
                          [this] { ToggleOptions(); });
    rex::ui::RegisterBind("bind_options_menu_escape", "Escape", "Open or close the options menu",
                          [this] { ToggleOptions(); });
  }

  void OnPostSetup() override {
    if (auto* input = static_cast<rex::input::InputSystem*>(runtime()->input_system())) {
      // Fires on the guest thread that polls input.
      input->SetMenuChordCallback(
          [this]() { app_context().CallInUIThread([this]() { ToggleOptions(); }); });
    }
    // Scripted tests open the menu with `set options_menu_open true`: a Start + Back press would
    // also reach the game (Start pauses a match).
    rex::cvar::RegisterChangeCallback("options_menu_open", [this](std::string_view,
                                                                  std::string_view) {
      app_context().CallInUIThread([this]() {
        if (REXCVAR_GET(options_menu_open) != (options_ != nullptr)) {
          last_toggle_ = {};
          ToggleOptions();
        }
      });
    });
  }

 private:
  // UI thread. Only once the game runs (the launcher has its own buttons).
  void ToggleOptions() {
    if (launcher_ || !runtime()) {
      return;
    }
    // One press can reach here twice (the menu's own Start + Back and the input system's chord,
    // or a held key): ignore toggles right after the last one.
    const auto now = std::chrono::steady_clock::now();
    if (now - last_toggle_ < std::chrono::milliseconds(400)) {
      return;
    }
    last_toggle_ = now;
    if (options_) {
      CloseOptions();
      return;
    }
    const bool pause = REXCVAR_GET(pause_in_options_menu);
    rex::ui::LauncherDialog::Options options;
    options.title = "Options";
    options.subtitle = "FIFA Street 3  |  0.0.2 barebone port";
    options.settings_path = fs3::UserSettingsPath();
    options.in_game = true;
    options.display_font_path = rex::filesystem::GetExecutableFolder() / "fonts" / "Anton-Regular.ttf";
    if (fs3::UpdateCheckOffered()) {
      options.update_check_cvar = "update_check";
    }
    options.in_game_note = pause ? "The game is paused while this menu is open."
                                 : "The game keeps running while this menu is open.";
    auto back = [this](const std::filesystem::path&) {
      app_context().CallInUIThreadDeferred([this]() { CloseOptions(); });
    };
    auto quit = [this]() {
      app_context().CallInUIThreadDeferred([this]() {
        CloseOptions();
        if (window()) {
          window()->RequestClose();
        }
      });
    };
    options_ = std::make_unique<rex::ui::LauncherDialog>(imgui_drawer(), window(),
                                                         std::move(options), nullptr,
                                                         std::move(back), std::move(quit));
    SetGuestInputBlocked(true);
    if (pause) {
      runtime()->SetGuestPaused(true);
    }
    REXCVAR_SET(options_menu_open, true);
    REXLOG_INFO("Options menu opened (guest frames so far: {})", GuestFrameCount());
  }

  // Frames the game has presented (counted while the FPS counter or debug overlay is on, else 0).
  uint64_t GuestFrameCount() const {
    auto* graphics = runtime() ? runtime()->graphics_system() : nullptr;
    auto* presenter = graphics ? graphics->presenter() : nullptr;
    return presenter ? presenter->GetGuestFrameStats().frame_count : 0;
  }

  void CloseOptions() {
    if (options_) {
      REXLOG_INFO("Options menu closed (guest frames so far: {})", GuestFrameCount());
      last_toggle_ = std::chrono::steady_clock::now();
    }
    options_.reset();
    REXCVAR_SET(options_menu_open, false);
    // The button that closed the menu (A, B, Start + Back) must not reach the game.
    if (auto* input =
            runtime() ? static_cast<rex::input::InputSystem*>(runtime()->input_system()) : nullptr) {
      input->IgnoreButtonsHeldNow();
    }
    if (runtime()) {
      runtime()->SetGuestPaused(false);
    }
    SetGuestInputBlocked(false);
  }

  std::unique_ptr<rex::ui::LauncherDialog> launcher_;
  std::unique_ptr<rex::ui::LauncherDialog> options_;
  std::chrono::steady_clock::time_point last_toggle_{};
};
