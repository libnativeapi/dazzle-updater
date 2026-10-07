#pragma once

#include <condition_variable>
#include <cstdint>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>

#include "foundation/updater_ui_preferences.h"
#include "updater_ui_port.h"
#include "updater_ui_state.h"
#include "updater_ui_strings.h"

// UpdaterUI's state machine, and the module's own worker thread.
//
// This is where ../specs/updater-ui.md's flow lives: every state, every button and
// every Updater call it makes, with no platform in sight. The backend draws what this
// decides, and the port performs the actions; both are injected, so a test drives the
// whole flow with a recording backend and a scripted port (core/tests/updater_ui_test.cpp).
// The public class (src/updater_ui.h) is a thin facade over one UiRunner.
//
// Threading (../specs/updater-ui.md, "线程"): every entry point locks, decides, asks the
// backend to draw and returns; anything that blocks — Updater's three actions — runs on
// the worker, one job at a time, so two runs can never overlap. While a download runs,
// the worker keeps the progress bar current instead of blocking on the download.
//
// The backend is called with the lock held, which is why a backend must never report an
// event from inside Draw/Hide/BringToFront/QuitApplication (see
// foundation/updater_ui_backend.h).

namespace dazzle_updater {
namespace internal {

class UiRunner {
 public:
  UiRunner(UiBackend& backend, UpdaterPort& port, UiPreferencesStore& preferences,
           const UiStrings& strings);

  /// Stops the worker thread and waits for the job in flight, if any. An application
  /// calls this when it wants the thread gone before exiting; the process normally
  /// leaves it running and quits by quitting the application.
  ~UiRunner();

  bool CheckForUpdates();
  bool CheckForUpdatesInBackground();
  bool IsVisible() const;

  void SetAppName(const std::string& name);
  std::string GetAppName() const;

  void SetAutomaticChecksEnabled(bool enabled);
  bool IsAutomaticChecksEnabled() const;
  void SetAutomaticDownloadsEnabled(bool enabled);
  bool IsAutomaticDownloadsEnabled() const;
  void SetAutomaticDownloadsAllowed(bool allowed);
  bool IsAutomaticDownloadsAllowed() const;
  /// Back to the answers of a first run, in memory and in the store.
  void ResetPreferences();

  /// Waits until every queued job has finished. The backend draws asynchronously, so
  /// this says nothing about the windows; tests use it to know that a scripted round has
  /// reached its last state.
  void WaitForIdle();

 private:
  using Job = std::function<void()>;

  // Entry points' bodies, for callers that already hold mutex_ (the event handler).
  bool CheckForUpdatesLocked();
  bool CheckForUpdatesInBackgroundLocked();
  void OnEventLocked(const UiEvent& event);

  void LoadPreferencesLocked() const;
  std::string AppNameLocked() const;
  UiContent ContentLocked(UiState state) const;

  /// Asks the backend to draw `state`'s content and remembers it as the state on screen.
  void ShowLocked(UiState state);
  void ShowErrorLocked(UiErrorStep step);
  void HideLocked();

  /// Starts a round over: the running check's answer, if there is one, belongs to
  /// nobody any more.
  void InvalidateRoundLocked();

  void StartCheckLocked(bool show_result);
  void StartDownloadLocked(bool silent);
  void StartInstallLocked();

  void EnqueueLocked(Job job);
  void WorkerMain();

  // The worker's jobs. They call the port without the lock, then lock to apply.
  void RunCheck();
  void RunDownload(uint64_t round, bool silent);
  void RunInstall(uint64_t round);

  UiBackend& backend_;
  UpdaterPort& port_;
  UiPreferencesStore& preferences_store_;
  /// The wording, resolved once by the caller: the state machine never asks the system
  /// itself, so a test can drive every state in one language.
  const UiStrings& strings_;

  mutable std::mutex mutex_;

  // Configuration and what the module remembers for the user.
  std::string app_name_;  // "" means the platform's own name
  bool automatic_downloads_allowed_ = true;
  /// Read from the platform's store on first use, then held; `mutable` because the
  /// const getters are the ones that may be the first to ask (see LoadPreferencesLocked).
  mutable UiPreferences preferences_;
  mutable bool preferences_loaded_ = false;
  /// The permission prompt's check box, until the user answers it.
  bool permission_downloads_ = false;

  // The round on screen: `visible_` is whether one of our windows is up, `state_` which
  // state it draws, and `round_` a token that invalidates a flow the user has left.
  bool visible_ = false;
  UiState state_ = UiState::kChecking;
  UiErrorStep error_step_ = UiErrorStep::kCheck;
  uint64_t round_ = 0;

  // The check in flight, if any: `check_round_` is the round whose answer is shown (0
  // when nobody wants it), and `check_show_result_` whether it is a user's check or a
  // silent one.
  bool check_in_flight_ = false;
  uint64_t check_round_ = 0;
  bool check_show_result_ = false;
  bool download_in_flight_ = false;

  // The package a user skipped while it was already downloaded: Updater keeps the file
  // until its next check clears it, so UpdaterUI stops offering it in the meantime.
  bool ignored_download_ = false;

  // The worker: one job at a time, started with the first job.
  std::thread worker_;
  bool worker_started_ = false;
  bool stopping_ = false;
  bool busy_ = false;
  std::deque<Job> jobs_;
  std::condition_variable job_cv_;
  std::condition_variable idle_cv_;
};

}  // namespace internal
}  // namespace dazzle_updater
