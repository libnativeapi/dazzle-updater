#include "updater_ui_runner.h"

#include <chrono>
#include <future>
#include <utility>

#include "foundation/system.h"
#include "updater_ui_content.h"
#include "updater_ui_strings.h"

namespace dazzle_updater {
namespace internal {
namespace {

/// How often the progress bar is refreshed while a download runs. Updater's
/// GetDownloadProgress() may be read from any thread while DownloadUpdate() blocks, which
/// is what this loop is for (../specs/updater-ui.md, "窗口与流程").
constexpr int kProgressIntervalMs = 100;

}  // namespace

UiRunner::UiRunner(UiBackend& backend, UpdaterPort& port, UiPreferencesStore& preferences,
                   const UiStrings& strings)
    : backend_(backend), port_(port), preferences_store_(preferences), strings_(strings) {
  // The backend reports button presses back here, from its own UI thread.
  backend_.SetEventSink([this](const UiEvent& event) {
    std::lock_guard<std::mutex> lock(mutex_);
    OnEventLocked(event);
  });
}

UiRunner::~UiRunner() {
  {
    std::lock_guard<std::mutex> lock(mutex_);
    stopping_ = true;
  }
  job_cv_.notify_all();
  // Joined, and only after whatever was queued has run: the thread outlives no member of
  // this object. The module's own singleton is never destroyed — an application quits
  // through its platform, not through unwinding statics — so this is the path a test
  // takes, and a test's jobs are short.
  if (worker_.joinable()) {
    worker_.join();
  }
}

// --- what the application calls -----------------------------------------------------

bool UiRunner::CheckForUpdates() {
  std::lock_guard<std::mutex> lock(mutex_);
  return CheckForUpdatesLocked();
}

bool UiRunner::CheckForUpdatesInBackground() {
  std::lock_guard<std::mutex> lock(mutex_);
  return CheckForUpdatesInBackgroundLocked();
}

bool UiRunner::IsVisible() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return visible_;
}

void UiRunner::SetAppName(const std::string& name) {
  std::lock_guard<std::mutex> lock(mutex_);
  app_name_ = name;
}

std::string UiRunner::GetAppName() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return AppNameLocked();
}

void UiRunner::SetAutomaticChecksEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(mutex_);
  LoadPreferencesLocked();
  preferences_.automatic_checks_answered = true;
  preferences_.automatic_checks = enabled;
  preferences_store_.Write(preferences_);
}

bool UiRunner::IsAutomaticChecksEnabled() const {
  std::lock_guard<std::mutex> lock(mutex_);
  LoadPreferencesLocked();
  return preferences_.automatic_checks_answered && preferences_.automatic_checks;
}

void UiRunner::SetAutomaticDownloadsEnabled(bool enabled) {
  std::lock_guard<std::mutex> lock(mutex_);
  LoadPreferencesLocked();
  preferences_.automatic_downloads = enabled;
  preferences_store_.Write(preferences_);
}

bool UiRunner::IsAutomaticDownloadsEnabled() const {
  std::lock_guard<std::mutex> lock(mutex_);
  LoadPreferencesLocked();
  return automatic_downloads_allowed_ && preferences_.automatic_downloads;
}

void UiRunner::ResetPreferences() {
  std::lock_guard<std::mutex> lock(mutex_);
  preferences_ = UiPreferences{};
  preferences_loaded_ = true;  // these defaults are the answers now, not a stale read
  preferences_store_.Write(preferences_);
}

void UiRunner::SetAutomaticDownloadsAllowed(bool allowed) {
  std::lock_guard<std::mutex> lock(mutex_);
  automatic_downloads_allowed_ = allowed;
}

bool UiRunner::IsAutomaticDownloadsAllowed() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return automatic_downloads_allowed_;
}

// --- the flow ----------------------------------------------------------------------

