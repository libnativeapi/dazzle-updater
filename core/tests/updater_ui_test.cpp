#include "test_support.h"

#include "../src/foundation/system.h"
#include "../src/foundation/updater_ui_preferences.h"
#include "../src/updater_ui_content.h"
#include "../src/updater_ui_runner.h"
#include "../src/updater_ui_state.h"
#include "../src/updater_ui_strings.h"

#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

// The state machine of specs/updater-ui.md, driven without a network, a package, an
// installer or a window: the port below is scripted by each test, and the backend keeps
// every state it was asked to draw. This is the "record every state transition with a
// fake Impl" of the spec's rollout step 1.
//
// Everything the module reaches outside itself is injected, so no test depends on the
// machine's language, its user preferences or Updater's real state.

using dazzle_updater::UiPreferences;
using dazzle_updater::UiPreferencesStore;
using dazzle_updater::internal::UiBackend;
using dazzle_updater::internal::UiContent;
using dazzle_updater::internal::UiEvent;
using dazzle_updater::internal::UiEventKind;
using dazzle_updater::internal::UiRunner;
using dazzle_updater::internal::UiState;
using dazzle_updater::internal::UiWindow;
using dazzle_updater::internal::UpdaterPort;

namespace {

constexpr auto kWait = std::chrono::seconds(5);

/// What a test's user has told UpdaterUI before the round starts.
class MemoryStore final : public UiPreferencesStore {
 public:
  UiPreferences Read() override {
    std::lock_guard<std::mutex> lock(mutex_);
    return preferences_;
  }

  void Write(const UiPreferences& preferences) override {
    std::lock_guard<std::mutex> lock(mutex_);
    preferences_ = preferences;
  }

 private:
  std::mutex mutex_;
  UiPreferences preferences_;
};

/// The Updater a test scripts.
///
/// `GateChecks()` and `GateDownloads()` hold one of the blocking calls on the worker
/// until `Release()` (or, for a download, `CancelDownload()`), which is what makes
/// "a second request adopts the running check" and "closing the window cancels the
/// download" observable.
class ScriptedPort final : public UpdaterPort {
 public:
  bool check_ok = true;
  bool has_update = true;
  bool download_ok = true;
  bool install_ok = true;
  std::string version = "1.2.3";
  std::string display_version = "1.2.3";
  std::string notes;
  std::string notes_url;
  int64_t size = 0;
  std::string error_code = "network";
  std::string error_message = "the update server is unreachable";
  std::string current_version = "1.0.0";

  std::atomic<int> checks{0};
  std::atomic<int> downloads{0};
  std::atomic<int> installs{0};
  std::atomic<int> cancels{0};
  std::atomic<double> progress{0.0};

  void GateChecks() {
    std::lock_guard<std::mutex> lock(gate_mutex_);
    gate_checks_ = true;
    check_started_ = false;
    check_released_ = false;
  }

  bool AwaitCheckStarted() {
    std::unique_lock<std::mutex> lock(gate_mutex_);
    return check_started_cv_.wait_for(lock, kWait, [this] { return check_started_; });
  }

  void Release() {
    std::lock_guard<std::mutex> lock(gate_mutex_);
    check_released_ = true;
    check_cv_.notify_all();
  }

  void GateDownloads() {
    std::lock_guard<std::mutex> lock(gate_mutex_);
    gate_downloads_ = true;
    download_started_ = false;
    download_released_ = false;
    download_cancelled_ = false;
  }

  bool AwaitDownloadStarted() {
    std::unique_lock<std::mutex> lock(gate_mutex_);
    return download_started_cv_.wait_for(lock, kWait, [this] { return download_started_; });
  }

  bool CheckForUpdates() override {
    checks.fetch_add(1);
    {
      std::unique_lock<std::mutex> lock(gate_mutex_);
      check_started_ = true;
      check_started_cv_.notify_all();
      if (gate_checks_) {
        check_cv_.wait(lock, [this] { return check_released_; });
      }
    }
    // A check clears what the last one downloaded, as Updater's does.
    downloaded_.store(false);
    return check_ok;
  }

  bool IsUpdateAvailable() const override { return check_ok && has_update; }
  std::string GetUpdateVersion() const override { return has_update ? version : std::string(); }
  std::string GetUpdateDisplayVersion() const override {
    return has_update ? display_version : std::string();
  }
  std::string GetUpdateReleaseNotes() const override { return has_update ? notes : std::string(); }
  std::string GetUpdateReleaseNotesUrl() const override {
    return has_update ? notes_url : std::string();
  }
  int64_t GetUpdateSize() const override { return has_update ? size : 0; }

