#include "../../foundation/updater_ui_preferences.h"

#include <sys/stat.h>
#include <sys/types.h>

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <vector>

#include "../../foundation/system.h"

// Linux's preferences store (../../specs/updater-ui.md, "用户偏好"): one ini file per
// application, `$XDG_CONFIG_HOME/dazzle/<application>.conf`, with the three answers in an
// `[updater-ui]` group. Per application because the answers are: a user who told one
// program not to check for updates has not told the next one.
//
// It is a plain text format read and written here rather than through GSettings or dconf:
// the windows are GTK, but the answers must also work for an application that only links
// the library — and a file the user can read, edit and delete is the honest Linux
// equivalent of `NSUserDefaults` and the registry key the other two platforms use.

namespace dazzle {
namespace {

const char* const kGroup = "updater-ui";
const char* const kAutomaticChecksKey = "AutomaticChecks";
const char* const kAutomaticDownloadsKey = "AutomaticDownloads";
const char* const kSkippedVersionKey = "SkippedVersion";

std::string EnvironmentVariable(const char* name) {
  const char* value = std::getenv(name);
  return value != nullptr ? std::string(value) : std::string();
}

std::string ConfigDirectory() {
  const std::string home = EnvironmentVariable("HOME");
  const std::string base = EnvironmentVariable("XDG_CONFIG_HOME");
  if (!base.empty() && base[0] == '/') {
    return base;
  }
  return home.empty() ? "" : home + "/.config";
}

/// The application's own file name: the executable's name, since that is what Linux has
/// to identify a program by, reduced to characters a file name can hold.
std::string ApplicationFileName() {
  std::string name = AppNameFromPlatform();
  if (name.empty()) {
    name = "updater-ui";
  }
  std::string safe;
  for (char character : name) {
    const unsigned char value = static_cast<unsigned char>(character);
    safe.push_back(std::isalnum(value) != 0 || character == '-' || character == '_' ||
                           character == '.'
                       ? character
                       : '_');
  }
  return safe;
}

std::string ConfigPath() {
  const std::string directory = ConfigDirectory();
  return directory.empty() ? "" : directory + "/dazzle/" + ApplicationFileName() + ".conf";
}

/// Creates every missing parent of `path`; true when the directory exists afterwards.
bool EnsureDirectory(const std::string& path) {
  if (path.empty()) {
    return false;
  }
  const size_t separator = path.find_last_of('/');
  const std::string parent = separator == std::string::npos ? "" : path.substr(0, separator);
  if (!parent.empty() && parent != "/" && !EnsureDirectory(parent)) {
    return false;
  }
  if (mkdir(path.c_str(), 0755) == 0) {
    return true;
  }
  struct stat info;
  return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

/// The keys of the `[updater-ui]` group, in the order the file holds them. Comments,
/// blank lines and any other group are kept out of the map, so an application may store
/// its own settings in the same file without them being rewritten.
std::map<std::string, std::string> ReadGroup(const std::string& path) {
  std::map<std::string, std::string> values;
  std::ifstream file(path);
  std::string line;
  bool in_group = false;
  while (std::getline(file, line)) {
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }
    const size_t first = line.find_first_not_of(" \t");
    if (first == std::string::npos || line[first] == '#' || line[first] == ';') {
      continue;
    }
    if (line[first] == '[') {
      const std::string group = "[" + std::string(kGroup) + "]";
      in_group = line.compare(first, group.size(), group) == 0;
      continue;
    }
    if (!in_group) {
      continue;
    }
    const size_t separator = line.find('=', first);
    if (separator == std::string::npos) {
      continue;
    }
    values[line.substr(first, separator - first)] = line.substr(separator + 1);
  }
  return values;
}

bool ParseBool(const std::map<std::string, std::string>& values, const char* key, bool fallback) {
  const auto found = values.find(key);
  if (found == values.end()) {
    return fallback;
  }
  return found->second == "1" || found->second == "true" || found->second == "yes";
}

std::string FormatBool(bool value) {
  return value ? "1" : "0";
}

class FilePreferencesStore final : public UiPreferencesStore {
 public:
  UiPreferences Read() override {
    std::lock_guard<std::mutex> lock(mutex_);
    UiPreferences preferences;
    const std::string path = ConfigPath();
    if (path.empty()) {
      return preferences;
    }
    const std::map<std::string, std::string> values = ReadGroup(path);
    // An absent key is "not answered yet", which is what makes the one-time question one
    // time only.
    preferences.automatic_checks_answered = values.count(kAutomaticChecksKey) > 0;
    preferences.automatic_checks = ParseBool(values, kAutomaticChecksKey, false);
    preferences.automatic_downloads = ParseBool(values, kAutomaticDownloadsKey, false);
    const auto skipped = values.find(kSkippedVersionKey);
    if (skipped != values.end()) {
      preferences.skipped_version = skipped->second;
    }
    return preferences;
  }

  void Write(const UiPreferences& preferences) override {
    std::lock_guard<std::mutex> lock(mutex_);
    const std::string path = ConfigPath();
    if (path.empty() || !EnsureDirectory(ConfigDirectory() + "/dazzle")) {
      return;  // no home directory to write into: the answers stay in memory
    }
    // The whole file is rewritten: it holds dazzle's three keys and nothing else.
    std::ofstream file(path, std::ios::trunc);
    if (!file) {
      return;
    }
    file << "# dazzle UpdaterUI preferences. Written by the application's own updater.\n";
    file << "[" << kGroup << "]\n";
    if (preferences.automatic_checks_answered) {
      file << kAutomaticChecksKey << "=" << FormatBool(preferences.automatic_checks) << "\n";
    }
    file << kAutomaticDownloadsKey << "=" << FormatBool(preferences.automatic_downloads) << "\n";
    if (!preferences.skipped_version.empty()) {
      file << kSkippedVersionKey << "=" << preferences.skipped_version << "\n";
    }
  }

 private:
  std::mutex mutex_;
};

/// A language tag from a POSIX locale name: "zh_CN.UTF-8" is asked for as "zh-CN", which
/// is what the wording table matches on.
std::string LanguageFromLocale(const std::string& locale) {
  std::string value = locale;
  const size_t dot = value.find('.');
  if (dot != std::string::npos) {
    value = value.substr(0, dot);
  }
  const size_t at = value.find('@');
  if (at != std::string::npos) {
    value = value.substr(0, at);
  }
  for (char& character : value) {
    if (character == '_') {
      character = '-';
    }
  }
  return value;
}

}  // namespace

UiPreferencesStore& GetUiPreferencesStore() {
  static FilePreferencesStore store;
  return store;
}

std::vector<std::string> UiPreferredLanguages() {
  // LANGUAGE is GNU's own list, most preferred first, and the only variable that can name
  // more than one language; the POSIX ones name exactly one each, in their usual order of
  // precedence.
  std::vector<std::string> languages;
  std::stringstream stream(EnvironmentVariable("LANGUAGE"));
  std::string entry;
  while (std::getline(stream, entry, ':')) {
    const std::string language = LanguageFromLocale(entry);
    if (!language.empty()) {
      languages.push_back(language);
    }
  }
  if (languages.empty()) {
    for (const char* variable : {"LC_ALL", "LC_MESSAGES", "LANG"}) {
      const std::string language = LanguageFromLocale(EnvironmentVariable(variable));
      if (language.empty() || language == "C" || language == "POSIX") {
        continue;
      }
      languages.push_back(language);
      break;
    }
  }
  return languages;
}

}  // namespace dazzle