bool UiRunner::CheckForUpdatesLocked() {
  LoadPreferencesLocked();
  if (!backend_.IsAvailable()) {
    return false;
  }
  if (visible_) {
    // A window is already up: the user gets it, not a second run.
    backend_.BringToFront();
    return true;
  }
  if (!ignored_download_ && port_.IsUpdateDownloaded()) {
    // Downloaded earlier in this process and kept for later: showing it again is what
    // the user asked for, and a new check would make Updater throw it away.
    InvalidateRoundLocked();
    ShowLocked(UiState::kDownloaded);
    return true;
  }
  if (download_in_flight_) {
    // An update is being downloaded for a round that had no window. The user asks now, so
    // the window appears with that download's progress instead of starting over.
    ShowLocked(UiState::kDownloading);
    return true;
  }
  if (check_in_flight_) {
    // A check is running: this round adopts its answer, and now it is a user's check, so
    // the answer is shown whatever it is.
    InvalidateRoundLocked();
    check_round_ = round_;
    check_show_result_ = true;
    ShowLocked(UiState::kChecking);
    return true;
  }
  StartCheckLocked(/*show_result=*/true);
  return true;
}

bool UiRunner::CheckForUpdatesInBackgroundLocked() {
  LoadPreferencesLocked();
  if (!backend_.IsAvailable()) {
    return false;
  }
  if (visible_) {
    return true;  // nothing to do: the user is already looking at this round
  }
  if (!ignored_download_ && port_.IsUpdateDownloaded()) {
    return true;  // the package is here; a silent check has nothing to add
  }
  if (!preferences_.automatic_checks_answered) {
    // The one-time question. The check this call was about carries on when the user
    // answers "Check Automatically" (see OnEventLocked).
    InvalidateRoundLocked();
    permission_downloads_ = false;
    ShowLocked(UiState::kPermission);
    return true;
  }
  if (!preferences_.automatic_checks) {
    return true;  // the user said no
  }
  if (download_in_flight_ || check_in_flight_) {
    return true;  // this round is already running a check or a silent download
  }
  StartCheckLocked(/*show_result=*/false);
  return true;
}

void UiRunner::StartCheckLocked(bool show_result) {
  InvalidateRoundLocked();
  check_in_flight_ = true;
  check_round_ = round_;
  check_show_result_ = show_result;
  if (show_result) {
    ShowLocked(UiState::kChecking);
  }
  EnqueueLocked([this] { RunCheck(); });
}

void UiRunner::StartDownloadLocked(bool silent) {
  download_in_flight_ = true;
  if (!silent) {
    ShowLocked(UiState::kDownloading);
  }
  const uint64_t round = round_;
  EnqueueLocked([this, round, silent] { RunDownload(round, silent); });
}

void UiRunner::StartInstallLocked() {
  ShowLocked(UiState::kInstalling);
  const uint64_t round = round_;
  EnqueueLocked([this, round] { RunInstall(round); });
}

void UiRunner::RunCheck() {
  const bool checked = port_.CheckForUpdates();  // blocks; the lock is not held

  std::lock_guard<std::mutex> lock(mutex_);
  check_in_flight_ = false;
  if (check_round_ != round_) {
    return;  // the window was closed, or a later round took over: the answer is dropped
  }
  if (!checked) {
    if (check_show_result_) {
      ShowErrorLocked(UiErrorStep::kCheck);
    }
    return;
  }
  if (!port_.IsUpdateAvailable()) {
    if (check_show_result_) {
      ShowLocked(UiState::kUpToDate);
    }
    return;
  }

  if (check_show_result_) {
    // A check the user asked for: every answer is shown, including a skipped version.
    ShowLocked(UiState::kAvailable);
    return;
  }

  // A silent check: only an update the user has not skipped is shown. Nothing is shown
  // when the version is the skipped one, and a newer version supersedes the skip.
  const std::string found = port_.GetUpdateVersion();
  if (!preferences_.skipped_version.empty()) {
    if (preferences_.skipped_version == found) {
      return;
    }
    preferences_.skipped_version.clear();
    preferences_store_.Write(preferences_);
  }
  if (automatic_downloads_allowed_ && preferences_.automatic_downloads) {
    StartDownloadLocked(/*silent=*/true);
    return;
  }
  ShowLocked(UiState::kAvailable);
}