  bool DownloadUpdate() override {
    downloads.fetch_add(1);
    {
      std::unique_lock<std::mutex> lock(gate_mutex_);
      download_started_ = true;
      download_started_cv_.notify_all();
      if (gate_downloads_) {
        download_cv_.wait(lock, [this] { return download_released_; });
      }
    }
    const bool downloaded = download_ok && !download_cancelled_.load();
    downloaded_.store(downloaded);
    progress.store(downloaded ? 1.0 : 0.0);
    return downloaded;
  }

  double GetDownloadProgress() const override { return progress.load(); }

  void CancelDownload() override {
    cancels.fetch_add(1);
    download_cancelled_.store(true);
    ReleaseDownload();  // Updater's cancel makes the blocked download return
  }

  bool IsUpdateDownloaded() const override { return downloaded_.load(); }

  bool InstallUpdate() override {
    installs.fetch_add(1);
    return install_ok;
  }

  std::string GetErrorCode() const override { return error_code; }
  std::string GetErrorMessage() const override { return error_message; }
  std::string GetCurrentVersion() const override { return current_version; }

  /// Lets the gated download finish.
  void ReleaseDownload() {
    std::lock_guard<std::mutex> lock(gate_mutex_);
    download_released_ = true;
    download_cv_.notify_all();
  }

 private:
  std::atomic<bool> downloaded_{false};
  std::atomic<bool> download_cancelled_{false};
  std::mutex gate_mutex_;
  std::condition_variable check_cv_;
  std::condition_variable check_started_cv_;
  std::condition_variable download_cv_;
  std::condition_variable download_started_cv_;
  bool gate_checks_ = false;
  bool check_started_ = false;
  bool check_released_ = false;
  bool gate_downloads_ = false;
  bool download_started_ = false;
  bool download_released_ = false;
};

/// The windows a test records: every state asked for, plus the calls that carry no
/// content (Hide, BringToFront, QuitApplication).
class RecordingBackend final : public UiBackend {
 public:
  bool available = true;

  bool IsAvailable() const override { return available; }

  void Draw(const UiContent& content) override {
    std::lock_guard<std::mutex> lock(mutex_);
    drawn_.push_back(content);
  }

  void Hide() override {
    std::lock_guard<std::mutex> lock(mutex_);
    ++hides_;
  }

  void BringToFront() override {
    std::lock_guard<std::mutex> lock(mutex_);
    ++fronts_;
  }

  void QuitApplication() override {
    std::lock_guard<std::mutex> lock(mutex_);
    ++quits_;
  }

  void SetEventSink(std::function<void(const UiEvent&)> sink) override {
    std::lock_guard<std::mutex> lock(mutex_);
    sink_ = std::move(sink);
  }

  std::vector<UiContent> All() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return drawn_;
  }

  int Draws() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return static_cast<int>(drawn_.size());
  }

  UiContent Last() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return drawn_.empty() ? UiContent{} : drawn_.back();
  }

  UiState LastState() const { return Last().state; }

  /// The first content drawn for `state`, or a default one when it was never drawn.
  UiContent Find(UiState state) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const UiContent& content : drawn_) {
      if (content.state == state) {
        return content;
      }
    }
    return UiContent{};
  }

  bool Saw(UiState state) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (const UiContent& content : drawn_) {
      if (content.state == state) {
        return true;
      }
    }
    return false;
  }

  int Hides() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return hides_;
  }

  int Fronts() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return fronts_;
  }

  int Quits() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return quits_;
  }

  /// Reports a button press the way a window's UI thread would.
  void Send(UiEventKind kind, bool flag = false) const {
    std::function<void(const UiEvent&)> sink;
    {
      std::lock_guard<std::mutex> lock(mutex_);
      sink = sink_;
    }
    if (sink) {
      UiEvent event;
      event.kind = kind;
      event.flag = flag;
      sink(event);
    }
  }

 private:
  mutable std::mutex mutex_;
  std::vector<UiContent> drawn_;
  std::function<void(const UiEvent&)> sink_;
  int hides_ = 0;
  int fronts_ = 0;
  int quits_ = 0;
};

