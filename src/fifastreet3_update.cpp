// fifastreet3 - "a new version is available" check. See fifastreet3_update.h.

#include "fifastreet3_update.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <thread>
#include <vector>

#include <rex/cvar.h>
#include <rex/filesystem.h>
#include <rex/logging.h>

#include "fifastreet3_launcher.h"

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <winhttp.h>
#endif

REXCVAR_DEFINE_BOOL(update_check, true, "FIFA Street 3",
                    "At start, the launcher asks GitHub once whether a newer full release is out "
                    "and shows a link to it (nothing is downloaded). Off when started by "
                    "RecompLauncher or in scripted runs.");
REXCVAR_DEFINE_STRING(update_check_test_version, "", "FIFA Street 3",
                      "Testing: use this as the program's version instead of the VERSION file");
REXCVAR_DEFINE_BOOL(update_check_test_prereleases, false, "FIFA Street 3",
                    "Testing: also count pre-releases (the newest release of any kind)");

namespace fs3 {

namespace {

constexpr const char* kRepo = "Simon3Sarkis/fifa-street-3-pc-recomp";
constexpr const char* kApiHost = "api.github.com";
constexpr const char* kApiPath = "/repos/Simon3Sarkis/fifa-street-3-pc-recomp/releases/latest";
// update_check_test_prereleases: the newest release of any kind (an array; its first tag_name).
constexpr const char* kApiPathAny =
    "/repos/Simon3Sarkis/fifa-street-3-pc-recomp/releases?per_page=1";
// Generic: no user or machine details in the request.
constexpr const char* kUserAgent = "fifastreet3-update-check";
constexpr int kTimeoutMs = 5000;
constexpr size_t kMaxBody = 512 * 1024;

const char* ApiPath() {
  return REXCVAR_GET(update_check_test_prereleases) ? kApiPathAny : kApiPath;
}

bool StartedByRecompLauncher() {
  const char* v = std::getenv("RECOMPLAUNCHER");
  return v && *v && std::string(v) != "0";
}

std::string ReadVersionFile() {
  std::ifstream in(rex::filesystem::GetExecutableFolder() / "VERSION");
  std::string version;
  std::getline(in, version);
  while (!version.empty() && std::isspace(static_cast<unsigned char>(version.back()))) {
    version.pop_back();
  }
  return version;
}

std::vector<int> VersionParts(std::string v) {
  if (!v.empty() && (v[0] == 'v' || v[0] == 'V')) {
    v.erase(0, 1);
  }
  v = v.substr(0, v.find_first_of("-+ "));
  std::vector<int> parts;
  size_t pos = 0;
  while (pos <= v.size()) {
    const size_t dot = v.find('.', pos);
    const std::string part = v.substr(pos, dot == std::string::npos ? std::string::npos : dot - pos);
    int n = 0;
    for (char c : part) {
      if (!std::isdigit(static_cast<unsigned char>(c))) {
        return {};  // not a plain version: never "newer"
      }
      n = n * 10 + (c - '0');
    }
    parts.push_back(n);
    if (dot == std::string::npos) {
      break;
    }
    pos = dot + 1;
  }
  return parts;
}

// The "tag_name" of the release JSON, if it looks like a tag (letters, digits, . - _ only).
std::optional<std::string> TagName(const std::string& json) {
  const size_t key = json.find("\"tag_name\"");
  if (key == std::string::npos) {
    return std::nullopt;
  }
  const size_t open = json.find('"', json.find(':', key) + 1);
  if (open == std::string::npos) {
    return std::nullopt;
  }
  const size_t close = json.find('"', open + 1);
  if (close == std::string::npos || close - open - 1 > 64) {
    return std::nullopt;
  }
  std::string tag = json.substr(open + 1, close - open - 1);
  for (char c : tag) {
    if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-' && c != '_') {
      return std::nullopt;
    }
  }
  return tag.empty() ? std::nullopt : std::optional<std::string>(tag);
}

#if defined(_WIN32)
std::optional<std::string> FetchLatestRelease() {
  std::optional<std::string> body;
  const std::wstring agent(kUserAgent, kUserAgent + std::char_traits<char>::length(kUserAgent));
  const std::wstring host(kApiHost, kApiHost + std::char_traits<char>::length(kApiHost));
  const char* api_path = ApiPath();
  const std::wstring path(api_path, api_path + std::char_traits<char>::length(api_path));
  HINTERNET session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) {
    return body;
  }
  WinHttpSetTimeouts(session, kTimeoutMs, kTimeoutMs, kTimeoutMs, kTimeoutMs);
  HINTERNET connect = WinHttpConnect(session, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
  HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", path.c_str(), nullptr,
                                                   WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                   WINHTTP_FLAG_SECURE)
                              : nullptr;
  if (request &&
      WinHttpSendRequest(request, L"Accept: application/vnd.github+json\r\n", DWORD(-1),
                         WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
      WinHttpReceiveResponse(request, nullptr)) {
    DWORD status = 0;
    DWORD size = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    if (status == 200) {
      std::string data;
      DWORD available = 0;
      while (WinHttpQueryDataAvailable(request, &available) && available > 0 &&
             data.size() < kMaxBody) {
        std::string chunk(available, '\0');
        DWORD read = 0;
        if (!WinHttpReadData(request, chunk.data(), available, &read) || read == 0) {
          break;
        }
        data.append(chunk.data(), read);
      }
      body = std::move(data);
    } else {
      // 404: no full release yet (only pre-releases).
      REXLOG_INFO("Update check: GitHub answered {}", status);
    }
  }
  if (request) WinHttpCloseHandle(request);
  if (connect) WinHttpCloseHandle(connect);
  WinHttpCloseHandle(session);
  return body;
}
#else
std::optional<std::string> FetchLatestRelease() {
  // curl is on SteamOS and the usual desktops; a fixed command line, nothing from the outside.
  const std::string command = std::string("curl -fsSL --max-time ") +
                              std::to_string(kTimeoutMs / 1000) + " -A " + kUserAgent +
                              " -H 'Accept: application/vnd.github+json' 'https://" + kApiHost +
                              ApiPath() + "' 2>/dev/null";
  FILE* pipe = popen(command.c_str(), "r");
  if (!pipe) {
    return std::nullopt;
  }
  std::string data;
  char buffer[4096];
  size_t n;
  while ((n = fread(buffer, 1, sizeof(buffer), pipe)) > 0 && data.size() < kMaxBody) {
    data.append(buffer, n);
  }
  const int rc = pclose(pipe);
  if (rc != 0 || data.empty()) {
    return std::nullopt;
  }
  return data;
}
#endif

}  // namespace

