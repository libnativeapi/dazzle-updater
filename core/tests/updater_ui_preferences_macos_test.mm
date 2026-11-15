#include "test_support.h"
#include "../src/foundation/updater_ui_preferences.h"

#import <Foundation/Foundation.h>

namespace {

// These old names are fixtures for the preference-key migration.
NSArray<NSString*>* PreferenceKeys() {
  return @[@"DazzleAutomaticChecks", @"DazzleAutomaticDownloads", @"DazzleSkippedVersion",
           @"DazzleUpdaterAutomaticChecks", @"DazzleUpdaterAutomaticDownloads",
           @"DazzleUpdaterSkippedVersion"];
}

// The test executable has its own defaults domain. Restore any values from an
// earlier run, and leave unrelated preferences untouched.
class ScopedPreferences {
 public:
  ScopedPreferences() : defaults([NSUserDefaults standardUserDefaults]),
                        saved([[NSMutableDictionary alloc] init]) {
    for (NSString* key in PreferenceKeys()) {
      id value = [defaults objectForKey:key];
      if (value != nil) [saved setObject:value forKey:key];
      [defaults removeObjectForKey:key];
    }
  }
  ~ScopedPreferences() {
    for (NSString* key in PreferenceKeys()) {
      [defaults removeObjectForKey:key];
      id value = [saved objectForKey:key];
      if (value != nil) [defaults setObject:value forKey:key];
    }
    [saved release];
  }

  NSUserDefaults* defaults;
  NSMutableDictionary* saved;
};

}  // namespace

DAZZLE_TEST(macos_preferences_migrate_existing_values) {
  @autoreleasepool {
    ScopedPreferences scope;
    [scope.defaults setBool:NO forKey:@"DazzleAutomaticChecks"];
    [scope.defaults setBool:YES forKey:@"DazzleAutomaticDownloads"];
    [scope.defaults setObject:@"2.0.0" forKey:@"DazzleSkippedVersion"];

    const auto preferences = dazzle_updater::GetUiPreferencesStore().Read();
    EXPECT_TRUE(preferences.automatic_checks_answered);
    EXPECT_FALSE(preferences.automatic_checks);
    EXPECT_TRUE(preferences.automatic_downloads);
    EXPECT_EQ(preferences.skipped_version, "2.0.0");
    EXPECT_TRUE([scope.defaults objectForKey:@"DazzleUpdaterAutomaticChecks"] != nil);
    EXPECT_TRUE([scope.defaults boolForKey:@"DazzleUpdaterAutomaticDownloads"]);
    EXPECT_TRUE([[scope.defaults stringForKey:@"DazzleUpdaterSkippedVersion"] isEqualToString:@"2.0.0"]);
    EXPECT_TRUE([scope.defaults objectForKey:@"DazzleAutomaticChecks"] == nil);
    EXPECT_TRUE([scope.defaults objectForKey:@"DazzleAutomaticDownloads"] == nil);
    EXPECT_TRUE([scope.defaults objectForKey:@"DazzleSkippedVersion"] == nil);
  }
}

DAZZLE_TEST(macos_preferences_new_values_override_legacy_values) {
  @autoreleasepool {
    ScopedPreferences scope;
    [scope.defaults setBool:YES forKey:@"DazzleAutomaticChecks"];
    [scope.defaults setBool:YES forKey:@"DazzleAutomaticDownloads"];
    [scope.defaults setObject:@"2.0.0" forKey:@"DazzleSkippedVersion"];
    [scope.defaults setBool:NO forKey:@"DazzleUpdaterAutomaticChecks"];
    [scope.defaults setBool:NO forKey:@"DazzleUpdaterAutomaticDownloads"];
    [scope.defaults setObject:@"3.0.0" forKey:@"DazzleUpdaterSkippedVersion"];

    const auto preferences = dazzle_updater::GetUiPreferencesStore().Read();
    EXPECT_TRUE(preferences.automatic_checks_answered);
    EXPECT_FALSE(preferences.automatic_checks);
    EXPECT_FALSE(preferences.automatic_downloads);
    EXPECT_EQ(preferences.skipped_version, "3.0.0");
  }
}

DAZZLE_TEST(macos_preferences_write_before_read_does_not_restore_cleared_values) {
  @autoreleasepool {
    ScopedPreferences scope;
    [scope.defaults setBool:YES forKey:@"DazzleAutomaticChecks"];
    [scope.defaults setBool:YES forKey:@"DazzleAutomaticDownloads"];
    [scope.defaults setObject:@"2.0.0" forKey:@"DazzleSkippedVersion"];

    auto& store = dazzle_updater::GetUiPreferencesStore();
    store.Write({});
    const auto preferences = store.Read();
    EXPECT_FALSE(preferences.automatic_checks_answered);
    EXPECT_FALSE(preferences.automatic_downloads);
    EXPECT_TRUE(preferences.skipped_version.empty());
    EXPECT_TRUE([scope.defaults objectForKey:@"DazzleAutomaticChecks"] == nil);
    EXPECT_TRUE([scope.defaults objectForKey:@"DazzleAutomaticDownloads"] == nil);
    EXPECT_TRUE([scope.defaults objectForKey:@"DazzleSkippedVersion"] == nil);
  }
}