/// One runner with everything it needs, for the length of a test.
class Fixture {
 public:
  Fixture()
      : strings(dazzle_updater::internal::UiStringsFor({"en"})),
        runner(new UiRunner(backend, port, store, strings)) {
    runner->SetAppName("Test App");
  }

  ~Fixture() {
    runner->WaitForIdle();
    delete runner;
  }

  UiPreferences Preferences() { return store.Read(); }

  /// Waits for the round to reach its last state, then returns what it drew last.
  UiContent Settle() {
    runner->WaitForIdle();
    return backend.Last();
  }

  dazzle_updater::internal::UiStrings strings;
  RecordingBackend backend;
  ScriptedPort port;
  MemoryStore store;
  UiRunner* runner;
};

/// The answer a user who already agreed to automatic checks has left behind.
UiPreferences Answered(bool automatic_downloads = false) {
  UiPreferences preferences;
  preferences.automatic_checks_answered = true;
  preferences.automatic_checks = true;
  preferences.automatic_downloads = automatic_downloads;
  return preferences;
}

std::vector<std::string> ButtonLabels(const UiContent& content) {
  std::vector<std::string> labels;
  for (const dazzle_updater::internal::UiButton& button : content.buttons) {
    labels.push_back(button.label);
  }
  return labels;
}

}  // namespace

// --- a check the user asked for -----------------------------------------------------

DAZZLE_TEST(updater_ui_user_check_shows_checking_then_the_available_window) {
  Fixture fixture;
  fixture.port.notes = "Fixes a crash.";
  fixture.port.GateChecks();
  EXPECT_TRUE(fixture.runner->CheckForUpdates());
  EXPECT_EQ(fixture.backend.Draws(), 1);
  EXPECT_EQ(static_cast<int>(fixture.backend.LastState()), static_cast<int>(UiState::kChecking));

  fixture.port.Release();
  const UiContent content = fixture.Settle();
  EXPECT_EQ(static_cast<int>(content.state), static_cast<int>(UiState::kAvailable));
  EXPECT_EQ(static_cast<int>(content.window), static_cast<int>(UiWindow::kUpdateAvailable));
  EXPECT_EQ(content.window_title, "Test App Update");
  EXPECT_EQ(content.heading, "A new version of Test App is available!");
  EXPECT_EQ(content.detail, "Test App 1.2.3 is now available—you have 1.0.0.");
  EXPECT_EQ(content.notes_markdown, "Fixes a crash.");
  EXPECT_TRUE(content.resizable);
  EXPECT_TRUE(content.checkbox.visible);  // automatic downloads are allowed, so it is offered
  EXPECT_FALSE(content.checkbox.checked);
  EXPECT_EQ(ButtonLabels(content).size(), static_cast<size_t>(3));
  EXPECT_EQ(content.buttons[0].label, "Skip This Version");
  EXPECT_EQ(content.buttons[1].label, "Later");
  EXPECT_EQ(content.buttons[2].label, "Install Update");
  EXPECT_TRUE(content.buttons[2].is_default);
}

DAZZLE_TEST(updater_ui_user_check_reports_being_up_to_date) {
  Fixture fixture;
  fixture.port.has_update = false;
  fixture.runner->CheckForUpdates();
  const UiContent content = fixture.Settle();
  EXPECT_EQ(static_cast<int>(content.state), static_cast<int>(UiState::kUpToDate));
  EXPECT_EQ(static_cast<int>(content.window), static_cast<int>(UiWindow::kUpdateStatus));
  EXPECT_EQ(content.window_title, "Software Update");
  EXPECT_EQ(content.detail, "Test App 1.0.0 is currently the newest version available.");
  EXPECT_EQ(content.buttons.size(), static_cast<size_t>(1));
  EXPECT_EQ(content.buttons[0].label, "OK");
}

DAZZLE_TEST(updater_ui_user_check_reports_the_error_that_failed) {
  Fixture fixture;
  fixture.port.check_ok = false;
  fixture.runner->CheckForUpdates();
  const UiContent content = fixture.Settle();
  EXPECT_EQ(static_cast<int>(content.state), static_cast<int>(UiState::kError));
  EXPECT_EQ(content.heading, "An error occurred while checking for updates.");
  EXPECT_EQ(content.detail,
            "Couldn’t connect to the update server. Check your internet connection and try again.");
  EXPECT_EQ(content.error_message, "the update server is unreachable");
  EXPECT_EQ(content.window_title, "Software Update");
  EXPECT_EQ(content.buttons[0].label, "OK");
}

