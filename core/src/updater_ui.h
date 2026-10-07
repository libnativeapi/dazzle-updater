#pragma once

#include <string>

namespace dazzle {

/**
 * @brief The built-in update windows.
 *
 * Updater draws no UI, and an application that has its own interface can use Updater
 * directly. UpdaterUI is the optional module for applications that would rather not: it
 * walks the user through checking, reading the release notes, downloading and installing
 * with the platform's own windows (AppKit on macOS, Win32 on Windows).
 *
 * It is only a user of Updater: every release it shows and every action it takes comes
 * from Updater, and it adds no update logic of its own. Configure Updater's source and
 * public key as usual, then change the two entry points. The two can be mixed in one
 * application, but not at the same time: while UpdaterUI is running, calling Updater's
 * actions directly answers `"another operation is in progress"`, and the other way
 * round.
 *
 * Every method may be called from any thread and returns immediately — which is what
 * makes it usable from Flutter, Qt or Electron, whose code does not always run on the
 * platform's UI thread. Updater's blocking actions run on a thread of this module's own,
 * and the windows are opened on the platform's UI thread. This is the only module in
 * dazzle that owns a thread.
 *
 * What the user answers is remembered in the platform's user preferences
 * (`NSUserDefaults` on macOS, `HKCU` on Windows): whether to check automatically,
 * whether to download automatically, and which version to skip. Updater itself still
 * persists nothing.
 *
 * Typical usage:
 * @code
 * dazzle::ReleaseIndexSource::GetInstance().SetUrl("https://example.com/myapp/releases.json");
 * dazzle::Updater::GetInstance().SetPublicKey("pfIShU4dEXqPd5ObYNfDBiQWcXozk7estwzTnF9BamQ=");
 *
 * // After the application has started:
 * dazzle::UpdaterUI::GetInstance().CheckForUpdatesInBackground();
 *
 * // The "Check for Updates…" menu item:
 * dazzle::UpdaterUI::GetInstance().CheckForUpdates();
 * @endcode
 *
 * Building without the module: configure with `-DDAZZLE_WITH_UPDATER_UI=OFF`. The
 * windows, their wording and the Markdown renderer are then left out of the library
 * together with the system UI frameworks they need, and the entry points below answer
 * false, exactly as they do on a platform that has no built-in windows. No source file
 * changes, and the C ABI stays the same, so an application or binding built either way
 * links and runs.
 */
class UpdaterUI {
 public:
  /**
   * @brief Get the singleton instance of UpdaterUI.
   *
   * @return Reference to the singleton UpdaterUI instance
   */
  static UpdaterUI& GetInstance();

  /**
   * @brief Check for updates because the user asked, and show the whole flow.
   *
   * The window opens at once in the Checking state. An update, "you are up to date" and
   * an error are all reported in it, and the user can go on to download and install.
   * When a window of this module is already up, it is brought to the front instead of
   * starting a second round, and a check already running is adopted rather than
   * repeated: Updater's check cannot be interrupted, so cancelling only closes the
   * window and drops the answer.
   *
   * A version the user skipped is shown as usual: skipping applies to the silent checks
   * below.
   *
   * @return true when a round was started or brought forward; false when this platform
   *   has no built-in windows, or when the module was left out of the build
   *   (`DAZZLE_WITH_UPDATER_UI=OFF`), in which case nothing is shown and nothing is
   *   checked.
   *
   * @note Platform availability:
   * macOS: ✅ (AppKit)
   * Windows: ✅ (Win32 windows with the Fluent controls drawn by dazzle)
   * Linux: ❌ (returns false; the update flow cannot be completed there)
   * iOS: ❌ (returns false; send the user to the App Store or TestFlight instead)
   * Android: ❌ (returns false)
   * OHOS: ❌ (returns false)
   */
  bool CheckForUpdates();

