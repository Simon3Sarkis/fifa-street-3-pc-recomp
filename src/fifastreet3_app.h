// fifastreet3 - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <memory>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>
#include <rex/input/input_system.h>
#include <rex/rex_app.h>
#include <rex/runtime.h>
#include <rex/ui/keybinds.h>
#include <rex/ui/window.h>
#include <rex/ui/windowed_app_context.h>

#include "fifastreet3_launcher.h"

REXCVAR_DECLARE(bool, launcher);

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
    options.subtitle = "PC port  |  0.0.1 pre-release, for testing";
    options.game_path = defaults.game_data_root;
    options.settings_path = fs3::UserSettingsPath();
    options.picker_hint =
        "Select your FIFA Street 3 disc image (.iso). It is read as it is, nothing is extracted.";
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
  }

 private:
  // UI thread. Only once the game runs (the launcher has its own buttons).
  void ToggleOptions() {
    if (launcher_ || !runtime()) {
      return;
    }
    if (options_) {
      CloseOptions();
      return;
    }
    rex::ui::LauncherDialog::Options options;
    options.title = "Options";
    options.subtitle = "FIFA Street 3  |  0.0.1 barebone port";
    options.settings_path = fs3::UserSettingsPath();
    options.in_game = true;
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
    REXLOG_INFO("Options menu opened");
  }

  void CloseOptions() {
    if (options_) {
      REXLOG_INFO("Options menu closed");
    }
    options_.reset();
    SetGuestInputBlocked(false);
  }

  std::unique_ptr<rex::ui::LauncherDialog> launcher_;
  std::unique_ptr<rex::ui::LauncherDialog> options_;
};