DAZZLE_TEST(updater_ui_a_window_is_brought_to_the_front_instead_of_a_second_round) {
  Fixture fixture;
  fixture.runner->CheckForUpdates();
  fixture.Settle();
  EXPECT_EQ(fixture.port.checks.load(), 1);
  EXPECT_EQ(fixture.backend.Draws(), 2);  // Checking, then Available

  EXPECT_TRUE(fixture.runner->CheckForUpdates());
  EXPECT_EQ(fixture.port.checks.load(), 1);  // no new check
  EXPECT_EQ(fixture.backend.Fronts(), 1);
  EXPECT_EQ(fixture.backend.Draws(), 2);  // and nothing new drawn
}

DAZZLE_TEST(updater_ui_a_second_request_adopts_the_check_already_running) {
  Fixture fixture;
  fixture.store.Write(Answered());
  fixture.port.GateChecks();
  // A silent check first: it shows nothing, but it is running.
  EXPECT_TRUE(fixture.runner->CheckForUpdatesInBackground());
  EXPECT_TRUE(fixture.port.AwaitCheckStarted());
  EXPECT_EQ(fixture.backend.Draws(), 0);

  // The user asks while it runs: the window opens and adopts the answer.
  EXPECT_TRUE(fixture.runner->CheckForUpdates());
  EXPECT_EQ(static_cast<int>(fixture.backend.LastState()), static_cast<int>(UiState::kChecking));
  EXPECT_EQ(fixture.port.checks.load(), 1);

  fixture.port.Release();
  EXPECT_EQ(static_cast<int>(fixture.Settle().state), static_cast<int>(UiState::kAvailable));
  EXPECT_EQ(fixture.port.checks.load(), 1);
}

// --- download and install -----------------------------------------------------------

DAZZLE_TEST(updater_ui_install_update_downloads_and_offers_to_restart) {
  Fixture fixture;
  fixture.port.size = 46000000;
  fixture.port.progress.store(0.5);
  fixture.runner->CheckForUpdates();
  fixture.Settle();

  fixture.backend.Send(UiEventKind::kPrimary);  // Install Update
  fixture.runner->WaitForIdle();

  const UiContent downloading = fixture.backend.Find(UiState::kDownloading);
  EXPECT_EQ(static_cast<int>(downloading.window), static_cast<int>(UiWindow::kUpdateStatus));
  EXPECT_EQ(downloading.window_title, "Test App Update");
  EXPECT_TRUE(downloading.progress.visible);
  EXPECT_TRUE(downloading.progress.determinate);
  EXPECT_EQ(downloading.progress.status_text, "23.0 MB of 46.0 MB");
  EXPECT_EQ(downloading.buttons[0].label, "Cancel");

  const UiContent ready = fixture.backend.Last();
  EXPECT_EQ(static_cast<int>(ready.state), static_cast<int>(UiState::kReady));
  EXPECT_EQ(ready.heading, "Ready to Install");
  EXPECT_EQ(ready.buttons[0].label, "Later");
  EXPECT_EQ(ready.buttons[1].label, "Install and Relaunch");
  EXPECT_EQ(fixture.port.downloads.load(), 1);
}

DAZZLE_TEST(updater_ui_install_and_relaunch_starts_the_installer_and_quits) {
  Fixture fixture;
  fixture.runner->CheckForUpdates();
  fixture.Settle();
  fixture.backend.Send(UiEventKind::kPrimary);
  fixture.Settle();  // Ready

  fixture.backend.Send(UiEventKind::kPrimary);  // Install and Relaunch
  fixture.runner->WaitForIdle();
  const UiContent installing = fixture.backend.Last();
  EXPECT_EQ(static_cast<int>(installing.state), static_cast<int>(UiState::kInstalling));
  EXPECT_EQ(installing.heading, "Installing…");
  EXPECT_FALSE(installing.closable);
  EXPECT_EQ(installing.buttons.size(), static_cast<size_t>(0));
  EXPECT_EQ(fixture.port.installs.load(), 1);
  EXPECT_EQ(fixture.backend.Quits(), 1);
}

DAZZLE_TEST(updater_ui_a_failed_install_is_reported_and_the_application_stays) {
  Fixture fixture;
  fixture.port.install_ok = false;
  fixture.runner->CheckForUpdates();
  fixture.Settle();
  fixture.backend.Send(UiEventKind::kPrimary);  // download
  fixture.Settle();                             // Ready
  fixture.backend.Send(UiEventKind::kPrimary);  // install
  const UiContent content = fixture.Settle();
  EXPECT_EQ(static_cast<int>(content.state), static_cast<int>(UiState::kError));
  EXPECT_EQ(content.heading, "An error occurred while installing the update.");
  EXPECT_EQ(fixture.backend.Quits(), 0);
}