void UiRunner::RunDownload(uint64_t round, bool silent) {
  // DownloadUpdate() blocks for as long as the package takes, so it runs on a thread of
  // its own while this one keeps the progress bar current.
  std::promise<bool> promise;
  std::future<bool> finished = promise.get_future();
  std::thread download([this, &promise] {
    bool downloaded = false;
    try {
      downloaded = port_.DownloadUpdate();
    } catch (...) {
      downloaded = false;
    }
    promise.set_value(downloaded);
  });

  while (finished.wait_for(std::chrono::milliseconds(kProgressIntervalMs)) !=
         std::future_status::ready) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (visible_ && round_ == round) {
      ShowLocked(UiState::kDownloading);  // same state: the bar and its text only
    }
  }
  const bool downloaded = finished.get();
  download.join();

  std::lock_guard<std::mutex> lock(mutex_);
  download_in_flight_ = false;
  if (round_ != round) {
    return;  // cancelled or closed: CancelDownload() makes the call return false
  }
  if (!downloaded) {
    // A download the user never saw — the silent one — fails quietly: there was no window
    // to report it in, and the spec's flow ends there (../specs/updater-ui.md).
    if (!silent) {
      ShowErrorLocked(UiErrorStep::kDownload);
    }
    return;
  }
  // The user's own download ends in the status window's Ready; a silent one was never
  // announced, so its first window is the Available one's Downloaded state.
  ignored_download_ = false;
  ShowLocked(silent ? UiState::kDownloaded : UiState::kReady);
}

void UiRunner::RunInstall(uint64_t round) {
  const bool installed = port_.InstallUpdate();  // blocks; the lock is not held

  std::lock_guard<std::mutex> lock(mutex_);
  if (round_ != round) {
    return;
  }
  if (!installed) {
    ShowErrorLocked(UiErrorStep::kInstall);
    return;
  }
  // The installer waits for this process to exit; the window stays up until it does.
  backend_.QuitApplication();
}

void UiRunner::OnEventLocked(const UiEvent& event) {
  // The check box reports its own change, whether or not the round went on.
  if (event.kind == UiEventKind::kCheckboxChanged) {
    if (state_ == UiState::kPermission) {
      permission_downloads_ = automatic_downloads_allowed_ && event.flag;
    } else if (visible_) {
      preferences_.automatic_downloads = automatic_downloads_allowed_ && event.flag;
      preferences_store_.Write(preferences_);
    }
    return;
  }
  if (!visible_) {
    return;  // an event for a window this round has already left
  }

  switch (state_) {
    case UiState::kPermission:
      switch (event.kind) {
        case UiEventKind::kPrimary:  // Check Automatically
          preferences_.automatic_checks_answered = true;
          preferences_.automatic_checks = true;
          preferences_.automatic_downloads = automatic_downloads_allowed_ && permission_downloads_;
          preferences_store_.Write(preferences_);
          HideLocked();
          CheckForUpdatesInBackgroundLocked();  // the silent check it was asked before
          break;
        case UiEventKind::kSecondary:  // Don't Check
        case UiEventKind::kCancel:
        case UiEventKind::kClose:
          preferences_.automatic_checks_answered = true;
          preferences_.automatic_checks = false;
          preferences_store_.Write(preferences_);
          HideLocked();
          break;
        default:
          break;
      }
      break;

    case UiState::kAvailable:
    case UiState::kDownloaded:
      switch (event.kind) {
        case UiEventKind::kPrimary:
          if (state_ == UiState::kAvailable) {
            StartDownloadLocked(/*silent=*/false);
          } else {
            StartInstallLocked();
          }
          break;
        case UiEventKind::kSkip:
          preferences_.skipped_version = port_.GetUpdateVersion();
          preferences_store_.Write(preferences_);
          // Skipping an update that is already downloaded also drops it: Updater keeps
          // the file until its next check, so this module stops offering it.
          ignored_download_ = state_ == UiState::kDownloaded;
          HideLocked();
          break;
        case UiEventKind::kSecondary:  // Later
        case UiEventKind::kCancel:
        case UiEventKind::kClose:
          // Later on Downloaded, and on Ready, keeps the package for the rest of the
          // process: Updater still has it, and CheckForUpdates() shows it again.
          HideLocked();
          break;
        default:
          break;
      }
      break;

    case UiState::kDownloading:
    case UiState::kChecking:
      switch (event.kind) {
        case UiEventKind::kCancel:
        case UiEventKind::kClose:
          if (state_ == UiState::kDownloading) {
            port_.CancelDownload();
          }
          // A check cannot be interrupted: it runs to the end and its answer is dropped.
          HideLocked();
          break;
        default:
          break;
      }
      break;

    case UiState::kReady:
      switch (event.kind) {
        case UiEventKind::kPrimary:
          StartInstallLocked();
          break;
        case UiEventKind::kSecondary:
        case UiEventKind::kCancel:
        case UiEventKind::kClose:
          HideLocked();
          break;
        default:
          break;
      }
      break;

    case UiState::kUpToDate:
    case UiState::kError:
      switch (event.kind) {
        case UiEventKind::kPrimary:
        case UiEventKind::kSecondary:
        case UiEventKind::kCancel:
        case UiEventKind::kClose:
          HideLocked();
          break;
        default:
          break;
      }
      break;

    case UiState::kInstalling:
      break;  // no buttons, and the window cannot be closed
  }
}

