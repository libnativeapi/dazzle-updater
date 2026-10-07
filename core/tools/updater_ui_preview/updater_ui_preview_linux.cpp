// A development tool: drives the real UpdaterUI state machine with a scripted Updater
// port and captures each GTK 4 window to a PNG, so the Linux windows can be compared
// with prototype/ — and with the macOS ones — state by state.
//
// It is not built by CMake and not part of the library or the tests; see README.md next
// to this file for the one-line build and how to read the pictures.
//
// The port is the tests' idea: nothing here touches the network, a package or an
// installer. Where the tests assert, this one only draws, so the windows can be looked
// at. Buttons are pressed the way a click presses them (the "clicked" signal of the real
// button, found by its label), so the pictures are of the real windows and the real flow.
//
// The pictures are rendered by GTK itself, not read off the screen: each window is
// snapshotted into a cairo image surface through GtkWidgetPaintable, so the tool needs a
// display but no window manager and no screenshot permission.

#include <gtk/gtk.h>

#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "../../src/foundation/updater_ui_backend.h"
#include "../../src/foundation/updater_ui_preferences.h"
#include "../../src/updater_ui_runner.h"
#include "../../src/updater_ui_state.h"
#include "../../src/updater_ui_strings.h"

using dazzle::UiPreferences;
using dazzle::UiPreferencesStore;
using dazzle::internal::UiRunner;
using dazzle::internal::UpdaterPort;

namespace {

/// DAZZLE_PREVIEW_DARK=1 renders the dark theme, DAZZLE_PREVIEW_LANG=zh the Simplified
/// Chinese wording, DAZZLE_PREVIEW_PREFIX names the files, and DAZZLE_PREVIEW_OUT the
/// directory they land in.
bool PreviewDark() {
  return g_getenv("DAZZLE_PREVIEW_DARK") != nullptr;
}

bool PreviewChinese() {
  const char* language = g_getenv("DAZZLE_PREVIEW_LANG");
  return language != nullptr && std::string(language) == "zh";
}

std::string PreviewPrefix() {
  const char* prefix = g_getenv("DAZZLE_PREVIEW_PREFIX");
  return prefix != nullptr ? std::string(prefix) : std::string();
}

std::string OutputDirectory() {
  const char* directory = g_getenv("DAZZLE_PREVIEW_OUT");
  if (directory != nullptr && *directory != '\0') {
    return directory;
  }
  return "build/preview-linux";
}

const char* const kNotes =
    "## What's new\n"
    "\n"
    "- **Faster** startup: the window opens in half the time.\n"
    "- Fixed a crash when the last document was closed twice.\n"
    "- `dazzle` now reports the version it is updating *from*.\n"
    "\n"
    "### Notes\n"
    "\n"
    "See the [full announcement](https://example.com/releases/2.6.1) for the details.\n"
    "\n"
    "```\n"
    "dazzle updater --check\n"
    "```\n";

class MemoryStore final : public UiPreferencesStore {
 public:
  UiPreferences Read() override { return preferences; }
  void Write(const UiPreferences& value) override { preferences = value; }
  UiPreferences preferences;
};

/// The scripted Updater: the same one the macOS preview and the unit tests use. The gates
/// hold a blocking call on the module's worker thread so the states that would otherwise
/// flash by can be captured.
class ScriptedPort final : public UpdaterPort {
 public:
  bool check_ok = true;
  bool has_update = true;
  bool download_ok = true;
  bool install_ok = true;
  std::string version = "2.6.1";
  std::string display_version = "2.6.1";
  std::string notes;
  std::string notes_url;
  int64_t size = 0;
  double progress = 0.0;
  std::string error_code = "network";
  std::string error_message = "update server unreachable (timeout after 30s)";
  std::string current_version = "2.5.0";