  /**
   * @brief Check for updates silently, and show a window only when there is something to
   *   tell the user about.
   *
   * Meant for application start-up and for the application's own timer. Nothing is shown
   * when the check finds no update, fails, or finds the version the user skipped; and
   * nothing is shown when the user has turned automatic checks off. When an update is
   * found and automatic downloads are on, the download runs in the background and the
   * user's first window is the one offering to install it.
   *
   * While the user has never answered whether to check automatically, this asks first,
   * in a window of its own, and carries on with the check when the answer is yes. An
   * application that called SetAutomaticChecksEnabled() itself is never asked.
   *
   * When a window of this module is already up, or an update is already downloaded and
   * waiting, this does nothing.
   *
   * @return true when the call was handled; false when this platform has no built-in
   *   windows, or when the module was left out of the build
   *   (`DAZZLE_WITH_UPDATER_UI=OFF`).
   *
   * @note Platform availability:
   * macOS: ✅ (AppKit)
   * Windows: ✅ (Win32 windows with the Fluent controls drawn by dazzle)
   * Linux: ❌ (returns false; the update flow cannot be completed there)
   * iOS: ❌ (returns false; send the user to the App Store or TestFlight instead)
   * Android: ❌ (returns false)
   * OHOS: ❌ (returns false)
   */
  bool CheckForUpdatesInBackground();

  /**
   * @brief Whether one of the update windows is on screen.
   *
   * An application can use this to disable its "Check for Updates…" menu item while a
   * round is running.
   *
   * @return true while a window of this module is showing.
   */
  bool IsVisible() const;

  /**
   * @brief Set the application name the windows use in their wording.
   *
   * The default comes from the platform: the main bundle's `CFBundleName` on macOS, the
   * main executable's `ProductName` VERSIONINFO on Windows.
   *
   * @param name The name to show; "" restores the platform's own.
   */
  void SetAppName(const std::string& name);

  /**
   * @brief Get the application name the windows use in their wording.
   *
   * @return The name set with SetAppName(), or the platform's own when none was set.
   */
  std::string GetAppName() const;

  /**
   * @brief Whether updates may be checked for automatically.
   *
   * False both while the user has not answered the question yet and when the user said
   * no; CheckForUpdatesInBackground() does nothing either way. The answer the user gives
   * in the permission window and the value an application sets in its own settings go to
   * the same place, and calling this once is an answer: the user is not asked again.
   *
   * @param enabled Whether silent checks may run.
   */
  void SetAutomaticChecksEnabled(bool enabled);

  /**
   * @brief Whether the user agreed to automatic update checks.
   *
   * @return true when the user (or the application) said yes.
   */
  bool IsAutomaticChecksEnabled() const;

  /**
   * @brief Whether an update found by a silent check is downloaded before the user is
   *   told about it.
   *
   * With this on, the first window the user sees offers to install an already downloaded
   * update. Off by default.
   *
   * @param enabled Whether to download in the background.
   */
  void SetAutomaticDownloadsEnabled(bool enabled);

  /**
   * @brief Whether silent downloads are on.
   *
   * Always false while SetAutomaticDownloadsAllowed(false) is in effect, whatever
   * SetAutomaticDownloadsEnabled() was last given.
   *
   * @return true when silent checks download the update they find.
   */
  bool IsAutomaticDownloadsEnabled() const;

  /**
   * @brief Whether the application allows automatic downloads at all.
   *
   * False removes the automatic-downloads check box from every window, so the choice is
   * the application's alone. True by default.
   *
   * @param allowed Whether the windows may offer automatic downloads.
   */
  void SetAutomaticDownloadsAllowed(bool allowed);

  /**
   * @brief Whether the application allows automatic downloads.
   *
   * @return The value set with SetAutomaticDownloadsAllowed(), true if never set.
   */
  bool IsAutomaticDownloadsAllowed() const;

 private:
  UpdaterUI();
  ~UpdaterUI();
  UpdaterUI(const UpdaterUI&) = delete;
  UpdaterUI& operator=(const UpdaterUI&) = delete;
  UpdaterUI(UpdaterUI&&) = delete;
  UpdaterUI& operator=(UpdaterUI&&) = delete;
};

}  // namespace dazzle