// --- drawing and bookkeeping --------------------------------------------------------

std::string UiRunner::AppNameLocked() const {
  return app_name_.empty() ? AppNameFromPlatform() : app_name_;
}

UiContent UiRunner::ContentLocked(UiState state) const {
  ContentInput input;
  input.strings = &strings_;
  input.state = state;
  input.error_step = error_step_;
  input.app_name = AppNameLocked();
  input.current_version = port_.GetCurrentVersion();
  input.update_version = port_.GetUpdateVersion();
  input.update_display_version = port_.GetUpdateDisplayVersion();
  input.release_notes = port_.GetUpdateReleaseNotes();
  input.release_notes_url = port_.GetUpdateReleaseNotesUrl();
  input.update_size = port_.GetUpdateSize();
  input.progress = port_.GetDownloadProgress();
  input.error_code = port_.GetErrorCode();
  input.error_message = port_.GetErrorMessage();
  input.show_automatic_downloads = automatic_downloads_allowed_;
  input.automatic_downloads_checked = state == UiState::kPermission
                                          ? permission_downloads_
                                          : (automatic_downloads_allowed_ &&
                                             preferences_.automatic_downloads);
  return BuildContent(input);
}

void UiRunner::ShowLocked(UiState state) {
  state_ = state;
  visible_ = true;
  backend_.Draw(ContentLocked(state));
}

void UiRunner::ShowErrorLocked(UiErrorStep step) {
  error_step_ = step;
  ShowLocked(UiState::kError);
}

void UiRunner::HideLocked() {
  InvalidateRoundLocked();
  visible_ = false;
  backend_.Hide();
}

void UiRunner::InvalidateRoundLocked() {
  ++round_;
  // A check that is still running now answers a round nobody is waiting for.
  check_round_ = 0;
}

void UiRunner::LoadPreferencesLocked() const {
  if (preferences_loaded_) {
    return;
  }
  preferences_ = preferences_store_.Read();
  preferences_loaded_ = true;
}

// --- the worker ---------------------------------------------------------------------

void UiRunner::EnqueueLocked(Job job) {
  jobs_.push_back(std::move(job));
  if (!worker_started_) {
    worker_started_ = true;
    worker_ = std::thread([this] { WorkerMain(); });
  }
  job_cv_.notify_one();
}

void UiRunner::WorkerMain() {
  for (;;) {
    Job job;
    {
      std::unique_lock<std::mutex> lock(mutex_);
      job_cv_.wait(lock, [this] { return stopping_ || !jobs_.empty(); });
      if (jobs_.empty()) {
        return;  // stopping_
      }
      job = std::move(jobs_.front());
      jobs_.pop_front();
      busy_ = true;
    }
    job();
    {
      std::lock_guard<std::mutex> lock(mutex_);
      busy_ = false;
    }
    idle_cv_.notify_all();
  }
}

void UiRunner::WaitForIdle() {
  std::unique_lock<std::mutex> lock(mutex_);
  idle_cv_.wait(lock, [this] { return jobs_.empty() && !busy_; });
}

}  // namespace internal
}  // namespace dazzle_updater