bool UpdateCheckOffered() {
  return !StartedByRecompLauncher();
}

bool UpdateCheckAllowed() {
  return REXCVAR_GET(update_check) && !IsScriptedRun() && !StartedByRecompLauncher();
}

bool IsNewerVersion(const std::string& candidate, const std::string& current) {
  std::vector<int> a = VersionParts(candidate), b = VersionParts(current);
  if (a.empty() || b.empty()) {
    return false;
  }
  a.resize(std::max(a.size(), b.size()), 0);
  b.resize(a.size(), 0);
  return a > b;
}

void StartUpdateCheck(std::shared_ptr<rex::ui::LauncherDialog::UpdateNotice> notice) {
  if (!notice || !UpdateCheckAllowed()) {
    return;
  }
  const std::string test_version = REXCVAR_GET(update_check_test_version);
  const std::string current = test_version.empty() ? ReadVersionFile() : test_version;
  if (current.empty()) {
    REXLOG_INFO("Update check: no VERSION file next to the program (development build), skipped");
    return;
  }
  // Detached: the notice is shared, so the thread may outlive the launcher.
  std::thread([notice, current]() {
    const std::optional<std::string> body = FetchLatestRelease();
    const std::optional<std::string> tag = body ? TagName(*body) : std::nullopt;
    if (!tag) {
      REXLOG_INFO("Update check: no full release found (offline, or none yet)");
      return;
    }
    if (!IsNewerVersion(*tag, current)) {
      REXLOG_INFO("Update check: {} is the latest full release, this is {}", *tag, current);
      return;
    }
    std::string version = *tag;
    if (!version.empty() && (version[0] == 'v' || version[0] == 'V')) {
      version.erase(0, 1);
    }
    REXLOG_INFO("Update check: version {} is available (this is {})", version, current);
    std::lock_guard lock(notice->mutex);
    notice->version = version;
    notice->url = std::string("https://github.com/") + kRepo + "/releases/tag/" + *tag;
  }).detach();
}

}  // namespace fs3