  bool CheckForUpdates() override {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (gate_checks_) {
        cv_.wait(lock, [this] { return released_; });
      }
    }
    downloaded_ = false;  // a check clears what the last one downloaded
    return check_ok;
  }
  bool IsUpdateAvailable() const override { return check_ok && has_update; }
  std::string GetUpdateVersion() const override { return has_update ? version : std::string(); }
  std::string GetUpdateDisplayVersion() const override { return display_version; }
  std::string GetUpdateReleaseNotes() const override { return notes; }
  std::string GetUpdateReleaseNotesUrl() const override { return notes_url; }
  int64_t GetUpdateSize() const override { return size; }
  bool DownloadUpdate() override {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (gate_downloads_) {
        download_cv_.wait(lock, [this] { return download_released_; });
      }
    }
    downloaded_ = download_ok;
    return download_ok;
  }
  double GetDownloadProgress() const override { return progress; }
  void CancelDownload() override { ReleaseDownload(); }
  bool IsUpdateDownloaded() const override { return downloaded_; }
  bool InstallUpdate() override {
    {
      std::unique_lock<std::mutex> lock(mutex_);
      if (gate_installs_) {
        install_cv_.wait(lock, [this] { return install_released_; });
      }
    }
    return install_ok;
  }
  std::string GetErrorCode() const override { return error_code; }
  std::string GetErrorMessage() const override { return error_message; }
  std::string GetCurrentVersion() const override { return current_version; }

  void GateChecks() {
    std::lock_guard<std::mutex> lock(mutex_);
    gate_checks_ = true;
    released_ = false;
  }
  void ReleaseChecks() {
    std::lock_guard<std::mutex> lock(mutex_);
    released_ = true;
    cv_.notify_all();
  }
  void GateDownloads() {
    std::lock_guard<std::mutex> lock(mutex_);
    gate_downloads_ = true;
    download_released_ = false;
  }
  void ReleaseDownload() {
    std::lock_guard<std::mutex> lock(mutex_);
    download_released_ = true;
    download_cv_.notify_all();
  }
  void ForgetDownload() { downloaded_ = false; }
  void GateInstalls() {
    std::lock_guard<std::mutex> lock(mutex_);
    gate_installs_ = true;
    install_released_ = false;
  }
  void ReleaseInstall() {
    std::lock_guard<std::mutex> lock(mutex_);
    install_released_ = true;
    install_cv_.notify_all();
  }

 private:
  bool downloaded_ = false;
  std::mutex mutex_;
  std::condition_variable cv_;
  std::condition_variable download_cv_;
  std::condition_variable install_cv_;
  bool gate_checks_ = false;
  bool released_ = false;
  bool gate_downloads_ = false;
  bool download_released_ = false;
  bool gate_installs_ = false;
  bool install_released_ = false;
};

MemoryStore g_store;
ScriptedPort g_port;

/// A runner that reads `g_store` when it first needs it, which is how a round is started
/// from a given set of user answers. The module's own singleton is never destroyed either.
UiRunner* NewRunner() {
  UiRunner* runner = new UiRunner(dazzle::internal::GetUiBackend(), g_port, g_store,
                                  dazzle::internal::UiStringsFor({PreviewChinese() ? "zh-Hans"
                                                                                  : "en"}));
  runner->SetAppName("Dazzle Demo");
  return runner;
}

/// The window the backend has on screen. The tool has no windows of its own, and the flow
/// replaces one window with the next, so the newest visible toplevel is the one on screen.
GtkWidget* CurrentWindow() {
  GListModel* toplevels = gtk_window_get_toplevels();
  const guint count = g_list_model_get_n_items(toplevels);
  GtkWidget* found = nullptr;
  for (guint index = 0; index < count; ++index) {
    GtkWindow* window = GTK_WINDOW(g_list_model_get_item(toplevels, index));
    if (gtk_widget_get_visible(GTK_WIDGET(window))) {
      found = GTK_WIDGET(window);
    }
    g_object_unref(window);  // the window itself is still owned by GTK
  }
  return found;
}