DAZZLE_TEST(updater_ui_a_failed_download_is_reported) {
  Fixture fixture;
  fixture.port.download_ok = false;
  fixture.runner->CheckForUpdates();
  fixture.Settle();
  fixture.backend.Send(UiEventKind::kPrimary);
  const UiContent content = fixture.Settle();
  EXPECT_EQ(static_cast<int>(content.state), static_cast<int>(UiState::kError));
  EXPECT_EQ(content.heading, "An error occurred while downloading the update.");
  EXPECT_EQ(fixture.backend.Quits(), 0);
}

DAZZLE_TEST(updater_ui_closing_the_window_while_downloading_cancels_the_download) {
  Fixture fixture;
  fixture.port.size = 1000;
  fixture.port.GateDownloads();
  fixture.runner->CheckForUpdates();
  fixture.Settle();
  fixture.backend.Send(UiEventKind::kPrimary);  // Install Update
  EXPECT_TRUE(fixture.port.AwaitDownloadStarted());
  EXPECT_EQ(static_cast<int>(fixture.backend.LastState()), static_cast<int>(UiState::kDownloading));

  fixture.backend.Send(UiEventKind::kCancel);  // closing the window means cancelling
  fixture.runner->WaitForIdle();
  EXPECT_EQ(fixture.port.cancels.load(), 1);
  EXPECT_EQ(fixture.backend.Hides(), 1);
  EXPECT_FALSE(fixture.runner->IsVisible());
  // A download the user stopped is not an error.
  EXPECT_FALSE(fixture.backend.Saw(UiState::kError));
}

DAZZLE_TEST(updater_ui_later_on_ready_keeps_the_package_for_the_process) {
  Fixture fixture;
  fixture.runner->CheckForUpdates();
  fixture.Settle();
  fixture.backend.Send(UiEventKind::kPrimary);  // download
  fixture.Settle();                             // Ready

  fixture.backend.Send(UiEventKind::kSecondary);  // Later
  fixture.runner->WaitForIdle();
  EXPECT_EQ(fixture.backend.Hides(), 1);

  const int checks = fixture.port.checks.load();
  fixture.runner->CheckForUpdates();
  const UiContent content = fixture.Settle();
  EXPECT_EQ(static_cast<int>(content.state), static_cast<int>(UiState::kDownloaded));
  EXPECT_EQ(content.heading, "A new version of Test App is ready to install!");
  EXPECT_EQ(content.detail,
            "Test App 1.2.3 has been downloaded. Would you like to install it and relaunch Test "
            "App now?");
  EXPECT_EQ(content.buttons[2].label, "Install and Relaunch");
  EXPECT_EQ(fixture.port.checks.load(), checks);  // shown without checking again
}

// --- skipping -----------------------------------------------------------------------

DAZZLE_TEST(updater_ui_skip_remembers_the_version_and_closes_the_window) {
  Fixture fixture;
  fixture.runner->CheckForUpdates();
  fixture.Settle();
  fixture.backend.Send(UiEventKind::kSkip);
  fixture.runner->WaitForIdle();

  EXPECT_EQ(fixture.Preferences().skipped_version, "1.2.3");
  EXPECT_EQ(fixture.backend.Hides(), 1);
  EXPECT_FALSE(fixture.runner->IsVisible());
}

DAZZLE_TEST(updater_ui_a_user_check_shows_a_skipped_version_anyway) {
  Fixture fixture;
  UiPreferences preferences = Answered();
  preferences.skipped_version = "1.2.3";
  fixture.store.Write(preferences);

  fixture.runner->CheckForUpdates();
  EXPECT_EQ(static_cast<int>(fixture.Settle().state), static_cast<int>(UiState::kAvailable));
}

DAZZLE_TEST(updater_ui_a_silent_check_hides_the_version_the_user_skipped) {
  Fixture fixture;
  UiPreferences preferences = Answered();
  preferences.skipped_version = "1.2.3";
  fixture.store.Write(preferences);

  EXPECT_TRUE(fixture.runner->CheckForUpdatesInBackground());
  fixture.runner->WaitForIdle();
  EXPECT_EQ(fixture.backend.Draws(), 0);
  EXPECT_EQ(fixture.Preferences().skipped_version, "1.2.3");
}

