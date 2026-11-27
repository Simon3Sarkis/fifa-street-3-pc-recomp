// fifastreet3 - "a new version is available" check for the pre-game launcher.
//
// One request to GitHub's releases API ("latest" = the newest full release; pre-releases and drafts
// are skipped), compared with the VERSION file next to the program. Nothing is downloaded or
// installed: the launcher only shows the version and a link to its release page.

#pragma once

#include <memory>
#include <string>

#include <rex/ui/overlay/launcher_overlay.h>

namespace fs3 {

// False when the check must not run: update_check is off, a scripted run (input_script), or
// started by RecompLauncher (RECOMPLAUNCHER=1: it updates the games it starts itself).
bool UpdateCheckAllowed();

// True unless started by RecompLauncher (the launcher then shows no "Check for updates" row).
bool UpdateCheckOffered();

// Starts the check on its own thread (it never blocks the launcher; a few seconds' timeout) and
// fills `notice` when a newer full release exists. Without a VERSION file (a development build)
// nothing is checked.
void StartUpdateCheck(std::shared_ptr<rex::ui::LauncherDialog::UpdateNotice> notice);

// Version comparison, exposed for tests: "v0.1.0" > "0.0.1"; a "-suffix" is ignored.
bool IsNewerVersion(const std::string& candidate, const std::string& current);

}  // namespace fs3
