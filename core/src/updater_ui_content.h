#pragma once

#include <cstdint>
#include <string>

#include "updater_ui_state.h"
#include "updater_ui_strings.h"

// Turns one state of a round into what a window shows: the window, its texts, its
// buttons and its controls (../specs/updater-ui.md, "窗口与流程" and "错误文案").
//
// Nothing here touches a platform, a thread or Updater: the state machine fills in a
// ContentInput and gets back the UiContent a backend draws. That keeps the wording and
// the button set of every state, including every error code, testable on its own.

namespace dazzle_updater {
namespace internal {

/// What the state machine knows about the round when it draws a state.
struct ContentInput {
  const UiStrings* strings = nullptr;
  UiState state = UiState::kChecking;
  UiErrorStep error_step = UiErrorStep::kCheck;
  std::string app_name;
  std::string current_version;
  std::string update_version;
  std::string update_display_version;
  std::string release_notes;      // Markdown, as GetUpdateReleaseNotes() returns it
  std::string release_notes_url;  // linked when there are no notes
  int64_t update_size = 0;        // bytes; 0 when the release index does not say
  double progress = 0.0;          // 0.0–1.0, or negative when the total size is unknown
  std::string error_code;         // GetErrorCode()
  std::string error_message;      // GetErrorMessage(), shown under the text
  bool show_automatic_downloads = false;
  bool automatic_downloads_checked = false;
};

/// The state, ready to draw.
UiContent BuildContent(const ContentInput& input);

/// Byte counts as the windows word them: "512 KB", "12.3 MB", "1.25 GB". Decimal units,
/// matching prototype/src/features/updater-ui/format.ts.
std::string FormatBytes(int64_t bytes);

}  // namespace internal
}  // namespace dazzle_updater