DAZZLE_TEST(updater_ui_a_newer_version_clears_the_skip) {
  Fixture fixture;
  UiPreferences preferences = Answered();
  preferences.skipped_version = "1.2.2";
  fixture.store.Write(preferences);

  fixture.runner->CheckForUpdatesInBackground();
  EXPECT_EQ(static_cast<int>(fixture.Settle().state), static_cast<int>(UiState::kAvailable));
  EXPECT_EQ(fixture.Preferences().skipped_version, "");
}

DAZZLE_TEST(updater_ui_skipping_a_downloaded_update_drops_it) {
  Fixture fixture;
  fixture.store.Write(Answered(/*automatic_downloads=*/true));
  fixture.runner->CheckForUpdatesInBackground();
  EXPECT_EQ(static_cast<int>(fixture.Settle().state), static_cast<int>(UiState::kDownloaded));

  fixture.backend.Send(UiEventKind::kSkip);
  fixture.runner->WaitForIdle();
  EXPECT_EQ(fixture.Preferences().skipped_version, "1.2.3");

  // The package is still in Updater's cache, but this module stops offering it: the next
  // check is a check, not the window it left behind. The check is held on the port, so the
  // window the check opens is the one drawn when this line runs — the worker would
  // otherwise be free to finish and draw the Available window first.
  fixture.port.GateChecks();
  fixture.runner->CheckForUpdates();
  EXPECT_EQ(static_cast<int>(fixture.backend.LastState()), static_cast<int>(UiState::kChecking));
  fixture.port.Release();
  EXPECT_EQ(static_cast<int>(fixture.Settle().state), static_cast<int>(UiState::kAvailable));
}

// --- silent checks ------------------------------------------------------------------

DAZZLE_TEST(updater_ui_a_silent_check_shows_nothing_when_there_is_no_update) {
  Fixture fixture;
  fixture.store.Write(Answered());
  fixture.port.has_update = false;
  EXPECT_TRUE(fixture.runner->CheckForUpdatesInBackground());
  fixture.runner->WaitForIdle();
  EXPECT_EQ(fixture.backend.Draws(), 0);
}

DAZZLE_TEST(updater_ui_a_silent_check_shows_nothing_when_it_fails) {
  Fixture fixture;
  fixture.store.Write(Answered());
  fixture.port.check_ok = false;
  fixture.runner->CheckForUpdatesInBackground();
  fixture.runner->WaitForIdle();
  EXPECT_EQ(fixture.backend.Draws(), 0);
}

DAZZLE_TEST(updater_ui_a_silent_check_does_nothing_when_the_user_said_no) {
  Fixture fixture;
  UiPreferences preferences;
  preferences.automatic_checks_answered = true;
  preferences.automatic_checks = false;
  fixture.store.Write(preferences);

  EXPECT_TRUE(fixture.runner->CheckForUpdatesInBackground());
  fixture.runner->WaitForIdle();
  EXPECT_EQ(fixture.port.checks.load(), 0);
  EXPECT_EQ(fixture.backend.Draws(), 0);
}

DAZZLE_TEST(updater_ui_a_silent_check_asks_before_the_first_one) {
  Fixture fixture;
  EXPECT_TRUE(fixture.runner->CheckForUpdatesInBackground());
  const UiContent content = fixture.backend.Last();
  EXPECT_EQ(static_cast<int>(content.state), static_cast<int>(UiState::kPermission));
  EXPECT_EQ(static_cast<int>(content.window), static_cast<int>(UiWindow::kUpdatePermission));
  EXPECT_EQ(content.window_title, "Software Update");
  EXPECT_EQ(content.heading, "Check for updates automatically?");
  EXPECT_EQ(content.detail,
            "Test App can check for new versions automatically. You can always check for updates "
            "manually from the Test App menu.");
  EXPECT_TRUE(content.checkbox.visible);
  EXPECT_FALSE(content.checkbox.checked);
  EXPECT_EQ(content.checkbox.label, "Automatically download and install updates");
  EXPECT_EQ(content.buttons[0].label, "Don’t Check");
  EXPECT_EQ(content.buttons[1].label, "Check Automatically");
  EXPECT_EQ(fixture.port.checks.load(), 0);  // nothing was checked yet

  // Check Automatically: the answer is stored and the check it was asked before runs.
  fixture.backend.Send(UiEventKind::kPrimary);
  const UiContent available = fixture.Settle();
  EXPECT_EQ(static_cast<int>(available.state), static_cast<int>(UiState::kAvailable));
  EXPECT_EQ(fixture.port.checks.load(), 1);
  const UiPreferences preferences = fixture.Preferences();
  EXPECT_TRUE(preferences.automatic_checks_answered);
  EXPECT_TRUE(preferences.automatic_checks);
  EXPECT_FALSE(preferences.automatic_downloads);
}

