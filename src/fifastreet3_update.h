// fifastreet3 - updates from the project's GitHub releases (the Updates tab of the launcher and of
// the in-game options menu).
//
// Check: one request to GitHub for the newest release (pre-releases count: every release so far
// is one), compared with the VERSION file next to the program. Nothing happens without the
// player: the Updates tab shows what is newer, and Update (Windows) downloads its Windows zip,
// checks it against the SHA-256 GitHub lists for it, unpacks it with Windows' own tar.exe and puts
// its files over the program's (files in use are renamed to .old first and removed at the next
// start). Settings (fifastreet3.user.toml), saves, the shader cache and the game files are not in
// a release, so they stay. Restart now starts the new version and ends this one. On Linux the tab
// links to the release page instead. Started by RecompLauncher, the tab only says that
// RecompLauncher updates the game.

#pragma once

#include <functional>
#include <memory>
#include <string>

#include <rex/ui/overlay/launcher_overlay.h>

namespace fs3 {

// The first thing at start: after an update's restart, wait for the old process to end, then
// remove the files the update renamed (.old) if they are free now.
void UpdaterStartup();

// The shared updater state the Updates tab shows (created on first use).
std::shared_ptr<rex::ui::LauncherDialog::UpdateNotice> Updater();

// True unless started by RecompLauncher (no "Check at start" row then).
bool UpdateCheckOffered();

// The check at start (update_check on, not scripted, not RecompLauncher), on its own thread.
void StartUpdateCheck();

// How the program ends itself for Restart now (the app's normal shutdown, settings saved).
void SetUpdaterQuit(std::function<void()> quit);

// Version comparison, exposed for tests: "v0.1.0" > "0.0.1"; a "-suffix" is ignored.
bool IsNewerVersion(const std::string& candidate, const std::string& current);

}  // namespace fs3
