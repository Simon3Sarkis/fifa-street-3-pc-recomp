// fifastreet3 - updates from GitHub releases. See fifastreet3_update.h.

#include "fifastreet3_update.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <mutex>
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
#include <bcrypt.h>
#include <winhttp.h>
#pragma comment(lib, "bcrypt.lib")
#endif

REXCVAR_DEFINE_BOOL(update_check, true, "FIFA Street 3",
                    "At start, ask GitHub once whether a newer version is out (shown in the Updates "
                    "tab; nothing is downloaded until you press Update). Off when started by "
                    "RecompLauncher or in scripted runs.");
REXCVAR_DEFINE_STRING(update_check_test_version, "", "FIFA Street 3",
                      "Testing: use this as the program's version instead of the VERSION file");
REXCVAR_DEFINE_BOOL(update_test_auto_install, false, "FIFA Street 3",
                    "Testing: when the check finds a newer release, install it and restart into it "
                    "without a button press");

namespace fs3 {

namespace {

using Notice = rex::ui::LauncherDialog::UpdateNotice;
using State = Notice::State;

constexpr const char* kRepo = "Simon3Sarkis/fifa-street-3-pc-recomp";
// The newest releases of any kind, newest first (every release so far is a pre-release).
constexpr const char* kApiPath = "/repos/Simon3Sarkis/fifa-street-3-pc-recomp/releases?per_page=5";
constexpr const char* kWindowsAsset = "-windows-x64.zip";
// Generic: no user or machine details in the requests.
constexpr const char* kUserAgent = "fifastreet3-updater";
constexpr int kTimeoutMs = 8000;
constexpr size_t kMaxBody = 1024 * 1024;

std::mutex g_state_mutex;  // guards the globals below
std::shared_ptr<Notice> g_notice;
std::function<void()> g_quit;
std::string g_zip_url, g_zip_sha256;
bool g_busy = false;

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

std::string CurrentVersion() {
  const std::string test_version = REXCVAR_GET(update_check_test_version);
  return test_version.empty() ? ReadVersionFile() : test_version;
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

// The string value of "key" at or after `from` in the JSON (up to `limit`), if it is a plain one.
std::optional<std::string> JsonString(const std::string& json, const std::string& key, size_t from,
                                      size_t limit, size_t* at = nullptr) {
  const size_t k = json.find("\"" + key + "\"", from);
  if (k == std::string::npos || k >= limit) {
    return std::nullopt;
  }
  const size_t colon = json.find(':', k);
  const size_t open = colon == std::string::npos ? colon : json.find_first_not_of(" \t\r\n", colon + 1);
  if (open == std::string::npos || json[open] != '"') {
    return std::nullopt;
  }
  const size_t close = json.find('"', open + 1);
  if (close == std::string::npos || close - open - 1 > 2048) {
    return std::nullopt;
  }
  if (at) {
    *at = close;
  }
  return json.substr(open + 1, close - open - 1);
}

bool PlainTag(const std::string& tag) {
  return !tag.empty() && tag.size() <= 64 &&
         std::all_of(tag.begin(), tag.end(), [](char c) {
           return std::isalnum(static_cast<unsigned char>(c)) || c == '.' || c == '-' || c == '_';
         });
}

void SetState(State state, const std::string& message = {}) {
  std::shared_ptr<Notice> notice = Updater();
  std::lock_guard lock(notice->mutex);
  notice->state = state;
  notice->message = message;
}

#if defined(_WIN32)
std::wstring Wide(const std::string& s) {
  if (s.empty()) {
    return {};
  }
  const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), nullptr, 0);
  std::wstring w(size_t(n), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, s.data(), int(s.size()), w.data(), n);
  return w;
}

// GET https://<url>; the body to `sink` (or into `out`), redirects followed. Returns the HTTP
// status (0 if the request failed).
int HttpGet(const std::string& url, std::string* out,
            const std::function<bool(const char*, size_t)>& sink,
            const std::function<void(uint64_t, uint64_t)>& progress, bool api) {
  URL_COMPONENTS parts = {};
  parts.dwStructSize = sizeof(parts);
  wchar_t host[256] = {}, path[2048] = {};
  parts.lpszHostName = host;
  parts.dwHostNameLength = DWORD(std::size(host));
  parts.lpszUrlPath = path;
  parts.dwUrlPathLength = DWORD(std::size(path));
  wchar_t extra[2048] = {};
  parts.lpszExtraInfo = extra;
  parts.dwExtraInfoLength = DWORD(std::size(extra));
  const std::wstring wurl = Wide(url);
  if (!WinHttpCrackUrl(wurl.c_str(), 0, 0, &parts) || parts.nScheme != INTERNET_SCHEME_HTTPS) {
    return 0;
  }
  const std::wstring full_path = std::wstring(path) + extra;
  const std::wstring agent = Wide(kUserAgent);
  HINTERNET session = WinHttpOpen(agent.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                                  WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
  if (!session) {
    return 0;
  }
  WinHttpSetTimeouts(session, kTimeoutMs, kTimeoutMs, kTimeoutMs, 60000);
  int status_out = 0;
  HINTERNET connect = WinHttpConnect(session, host, parts.nPort, 0);
  HINTERNET request = connect ? WinHttpOpenRequest(connect, L"GET", full_path.c_str(), nullptr,
                                                   WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                                   WINHTTP_FLAG_SECURE)
                              : nullptr;
  const wchar_t* headers = api ? L"Accept: application/vnd.github+json\r\n" : WINHTTP_NO_ADDITIONAL_HEADERS;
  if (request &&
      WinHttpSendRequest(request, headers, api ? DWORD(-1) : 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
      WinHttpReceiveResponse(request, nullptr)) {
    DWORD status = 0, size = sizeof(status);
    WinHttpQueryHeaders(request, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &size, WINHTTP_NO_HEADER_INDEX);
    status_out = int(status);
    uint64_t total = 0;
    wchar_t length[32] = {};
    DWORD length_size = sizeof(length);
    if (WinHttpQueryHeaders(request, WINHTTP_QUERY_CONTENT_LENGTH, WINHTTP_HEADER_NAME_BY_INDEX,
                            length, &length_size, WINHTTP_NO_HEADER_INDEX)) {
      total = _wcstoui64(length, nullptr, 10);
    }
    if (status == 200) {
      uint64_t got = 0;
      std::vector<char> chunk(65536);
      DWORD read = 0;
      while (WinHttpReadData(request, chunk.data(), DWORD(chunk.size()), &read) && read > 0) {
        got += read;
        if (sink) {
          if (!sink(chunk.data(), read)) {
            status_out = -1;
            break;
          }
        } else if (out) {
          if (out->size() + read > kMaxBody) {
            break;
          }
          out->append(chunk.data(), read);
        }
        if (progress) {
          progress(got, total);
        }
      }
      if (status_out == 200 && total && got != total) {
        status_out = -1;  // cut short
      }
    }
  }
  if (request) WinHttpCloseHandle(request);
  if (connect) WinHttpCloseHandle(connect);
  WinHttpCloseHandle(session);
  return status_out;
}

std::optional<std::string> FetchReleases() {
  std::string body;
  const int status = HttpGet(std::string("https://api.github.com") + kApiPath, &body, nullptr,
                             nullptr, true);
  if (status != 200) {
    REXLOG_INFO("Update check: GitHub answered {}", status);
    return std::nullopt;
  }
  return body;
}
#else
std::optional<std::string> FetchReleases() {
  // curl is on SteamOS and the usual desktops; a fixed command line, nothing from the outside.
  const std::string command = std::string("curl -fsSL --max-time ") +
                              std::to_string(kTimeoutMs / 1000) + " -A " + kUserAgent +
                              " -H 'Accept: application/vnd.github+json' 'https://api.github.com" +
                              kApiPath + "' 2>/dev/null";
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

void CheckThread() {
  const std::string current = CurrentVersion();
  const std::optional<std::string> body = FetchReleases();
  if (!body) {
    SetState(State::kFailed, "Couldn't reach GitHub. Check the internet connection and try again.");
    return;
  }
  // The first release in the list is the newest; its assets come after its tag_name and before
  // the next release's tag_name.
  size_t tag_end = 0;
  const std::optional<std::string> tag = JsonString(*body, "tag_name", 0, body->size(), &tag_end);
  if (!tag || !PlainTag(*tag)) {
    SetState(State::kFailed, "GitHub has no release of this program yet.");
    return;
  }
  size_t next = body->find("\"tag_name\"", tag_end);
  if (next == std::string::npos) {
    next = body->size();
  }
  std::string zip_url, zip_sha256;
  for (size_t pos = tag_end;;) {
    size_t name_end = 0;
    const std::optional<std::string> name = JsonString(*body, "name", pos, next, &name_end);
    if (!name) {
      break;
    }
    pos = name_end;
    if (name->size() > std::char_traits<char>::length(kWindowsAsset) &&
        name->compare(name->size() - std::char_traits<char>::length(kWindowsAsset),
                      std::string::npos, kWindowsAsset) == 0) {
      size_t url_end = 0;
      const std::optional<std::string> url =
          JsonString(*body, "browser_download_url", name_end, next, &url_end);
      const std::optional<std::string> digest = JsonString(*body, "digest", name_end, url_end);
      if (url && url->rfind("https://github.com/", 0) == 0) {
        zip_url = *url;
        if (digest && digest->rfind("sha256:", 0) == 0 && digest->size() == 7 + 64) {
          zip_sha256 = digest->substr(7);
        }
      }
      break;
    }
  }
  std::string version = *tag;
  if (version[0] == 'v' || version[0] == 'V') {
    version.erase(0, 1);
  }
  const bool newer = IsNewerVersion(*tag, current);
  REXLOG_INFO("Update check: newest release {}, this is {}{}", version,
              current.empty() ? "a development build" : current,
              newer ? " - an update is available" : "");
  {
    std::lock_guard lock(g_state_mutex);
    g_zip_url = zip_url;
    g_zip_sha256 = zip_sha256;
  }
  std::shared_ptr<Notice> notice = Updater();
  std::lock_guard lock(notice->mutex);
  notice->current = current;
  notice->version = newer ? version : std::string();
  notice->url = std::string("https://github.com/") + kRepo + "/releases/tag/" + *tag;
#if defined(_WIN32)
  notice->can_install = newer && !zip_url.empty();
#else
  notice->can_install = false;  // Linux: the release page
#endif
  notice->state = newer ? State::kAvailable : State::kLatest;
  notice->message.clear();
}

void Check() {
  {
    std::lock_guard lock(g_state_mutex);
    if (g_busy) {
      return;
    }
    g_busy = true;
  }
  SetState(State::kChecking);
  std::thread([]() {
    CheckThread();
    {
      std::lock_guard lock(g_state_mutex);
      g_busy = false;
    }
    std::shared_ptr<Notice> notice = Updater();
    std::function<void()> install;
    {
      std::lock_guard lock(notice->mutex);
      if (notice->state == State::kAvailable && notice->can_install) {
        install = notice->install;
      }
    }
    if (install && REXCVAR_GET(update_test_auto_install)) {
      install();
    }
  }).detach();
}

#if defined(_WIN32)
std::filesystem::path ProgramDir() {
  return rex::filesystem::GetExecutableFolder();
}

bool RunHidden(std::wstring command) {
  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};
  if (!CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr,
                      nullptr, &si, &pi)) {
    return false;
  }
  CloseHandle(pi.hThread);
  WaitForSingleObject(pi.hProcess, INFINITE);
  DWORD code = 1;
  GetExitCodeProcess(pi.hProcess, &code);
  CloseHandle(pi.hProcess);
  return code == 0;
}

// The new files over the program's. A file that is there already (the running exe and DLLs among
// them: Windows lets them be renamed, not replaced) is renamed to <name>.old first and listed in
// update-old-files.txt for removal at the next start. On a failure the renamed ones go back.
bool ApplyFiles(const std::filesystem::path& from, const std::filesystem::path& to, int& count) {
  std::vector<std::filesystem::path> moved;
  std::ofstream old_list(to / "update-old-files.txt", std::ios::app);
  std::error_code ec;
  bool ok = true;
  for (auto it = std::filesystem::recursive_directory_iterator(from, ec);
       ok && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
    if (ec) {
      ok = false;
      break;
    }
    const std::filesystem::path rel = std::filesystem::relative(it->path(), from, ec);
    const std::filesystem::path target = to / rel;
    if (it->is_directory()) {
      std::filesystem::create_directories(target, ec);
      continue;
    }
    if (std::filesystem::exists(target)) {
      std::filesystem::path old = target;
      old += ".old";
      SetFileAttributesW(old.c_str(), FILE_ATTRIBUTE_NORMAL);
      DeleteFileW(old.c_str());
      if (!MoveFileExW(target.c_str(), old.c_str(), MOVEFILE_REPLACE_EXISTING)) {
        ok = false;
        break;
      }
      moved.push_back(target);
      const auto old_u8 = old.u8string();
      old_list << std::string(old_u8.begin(), old_u8.end()) << "\n";
    }
    if (!CopyFileW(it->path().c_str(), target.c_str(), FALSE)) {
      ok = false;
      break;
    }
    ++count;
  }
  if (!ok) {
    for (auto r = moved.rbegin(); r != moved.rend(); ++r) {
      std::filesystem::path old = *r;
      old += ".old";
      DeleteFileW(r->c_str());
      MoveFileExW(old.c_str(), r->c_str(), MOVEFILE_REPLACE_EXISTING);
    }
  }
  return ok;
}

void InstallThread() {
  std::string url, sha256;
  {
    std::lock_guard lock(g_state_mutex);
    url = g_zip_url;
    sha256 = g_zip_sha256;
  }
  auto fail = [](const std::string& message) {
    REXLOG_WARN("Update: {}", message);
    SetState(State::kFailed, message);
  };
  const std::filesystem::path dir = ProgramDir();
  const std::filesystem::path work = dir / "update";
  const std::filesystem::path zip = work / "package.zip";
  const std::filesystem::path stage = work / "files";
  std::error_code ec;
  std::filesystem::remove_all(work, ec);
  std::filesystem::create_directories(stage, ec);
  std::ofstream file(zip, std::ios::binary);
  if (!file) {
    fail("Couldn't write in the program's folder.");
    return;
  }
  BCRYPT_ALG_HANDLE alg = nullptr;
  BCRYPT_HASH_HANDLE hash = nullptr;
  if (BCryptOpenAlgorithmProvider(&alg, BCRYPT_SHA256_ALGORITHM, nullptr, 0) == 0) {
    BCryptCreateHash(alg, &hash, nullptr, 0, nullptr, 0, 0);
  }
  REXLOG_INFO("Update: downloading {}", url);
  std::shared_ptr<Notice> notice = Updater();
  const int status = HttpGet(
      url, nullptr,
      [&](const char* data, size_t n) {
        if (hash) {
          BCryptHashData(hash, PUCHAR(data), ULONG(n), 0);
        }
        file.write(data, std::streamsize(n));
        return bool(file);
      },
      [&](uint64_t got, uint64_t total) {
        std::lock_guard lock(notice->mutex);
        notice->progress = total ? float(double(got) / double(total)) * 0.9f : 0.0f;
      },
      false);
  file.close();
  unsigned char digest[32] = {};
  const bool hashed = hash && BCryptFinishHash(hash, digest, 32, 0) == 0;
  if (hash) BCryptDestroyHash(hash);
  if (alg) BCryptCloseAlgorithmProvider(alg, 0);
  if (status != 200) {
    std::filesystem::remove_all(work, ec);
    fail(status < 0 ? "The download stopped. Check the internet connection and try again."
                    : "The download failed.");
    return;
  }
  if (!sha256.empty()) {
    char hex[65] = {};
    for (int i = 0; i < 32; ++i) {
      std::snprintf(hex + i * 2, 3, "%02x", digest[i]);
    }
    std::string expected = sha256;
    std::transform(expected.begin(), expected.end(), expected.begin(),
                   [](unsigned char c) { return char(std::tolower(c)); });
    if (!hashed || expected != hex) {
      std::filesystem::remove_all(work, ec);
      fail("The download was damaged (its checksum doesn't match). Try again.");
      return;
    }
    REXLOG_INFO("Update: SHA-256 matches the release's");
  }
  {
    std::lock_guard lock(notice->mutex);
    notice->progress = 0.92f;
  }
  // Windows' own tar (Windows 10 1803 and later) reads zips.
  wchar_t system_dir[MAX_PATH] = {};
  GetSystemDirectoryW(system_dir, MAX_PATH);
  if (!RunHidden(L"\"" + std::wstring(system_dir) + L"\\tar.exe\" -xf \"" + zip.wstring() +
                 L"\" -C \"" + stage.wstring() + L"\"")) {
    std::filesystem::remove_all(work, ec);
    fail("Couldn't unpack the download.");
    return;
  }
  // The release zip holds the program's files at its top (or in one folder).
  std::filesystem::path top = stage;
  if (!std::filesystem::exists(top / "fifastreet3.exe")) {
    for (const auto& entry : std::filesystem::directory_iterator(stage, ec)) {
      if (entry.is_directory() && std::filesystem::exists(entry.path() / "fifastreet3.exe")) {
        top = entry.path();
      }
    }
  }
  if (!std::filesystem::exists(top / "fifastreet3.exe") ||
      !std::filesystem::exists(top / "VERSION")) {
    std::filesystem::remove_all(work, ec);
    fail("The download isn't a Windows release of this program.");
    return;
  }
  int count = 0;
  const bool ok = ApplyFiles(top, dir, count);
  std::filesystem::remove_all(work, ec);
  if (!ok) {
    fail("Couldn't replace the program's files (is another copy running?). Nothing was changed.");
    return;
  }
  REXLOG_INFO("Update: installed {} files", count);
  std::lock_guard lock(notice->mutex);
  notice->progress = 1.0f;
  notice->state = State::kReady;
  notice->message.clear();
  if (REXCVAR_GET(update_test_auto_install) && notice->restart) {
    std::thread(notice->restart).detach();
  }
}

void Install() {
  {
    std::lock_guard lock(g_state_mutex);
    if (g_busy || g_zip_url.empty()) {
      return;
    }
    g_busy = true;
  }
  {
    std::shared_ptr<Notice> notice = Updater();
    std::lock_guard lock(notice->mutex);
    notice->state = State::kDownloading;
    notice->progress = 0.0f;
  }
  std::thread([]() {
    InstallThread();
    std::lock_guard lock(g_state_mutex);
    g_busy = false;
  }).detach();
}

// The new version is told to wait for this process to end (UpdaterStartup), then this one ends
// through the app's normal shutdown.
void Restart() {
  wchar_t pid[32];
  _snwprintf(pid, 32, L"%lu", GetCurrentProcessId());
  SetEnvironmentVariableW(L"FS3_UPDATE_WAIT_PID", pid);
  std::wstring command = GetCommandLineW();
  STARTUPINFOW si = {};
  si.cb = sizeof(si);
  PROCESS_INFORMATION pi = {};
  const BOOL ok = CreateProcessW(nullptr, command.data(), nullptr, nullptr, FALSE, 0, nullptr,
                                 ProgramDir().c_str(), &si, &pi);
  SetEnvironmentVariableW(L"FS3_UPDATE_WAIT_PID", nullptr);
  if (!ok) {
    SetState(State::kFailed, "Couldn't start the new version. Start the game again yourself.");
    return;
  }
  CloseHandle(pi.hThread);
  CloseHandle(pi.hProcess);
  REXLOG_INFO("Update: restarting into the new version");
  std::function<void()> quit;
  {
    std::lock_guard lock(g_state_mutex);
    quit = g_quit;
  }
  if (quit) {
    quit();
  }
}
#endif

}  // namespace

std::shared_ptr<Notice> Updater() {
  std::lock_guard lock(g_state_mutex);
  if (!g_notice) {
    g_notice = std::make_shared<Notice>();
    g_notice->current = CurrentVersion();
    if (StartedByRecompLauncher()) {
      g_notice->state = State::kManaged;
      g_notice->message = "RecompLauncher keeps this game up to date.";
    } else {
      g_notice->check = []() { Check(); };
#if defined(_WIN32)
      g_notice->install = []() { Install(); };
      g_notice->restart = []() { Restart(); };
#endif
    }
  }
  return g_notice;
}

void UpdaterStartup() {
#if defined(_WIN32)
  wchar_t wait[32] = {};
  if (GetEnvironmentVariableW(L"FS3_UPDATE_WAIT_PID", wait, 32)) {
    // A restart after an update: the old process ends first (it holds the old files).
    if (HANDLE process = OpenProcess(SYNCHRONIZE, FALSE, DWORD(wcstoul(wait, nullptr, 10)))) {
      WaitForSingleObject(process, 30000);
      CloseHandle(process);
    }
    SetEnvironmentVariableW(L"FS3_UPDATE_WAIT_PID", nullptr);
  }
  const std::filesystem::path list = rex::filesystem::GetExecutableFolder() / "update-old-files.txt";
  std::ifstream in(list);
  if (!in) {
    return;
  }
  std::vector<std::string> left;
  for (std::string line; std::getline(in, line);) {
    if (line.size() < 5 || line.compare(line.size() - 4, 4, ".old") != 0) {
      continue;  // only what the updater renamed
    }
    const std::filesystem::path old = std::filesystem::u8path(line);
    SetFileAttributesW(old.c_str(), FILE_ATTRIBUTE_NORMAL);
    if (!DeleteFileW(old.c_str()) && std::filesystem::exists(old)) {
      left.push_back(line);  // still in use: next time
    }
  }
  in.close();
  std::error_code ec;
  std::filesystem::remove(list, ec);
  if (!left.empty()) {
    std::ofstream out(list);
    for (const auto& line : left) {
      out << line << "\n";
    }
  }
#endif
}

bool UpdateCheckOffered() {
  return !StartedByRecompLauncher();
}

void StartUpdateCheck() {
  Updater();
  if (!REXCVAR_GET(update_check) || IsScriptedRun() || StartedByRecompLauncher()) {
    return;
  }
  if (CurrentVersion().empty()) {
    REXLOG_INFO("Update check: no VERSION file next to the program (development build), skipped");
    return;
  }
  Check();
}

void SetUpdaterQuit(std::function<void()> quit) {
  std::lock_guard lock(g_state_mutex);
  g_quit = std::move(quit);
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

}  // namespace fs3