DAZZLE_TEST(updater_ui_dont_check_turns_silent_checks_off) {
  Fixture fixture;
  fixture.runner->CheckForUpdatesInBackground();
  fixture.backend.Send(UiEventKind::kSecondary);
  fixture.runner->WaitForIdle();

  const UiPreferences preferences = fixture.Preferences();
  EXPECT_TRUE(preferences.automatic_checks_answered);
  EXPECT_FALSE(preferences.automatic_checks);
  EXPECT_EQ(fixture.port.checks.load(), 0);
  EXPECT_FALSE(fixture.runner->IsAutomaticChecksEnabled());

  // The next silent check does nothing at all.
  fixture.runner->CheckForUpdatesInBackground();
  fixture.runner->WaitForIdle();
  EXPECT_EQ(fixture.port.checks.load(), 0);
  EXPECT_EQ(fixture.backend.Draws(), 1);  // only the question that was asked
}

DAZZLE_TEST(updater_ui_the_permission_prompt_keeps_the_automatic_downloads_answer) {
  Fixture fixture;
  fixture.runner->CheckForUpdatesInBackground();
  fixture.backend.Send(UiEventKind::kCheckboxChanged, true);
  fixture.backend.Send(UiEventKind::kPrimary);
  fixture.Settle();
  EXPECT_TRUE(fixture.Preferences().automatic_downloads);
  EXPECT_TRUE(fixture.runner->IsAutomaticDownloadsEnabled());
}

DAZZLE_TEST(updater_ui_automatic_downloads_fetch_before_the_user_is_told) {
  Fixture fixture;
  fixture.store.Write(Answered(/*automatic_downloads=*/true));
  fixture.runner->CheckForUpdatesInBackground();

  const UiContent content = fixture.Settle();
  EXPECT_EQ(static_cast<int>(content.state), static_cast<int>(UiState::kDownloaded));
  EXPECT_EQ(static_cast<int>(content.window), static_cast<int>(UiWindow::kUpdateAvailable));
  EXPECT_EQ(content.heading, "A new version of Test App is ready to install!");
  EXPECT_EQ(content.buttons[2].label, "Install and Relaunch");
  EXPECT_EQ(fixture.port.downloads.load(), 1);
  EXPECT_EQ(fixture.backend.Draws(), 1);  // the download in between showed nothing
  EXPECT_TRUE(fixture.runner->IsVisible());
}

DAZZLE_TEST(updater_ui_a_user_check_during_a_silent_download_shows_its_progress) {
  Fixture fixture;
  fixture.store.Write(Answered(/*automatic_downloads=*/true));
  fixture.port.size = 1000;
  fixture.port.GateDownloads();
  fixture.runner->CheckForUpdatesInBackground();
  EXPECT_TRUE(fixture.port.AwaitDownloadStarted());
  EXPECT_EQ(fixture.backend.Draws(), 0);  // so far the user has seen nothing
  EXPECT_EQ(fixture.port.checks.load(), 1);

  // The user asks now: the window shows the download that is already running.
  EXPECT_TRUE(fixture.runner->CheckForUpdates());
  EXPECT_EQ(static_cast<int>(fixture.backend.LastState()), static_cast<int>(UiState::kDownloading));
  EXPECT_EQ(fixture.port.checks.load(), 1);  // no second check
  EXPECT_EQ(fixture.port.downloads.load(), 1);

  fixture.port.ReleaseDownload();
  EXPECT_EQ(static_cast<int>(fixture.Settle().state), static_cast<int>(UiState::kDownloaded));
}

