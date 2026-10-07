#pragma once

#include <string>
#include <vector>

// The windows' wording (../specs/updater-ui.md, "本地化"): English and Simplified
// Chinese, chosen from the system's preferred languages, English for everything else.
//
// The table is data, not code: every message with a value in it is a pattern with {0},
// {1} … placeholders, so the two languages stay side by side and can be diffed against
// prototype/src/features/updater-ui/strings.ts, which holds the same wording.
// Internal: not a public header, and not read by the code generator.

namespace dazzle {
namespace internal {

struct UiStrings {
  const char* software_update;
  const char* app_update;          // {0}: the application name
  const char* checking;
  const char* available_title;     // {0}
  const char* available_detail;    // {0} name, {1} new version, {2} running version
  const char* view_release_notes;
  const char* downloading;
  const char* downloaded_of;       // {0} received, {1} total
  const char* downloaded;          // {0} received
  const char* ready_title;
  const char* installing;
  const char* up_to_date_title;
  const char* up_to_date_detail;   // {0} name, {1} running version

  const char* error_check;
  const char* error_download;
  const char* error_install;
  const char* error_network;
  const char* error_invalid_index;
  const char* error_signature;
  const char* error_permission;    // {0} name
  const char* error_app_location;  // {0} name
  const char* error_installer;
  const char* error_other;

  const char* skip;
  const char* automatic_downloads;
  const char* downloaded_title;    // {0}
  const char* downloaded_detail;   // {0} name, {1} new version

  const char* permission_title;
  const char* permission_detail;   // {0}
  const char* permission_downloads;
  const char* dont_check;
  const char* check_automatically;

  const char* cancel;
  const char* later;
  const char* install_update;
  const char* install_and_relaunch;
  const char* ok;
};

/// The wording for the system's preferred language, from the seam in
/// foundation/updater_ui_preferences.h; English when none of the languages is one the
/// windows are worded in.
const UiStrings& GetUiStrings();

/// The wording for the first of `preferred_languages` that is one of them. Exposed for
/// tests, which must not depend on the machine's language.
const UiStrings& UiStringsFor(const std::vector<std::string>& preferred_languages);

/// Replaces every occurrence of {0}, {1} … in `pattern` with the matching value. A
/// placeholder beyond the values given is left as it is, so a mistake in the table is
/// visible rather than silent.
std::string Format(const char* pattern, const std::vector<std::string>& values);

}  // namespace internal
}  // namespace dazzle