/// The labels the state's buttons carry, in the language this run draws: the flow is
/// driven by pressing the real buttons, so their names have to be the drawn ones.
const dazzle::internal::UiStrings& Strings() {
  static const dazzle::internal::UiStrings& strings =
      dazzle::internal::UiStringsFor({PreviewChinese() ? "zh-Hans" : "en"});
  return strings;
}

GtkWidget* FindButton(GtkWidget* root, const std::string& label) {
  if (GTK_IS_BUTTON(root)) {
    const char* text = gtk_button_get_label(GTK_BUTTON(root));
    if (text != nullptr && label == text) {
      return root;
    }
  }
  for (GtkWidget* child = gtk_widget_get_first_child(root); child != nullptr;
       child = gtk_widget_get_next_sibling(child)) {
    if (GtkWidget* found = FindButton(child, label)) {
      return found;
    }
  }
  return nullptr;
}

/// Presses the window's button with this title, the way a click would.
void Click(const std::string& label) {
  GtkWidget* window = CurrentWindow();
  if (window == nullptr) {
    std::printf("no window for click %s\n", label.c_str());
    return;
  }
  GtkWidget* button = FindButton(window, label);
  if (button == nullptr) {
    std::printf("no button %s in %s\n", label.c_str(), gtk_window_get_title(GTK_WINDOW(window)));
    return;
  }
  g_signal_emit_by_name(button, "clicked");
}


/// Runs the main loop for a while, so the worker's posted draws arrive and the window is
/// laid out before it is captured.
void Pump(int milliseconds) {
  const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(milliseconds);
  while (std::chrono::steady_clock::now() < deadline) {
    while (g_main_context_iteration(nullptr, FALSE)) {
    }
    std::this_thread::sleep_for(std::chrono::milliseconds(5));
  }
}

/// Runs the main loop until the flow has settled on one window: the window it replaces is
/// destroyed as the new one is presented, and a capture taken in between is of the dying
/// window.
void Settle() {
  for (int attempt = 0; attempt < 40; ++attempt) {
    Pump(50);
    GtkWidget* window = CurrentWindow();
    if (window == nullptr || !gtk_widget_get_mapped(window)) {
      continue;
    }
    GtkWidget* before = window;
    Pump(50);
    if (CurrentWindow() == before && gtk_widget_get_width(before) > 0) {
      return;
    }
  }
}

void Capture(const std::string& name) {
  GtkWidget* window = CurrentWindow();
  if (window == nullptr) {
    std::printf("no window to capture for %s\n", name.c_str());
    return;
  }
  const int width = gtk_widget_get_width(window);
  const int height = gtk_widget_get_height(window);
  if (width <= 0 || height <= 0) {
    std::printf("window %s has no size yet (%dx%d)\n", name.c_str(), width, height);
    return;
  }

  GtkSnapshot* snapshot = gtk_snapshot_new();
  GdkPaintable* paintable = gtk_widget_paintable_new(window);
  gdk_paintable_snapshot(paintable, snapshot, width, height);
  GskRenderNode* node = gtk_snapshot_free_to_node(snapshot);
  g_object_unref(paintable);

  cairo_surface_t* surface = cairo_image_surface_create(CAIRO_FORMAT_ARGB32, width, height);
  cairo_t* context = cairo_create(surface);
  if (node != nullptr) {
    gsk_render_node_draw(node, context);
    gsk_render_node_unref(node);
  }
  cairo_destroy(context);

  const std::string file = OutputDirectory() + "/" + PreviewPrefix() + name + ".png";
  const cairo_status_t status = cairo_surface_write_to_png(surface, file.c_str());
  cairo_surface_destroy(surface);
  std::printf("%-26s %s  %dx%d%s\n", name.c_str(), gtk_window_get_title(GTK_WINDOW(window)),
              width, height, status == CAIRO_STATUS_SUCCESS ? "" : "  (write failed)");
  std::fflush(stdout);
}

struct Step {
  const char* capture;
  std::function<void()> action;
};

}  // namespace

