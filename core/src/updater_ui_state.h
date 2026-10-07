#pragma once

#include <functional>
#include <string>
#include <vector>

// What UpdaterUI draws, and what it asks the platform to draw it with. Internal: the
// public class is src/updater_ui.h, and this header is not read by the code generator.
//
// The states are the ones in ../specs/updater-ui.md, "窗口与流程", with the same names.
// Which window owns a state is fixed (see the table there); the state machine decides
// the state and the content, a platform backend decides the window.

namespace dazzle {
namespace internal {

/// One state of a round. `kPermission` is the one-time question, which is not part of
/// the update flow itself.
enum class UiState {
  kChecking,
  kAvailable,
  kDownloaded,
  kDownloading,
  kReady,
  kInstalling,
  kUpToDate,
  kError,
  kPermission,
};

/// The window that draws a state.
enum class UiWindow {
  kNone,
  kUpdateAvailable,   // Available, Downloaded
  kUpdateStatus,      // Checking, Downloading, Ready, Installing, UpToDate, Error
  kUpdatePermission,  // the first silent check's question
};

/// Which Updater call failed; it picks the Error state's headline.
enum class UiErrorStep { kCheck, kDownload, kInstall };

/// What the user did. The state machine turns it into the next state and the next
/// Updater call; the backend only reports it.
enum class UiEventKind {
  kPrimary,          // the default button: Install Update, Install and Relaunch, OK, Check Automatically
  kSecondary,        // Later, Don't Check
  kCancel,           // Cancel
  kSkip,             // Skip This Version
  kClose,            // Esc or the window's close button
  kCheckboxChanged,  // flag carries the check box's new value
};

struct UiEvent {
  UiEventKind kind = UiEventKind::kClose;
  bool flag = false;
};

/// A button of the state's action row. `label` is already localized.
struct UiButton {
  std::string label;
  UiEventKind event = UiEventKind::kPrimary;
  bool is_default = false;
};

/// The automatic-downloads check box: only the Available window and the permission
/// prompt have one, and only while the application allows automatic downloads. The
/// label differs between the two (see the wording table), so it travels with the box.
struct UiCheckbox {
  bool visible = false;
  bool checked = false;
  std::string label;
};

/// A progress bar and the optional line of text beside it. `determinate` is false for
/// an indeterminate bar: while checking, while installing, and while downloading a
/// package whose size the release index does not state.
struct UiProgress {
  bool visible = false;
  bool determinate = false;
  double fraction = 0.0;
  std::string status_text;
};

/// Everything a window needs for one state. The state machine builds it (see
/// updater_ui_content.h) and the backend draws it; passing it by value across threads
/// is what keeps the platform layer free of flow logic.
struct UiContent {
  UiState state = UiState::kChecking;
  UiWindow window = UiWindow::kNone;

  std::string window_title;
  std::string heading;        // the bold line
  std::string detail;         // the small line under it
  std::string error_message;  // GetErrorMessage(), the small monospace line under a detail
  std::string notes_markdown; // non-empty: the notes box is shown, and the window resizes
  std::string notes_url;      // linked when notes_markdown is empty
  std::string notes_link_label;

  UiCheckbox checkbox;
  UiProgress progress;
  std::vector<UiButton> buttons;
  bool closable = true;   // Installing has no way out
  bool resizable = false; // only with a notes box
};

/// Whether a state is drawn in one of the three windows (false only before the first
/// state, when `window` is kNone).
inline bool IsUpdateWindow(UiWindow window) {
  return window == UiWindow::kUpdateAvailable || window == UiWindow::kUpdateStatus ||
         window == UiWindow::kUpdatePermission;
}

/// The platform's native windows, and the only platform seam UpdaterUI has.
///
/// The implementation lives in platform/<os>/updater_ui_backend_<os>.*; a platform that
/// provides no file uses the cross-platform default
/// (foundation/updater_ui_backend_default.cpp), which has no window at all and reports
/// itself unavailable — that is also what a build with DAZZLE_WITH_UPDATER_UI=OFF
/// compiles, so "this platform has no UI" and "the UI was left out of the build" are the
/// same code path.
///
/// Every method is called from any thread and must return immediately: an
/// implementation marshals to its own UI thread (the main thread on macOS, a thread of
/// the module's own on Windows). An implementation must never call the event sink from
/// inside Draw/Hide/BringToFront/QuitApplication; those calls happen while the state
/// machine holds its lock.
class UiBackend {
 public:
  virtual ~UiBackend() = default;

  /// Whether this platform has the built-in windows. When false, UpdaterUI's entry
  /// points return false and nothing is ever shown.
  virtual bool IsAvailable() const = 0;

  /// Draws `content` in the window that owns its state, creating that window when
  /// needed. The window may differ from the one on screen — the flow moves between two
  /// windows — in which case the implementation opens the new one centred on the old and
  /// then closes the old one, so the change looks like an in-place replacement. Two
  /// contents of the same window update it instead of rebuilding it.
  virtual void Draw(const UiContent& content) = 0;

  /// Closes the window on screen, if any.
  virtual void Hide() = 0;

  /// Brings the window on screen to the front without changing its content.
  virtual void BringToFront() = 0;

  /// Asks the application to quit the way its platform does, after InstallUpdate()
  /// returned true (../specs/updater-ui.md, "退出应用").
  virtual void QuitApplication() = 0;

  /// The sink button presses are reported to. Set once, before the first Draw().
  virtual void SetEventSink(std::function<void(const UiEvent&)> sink) = 0;
};

}  // namespace internal
}  // namespace dazzle
