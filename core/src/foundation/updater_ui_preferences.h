#pragma once

#include <string>
#include <vector>

// The user's own settings that UpdaterUI needs: what ../specs/updater-ui.md,
// "用户偏好", stores in the platform's user preferences, plus the language the windows
// are worded in.
//
// A seam, like the windows themselves: the implementation lives in
// platform/<os>/updater_ui_preferences_<os>.*, and a platform that provides no file uses
// the cross-platform default (foundation/updater_ui_preferences_default.cpp), which keeps
// the answer in memory for the life of the process, so a platform without a store still
// behaves the same within one run. It is an object rather than free functions because the
// state machine holds one store for its lifetime and a test substitutes its own — the
// functions never throw.

namespace dazzle {

/// What UpdaterUI remembers for the user on their behalf.
struct UiPreferences {
  /// False until the user answered the permission prompt, or the application called
  /// UpdaterUI::SetAutomaticChecksEnabled() itself. The window asks only while this is
  /// false; `automatic_checks` is meaningless until then.
  bool automatic_checks_answered = false;
  bool automatic_checks = false;
  bool automatic_downloads = false;
  /// The version the user skipped, or "" when none is skipped.
  std::string skipped_version;
};

/// Where this platform keeps those answers.
class UiPreferencesStore {
 public:
  virtual ~UiPreferencesStore() = default;

  /// The stored answers; all-default when nothing is stored yet.
  virtual UiPreferences Read() = 0;

  /// Replaces what was stored. Called with the whole struct, so a platform store writes
  /// the values it knows about and leaves the rest alone.
  virtual void Write(const UiPreferences& preferences) = 0;
};

/// The store for this platform and this build; never null, never throws.
UiPreferencesStore& GetUiPreferencesStore();

/// The user's preferred languages, most preferred first, as BCP-47 tags such as
/// "zh-Hans-CN" or "en-US"; empty when the platform cannot tell. UpdaterUI uses the first
/// tag it has wording for and falls back to English.
std::vector<std::string> UiPreferredLanguages();

}  // namespace dazzle