int main() {
  if (!gtk_init_check()) {
    std::fprintf(stderr, "no display: run this inside a session (or under Xvfb)\n");
    return 1;
  }
  if (PreviewDark()) {
    g_object_set(gtk_settings_get_default(), "gtk-application-prefer-dark-theme", TRUE, nullptr);
  }
  g_set_application_name("Dazzle Demo");

  const std::string directory = OutputDirectory();
  g_mkdir_with_parents(directory.c_str(), 0755);

  UiRunner* runner = NewRunner();

  g_port.notes = kNotes;
  g_port.size = 46000000;

  // Every entry drives one round; where a round has to start from preferences the runner
  // has not read yet, it starts from a fresh runner, which reads the store then.
  std::vector<Step> steps;

  // 1. the user asks, and the check hangs on the port so the state can be captured
  g_port.GateChecks();
  steps.push_back({"01-checking", [runner] { runner->CheckForUpdates(); }});
  steps.push_back({"02-available", [] { g_port.ReleaseChecks(); }});

  // 2. the download: gated first so the bar can be seen, then half full, then finished
  g_port.GateDownloads();
  steps.push_back({"03-downloading", [] { Click(Strings().install_update); }});
  steps.push_back({"04-progress-half", [] { g_port.progress = 0.5; }});
  steps.push_back({"05-ready", [] { g_port.ReleaseDownload(); }});

  // 3. the install: the window stays up through Installing, and a failed installer is
  //    what the Error state shows
  g_port.GateInstalls();
  steps.push_back({"06-installing", [] { Click(Strings().install_and_relaunch); }});
  steps.push_back({"07-error-install", [] {
                     g_port.install_ok = false;
                     g_port.ReleaseInstall();
                   }});

  // 4. up to date, from a runner whose round has not downloaded anything yet
  steps.push_back({"08-up-to-date", [] {
                     Click(Strings().ok);
                     g_port.ForgetDownload();
                     g_port.install_ok = true;
                     g_port.has_update = false;
                     NewRunner()->CheckForUpdates();
                   }});

  // 5. an error the window explains by its code
  steps.push_back({"09-error-signature", [] {
                     Click(Strings().ok);
                     g_port.ForgetDownload();
                     g_port.has_update = true;
                     g_port.check_ok = false;
                     g_port.error_code = "signature";
                     g_port.error_message = "signature mismatch: expected pfIShU4…, got 7Yb2Qx…";
                     NewRunner()->CheckForUpdates();
                   }});

  // 6. the first silent check, which asks before it does anything
  steps.push_back({"10-permission", [] {
                     Click(Strings().ok);
                     g_port.ForgetDownload();
                     g_port.check_ok = true;
                     g_store.preferences = UiPreferences{};
                     NewRunner()->CheckForUpdatesInBackground();
                   }});

  // 7. notes that are a link rather than a box
  steps.push_back({"11-available-notes-link", [] {
                     Click(Strings().dont_check);  // the permission prompt's way out
                     g_port.ForgetDownload();
                     g_port.notes.clear();
                     g_port.notes_url = "https://example.com/releases/2.6.1";
                     NewRunner()->CheckForUpdates();
                   }});

  // 8. automatic downloads: the first window the user sees offers the package
  steps.push_back({"12-downloaded", [] {
                     Click(Strings().later);
                     g_port.ForgetDownload();
                     UiPreferences preferences;
                     preferences.automatic_checks_answered = true;
                     preferences.automatic_checks = true;
                     preferences.automatic_downloads = true;
                     g_store.preferences = preferences;
                     g_port.notes = kNotes;
                     g_port.notes_url.clear();
                     g_port.ReleaseDownload();
                     NewRunner()->CheckForUpdatesInBackground();
                   }});

  for (const Step& step : steps) {
    step.action();
    Settle();
    if (step.capture != nullptr) {
      Capture(step.capture);
    }
  }
  return 0;
}