DAZZLE_TEST(updater_ui_a_silent_check_waits_for_the_silent_download_before_it) {
  Fixture fixture;
  fixture.store.Write(Answered(/*automatic_downloads=*/true));
  fixture.port.GateDownloads();
  fixture.runner->CheckForUpdatesInBackground();
  EXPECT_TRUE(fixture.port.AwaitDownloadStarted());

  EXPECT_TRUE(fixture.runner->CheckForUpdatesInBackground());
  // Nothing new was started: the round already has a download running, and the call
  // returns without waiting for it.
  EXPECT_EQ(fixture.port.checks.load(), 1);
  EXPECT_EQ(fixture.port.downloads.load(), 1);
  fixture.port.ReleaseDownload();
  fixture.runner->WaitForIdle();
}

DAZZLE_TEST(updater_ui_a_failed_silent_download_shows_nothing) {
  Fixture fixture;
  fixture.store.Write(Answered(/*automatic_downloads=*/true));
  fixture.port.download_ok = false;
  fixture.runner->CheckForUpdatesInBackground();
  fixture.runner->WaitForIdle();
  EXPECT_EQ(fixture.backend.Draws(), 0);
}

DAZZLE_TEST(updater_ui_a_silent_check_does_nothing_while_a_window_is_up) {
  Fixture fixture;
  fixture.store.Write(Answered());
  fixture.runner->CheckForUpdates();
  fixture.Settle();
  const int draws = fixture.backend.Draws();

  EXPECT_TRUE(fixture.runner->CheckForUpdatesInBackground());
  fixture.runner->WaitForIdle();
  EXPECT_EQ(fixture.backend.Draws(), draws);
  EXPECT_EQ(fixture.port.checks.load(), 1);
}

// --- what the application controls --------------------------------------------------

DAZZLE_TEST(updater_ui_the_application_can_turn_automatic_checks_on) {
  Fixture fixture;
  EXPECT_FALSE(fixture.runner->IsAutomaticChecksEnabled());
  fixture.runner->SetAutomaticChecksEnabled(true);
  EXPECT_TRUE(fixture.runner->IsAutomaticChecksEnabled());

  // Answered by the application: the silent check runs without asking.
  fixture.runner->CheckForUpdatesInBackground();
  EXPECT_EQ(static_cast<int>(fixture.Settle().state), static_cast<int>(UiState::kAvailable));
  EXPECT_EQ(fixture.port.checks.load(), 1);
  EXPECT_FALSE(fixture.backend.Saw(UiState::kPermission));
}

DAZZLE_TEST(updater_ui_the_application_can_refuse_automatic_downloads) {
  Fixture fixture;
  fixture.runner->SetAutomaticDownloadsAllowed(false);
  EXPECT_FALSE(fixture.runner->IsAutomaticDownloadsAllowed());
  fixture.runner->SetAutomaticDownloadsEnabled(true);
  // The application's refusal wins over what the user asked for.
  EXPECT_FALSE(fixture.runner->IsAutomaticDownloadsEnabled());

  fixture.runner->CheckForUpdates();
  const UiContent content = fixture.Settle();
  EXPECT_EQ(static_cast<int>(content.state), static_cast<int>(UiState::kAvailable));
  EXPECT_FALSE(content.checkbox.visible);  // nothing to tick in the first place
}

DAZZLE_TEST(updater_ui_a_silent_check_does_not_download_when_the_application_refuses) {
  Fixture fixture;
  fixture.runner->SetAutomaticDownloadsAllowed(false);
  fixture.store.Write(Answered(/*automatic_downloads=*/true));

  fixture.runner->CheckForUpdatesInBackground();
  const UiContent content = fixture.Settle();
  EXPECT_EQ(static_cast<int>(content.state), static_cast<int>(UiState::kAvailable));
  EXPECT_FALSE(content.checkbox.visible);
  EXPECT_EQ(fixture.port.downloads.load(), 0);
}

DAZZLE_TEST(updater_ui_a_platform_without_windows_answers_false_and_shows_nothing) {
  Fixture fixture;
  fixture.backend.available = false;
  EXPECT_FALSE(fixture.runner->CheckForUpdates());
  EXPECT_FALSE(fixture.runner->CheckForUpdatesInBackground());
  fixture.runner->WaitForIdle();
  EXPECT_EQ(fixture.backend.Draws(), 0);
  EXPECT_EQ(fixture.port.checks.load(), 0);
}

DAZZLE_TEST(updater_ui_the_app_name_defaults_to_the_platform) {
  Fixture fixture;
  EXPECT_EQ(fixture.runner->GetAppName(), "Test App");
  fixture.runner->SetAppName("");
  EXPECT_EQ(fixture.runner->GetAppName(), dazzle_updater::AppNameFromPlatform());
}
