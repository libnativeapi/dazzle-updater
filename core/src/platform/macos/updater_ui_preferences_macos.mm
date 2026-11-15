#include "../../foundation/updater_ui_preferences.h"

#import <Foundation/Foundation.h>

// The macOS store for what UpdaterUI remembers (../../specs/updater-ui.md, "用户偏好"):
// the standard user defaults of the application, so the answers follow the application
// and are per user.
//
// Key names are dazzle_updater's own, as the spec's table lists them. An absent automatic-checks
// key is "not answered yet", which is what makes the one-time question one time only.

namespace dazzle_updater {
namespace {

std::string ToUtf8(NSString* value) {
  if (value == nil) {
    return "";
  }
  const char* utf8 = [value UTF8String];
  return utf8 != nullptr ? std::string(utf8) : std::string();
}

NSString* ToNSString(const std::string& value) {
  return [NSString stringWithUTF8String:value.c_str()];
}

NSString* const kAutomaticChecksKey = @"DazzleUpdaterAutomaticChecks";
NSString* const kAutomaticDownloadsKey = @"DazzleUpdaterAutomaticDownloads";
NSString* const kSkippedVersionKey = @"DazzleUpdaterSkippedVersion";

// Legacy names are retained only to migrate existing preferences. New values win;
// remove old keys even when both exist so clearing a setting cannot resurrect it.
void MigratePreferences(NSUserDefaults* defaults) {
  NSDictionary<NSString*, NSString*>* keys = @{
    @"DazzleAutomaticChecks": kAutomaticChecksKey,
    @"DazzleAutomaticDownloads": kAutomaticDownloadsKey,
    @"DazzleSkippedVersion": kSkippedVersionKey,
  };
  for (NSString* oldKey in keys) {
    id value = [defaults objectForKey:oldKey];
    if (value != nil) {
      NSString* newKey = keys[oldKey];
      if ([defaults objectForKey:newKey] == nil) {
        [defaults setObject:value forKey:newKey];
      }
      [defaults removeObjectForKey:oldKey];
    }
  }
}

class DefaultsPreferencesStore final : public UiPreferencesStore {
 public:
  UiPreferences Read() override {
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    MigratePreferences(defaults);
    UiPreferences preferences;
    if ([defaults objectForKey:kAutomaticChecksKey] != nil) {
      preferences.automatic_checks_answered = true;
      preferences.automatic_checks = [defaults boolForKey:kAutomaticChecksKey];
    }
    preferences.automatic_downloads = [defaults boolForKey:kAutomaticDownloadsKey];
    preferences.skipped_version = ToUtf8([defaults stringForKey:kSkippedVersionKey]);
    return preferences;
  }

  void Write(const UiPreferences& preferences) override {
    NSUserDefaults* defaults = [NSUserDefaults standardUserDefaults];
    MigratePreferences(defaults);
    if (preferences.automatic_checks_answered) {
      [defaults setBool:preferences.automatic_checks forKey:kAutomaticChecksKey];
    } else {
      [defaults removeObjectForKey:kAutomaticChecksKey];
    }
    [defaults setBool:preferences.automatic_downloads forKey:kAutomaticDownloadsKey];
    if (preferences.skipped_version.empty()) {
      [defaults removeObjectForKey:kSkippedVersionKey];
    } else {
      [defaults setObject:ToNSString(preferences.skipped_version) forKey:kSkippedVersionKey];
    }
  }
};

}  // namespace

UiPreferencesStore& GetUiPreferencesStore() {
  static DefaultsPreferencesStore store;
  return store;
}

std::vector<std::string> UiPreferredLanguages() {
  std::vector<std::string> languages;
  for (NSString* language in [NSLocale preferredLanguages]) {
    languages.push_back(ToUtf8(language));
  }
  return languages;
}

}  // namespace dazzle_updater
