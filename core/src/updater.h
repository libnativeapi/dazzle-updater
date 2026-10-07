#pragma once

#include <cstdint>
#include <mutex>
#include <string>

namespace dazzle_updater {

/**
 * @brief The application's software updater.
 *
 * Updater finds a new version through a data source, downloads and verifies it, and
 * starts the platform installer. It draws no UI and knows no data format: source
 * modules read the release index and, later, the Sparkle appcast, and Updater picks one
 * by name with SetSource().
 *
 * The three actions CheckForUpdates(), DownloadUpdate() and InstallUpdate() block: they
 * run to completion on the calling thread and report through their return value and the
 * getters below. Call them from a worker thread, not from the UI thread; the results can
 * then be read and shown on the UI thread. GetDownloadProgress() and CancelDownload()
 * may be called from any other thread.
 *
 * All methods are thread-safe, and the three actions are mutually exclusive: while one
 * is running, any call to one of them returns false with the `"busy"` error code.
 * DownloadUpdate() requires a check that found an update and InstallUpdate() requires a
 * verified download. Changing the source, or its configuration, while a check runs does
 * not affect that check. Nothing is persisted, so a new process checks again.
 *
 * Typical usage:
 * @code
 * dazzle_updater::ReleaseIndexSource::GetInstance().SetUrl("https://example.com/myapp/releases.json");
 *
 * auto& updater = dazzle_updater::Updater::GetInstance();
 * updater.SetPublicKey("pfIShU4dEXqPd5ObYNfDBiQWcXozk7estwzTnF9BamQ=");
 *
 * if (updater.CheckForUpdates() && updater.IsUpdateAvailable()) {
 *   // Show GetUpdateDisplayVersion() and GetUpdateReleaseNotes(), then:
 *   if (updater.DownloadUpdate() && updater.InstallUpdate()) {
 *     // Quit the application so the installer can replace it.
 *   }
 * }
 * @endcode
 */
class Updater {
 public:
  /**
   * @brief Get the singleton instance of Updater.
   *
   * @return Reference to the singleton Updater instance
   */
  static Updater& GetInstance();

  /**
   * @brief Set the data source to check for updates.
   *
   * The source is chosen by its registered name. "release-index" is the default and
   * reads dazzle_updater's own release index; the Sparkle-compatible module, once implemented,
   * registers itself as "appcast".
   *
   * @param name The name of a registered source, e.g. "release-index"; "" restores the
   *   default. An unregistered name is accepted here but makes CheckForUpdates() fail
   *   with the `"invalid-index"` error code.
   */
  void SetSource(const std::string& name);

  /**
   * @brief Get the data source to check for updates.
   *
   * @return The name set with SetSource(), or "release-index" if none has been set.
   */
  std::string GetSource() const;

  /**
   * @brief Set the version of the running application.
   *
   * The source compares it with the versions in the data source. The default comes from
   * the platform: the main bundle's `CFBundleShortVersionString` on macOS and iOS, and
   * the main executable's `ProductVersion` VERSIONINFO on Windows; other platforms have
   * no default.
   *
   * @param version The current version, e.g. "1.4.0"; "" restores the platform default.
   */
  void SetCurrentVersion(const std::string& version);

  /**
   * @brief Get the version of the running application.
   *
   * @return The version set with SetCurrentVersion(), or the platform default, or "" if
   *   there is no default on this platform.
   */
  std::string GetCurrentVersion() const;

  /**
   * @brief Set the public key that installer signatures are verified against.
   *
   * The application ships the Ed25519 public key, Base64 encoded (32 bytes), and the
   * matching private key signs the installers. Verification is mandatory: with no key
   * set, DownloadUpdate() fails.
   *
   * @param public_key The Base64 encoded Ed25519 public key; "" clears it.
   */
  void SetPublicKey(const std::string& public_key);

  /**
   * @brief Get the public key that installer signatures are verified against.
   *
   * @return The key set with SetPublicKey(), or "" if none has been set.
   */
  std::string GetPublicKey() const;

  /**
   * @brief Find the newest update offered to this machine.
   *
   * Queries the selected source, then keeps the highest release that applies to this
   * machine and is newer than GetCurrentVersion(). Blocks until the query is done. Each
   * call clears the previous result, including a downloaded installer.
   *
   * @return true if the query completed, with or without an update; false if it failed,
   *   in which case GetErrorMessage() says why.
   *
   * @note Platform availability:
   * macOS: ✅
   * Windows: ✅
   * Linux: ✅
   * iOS: ✅ (finding an update only; guide the user to the App Store or TestFlight)
   * Android: ❌ (returns false, GetErrorMessage() is "not supported on this platform")
   * OHOS: ❌ (returns false, GetErrorMessage() is "not supported on this platform")
   */
  bool CheckForUpdates();

  /**
   * @brief Whether the last check found an update.
   *
   * @return true if CheckForUpdates() found a release newer than GetCurrentVersion().
   */
  bool IsUpdateAvailable() const;

  /**
   * @brief The version of the update found by the last check.
   *
   * @return The version used for comparison, e.g. "1.4.0"; "" if IsUpdateAvailable() is
   *   false.
   */
  std::string GetUpdateVersion() const;

  /**
   * @brief The version of the update to show to the user.
   *
   * @return The release's display version, or GetUpdateVersion() when the release does
   *   not set one; "" if IsUpdateAvailable() is false.
   */
  std::string GetUpdateDisplayVersion() const;

  /**
   * @brief The release notes of the update found by the last check.
   *
   * Markdown. A source whose format carries another markup (the Sparkle appcast's HTML)
   * converts it before the release reaches Updater, so there is one format here.
   *
   * @return The release notes; "" if the release has none or IsUpdateAvailable() is
   *   false.
   */
  std::string GetUpdateReleaseNotes() const;

  /**
   * @brief The web address of the full release notes.
   *
   * @return The address, e.g. "https://example.com/myapp/changelog#1.4.0"; "" if the
   *   release has none or IsUpdateAvailable() is false.
   */
  std::string GetUpdateReleaseNotesUrl() const;

  /**
   * @brief The size of the update's installer.
   *
   * @return The installer size in bytes, as announced by the source; 0 if it is unknown
   *   or IsUpdateAvailable() is false.
   */
  int64_t GetUpdateSize() const;

  /**
   * @brief Download the update found by the last check and verify its signature.
   *
   * Blocks until the installer is in dazzle_updater's cache directory and its Ed25519 signature
   * has been verified against GetPublicKey(). Call CancelDownload() from another thread
   * to stop it. Fails, and deletes the file, when no public key is set, when the release
   * carries no signature, or when the signature does not match.
   *
   * @return true if the installer is downloaded and its signature is valid; false
   *   otherwise, including when the download was cancelled. GetErrorMessage() says why.
   *
   * @note Platform availability:
   * macOS: ✅
   * Windows: ✅
   * Linux: ✅
   * iOS: ❌ (returns false, GetErrorMessage() is "not supported on this platform")
   * Android: ❌ (returns false, GetErrorMessage() is "not supported on this platform")
   * OHOS: ❌ (returns false, GetErrorMessage() is "not supported on this platform")
   */
  bool DownloadUpdate();

  /**
   * @brief The progress of the download started by DownloadUpdate().
   *
   * May be called from any thread, e.g. from a UI timer while DownloadUpdate() blocks on
   * another thread.
   *
   * @return The downloaded fraction, from 0.0 to 1.0; -1.0 while the total size is
   *   unknown; 0.0 before a download starts. It stays at 1.0 after the download
   *   finished, and a failed download resets it to 0.0.
   */
  double GetDownloadProgress() const;

  /**
   * @brief Stop the download started by DownloadUpdate().
   *
   * May be called from any thread. DownloadUpdate() then returns false with the
   * `"cancelled"` error code. Does nothing when no download is running.
   */
  void CancelDownload();

  /**
   * @brief Whether a verified update is ready to install.
   *
   * @return true after DownloadUpdate() returned true, until CheckForUpdates() clears
   *   the result.
   */
  bool IsUpdateDownloaded() const;

  /**
   * @brief Start installing the downloaded update.
   *
   * Blocks until the installer has been started. The caller must quit the process as
   * soon as this returns true: the installer waits for this process to exit, then
   * replaces the application and starts it again (on Windows the installer decides
   * whether to start it). dazzle_updater never quits the application itself, so that each UI
   * framework can run its own shutdown, saving documents and closing windows.
   *
   * @return true if the installer was started; false if it was not and the application
   *   should keep running, in which case GetErrorMessage() says why.
   *
   * @note Platform availability:
   * macOS: ✅ (the downloaded zip holds the new .app)
   * Windows: ✅ (an exe or msi installer runs with the release's installer arguments)
   * Linux: ❌ (returns false, GetErrorMessage() is "not supported on this platform")
   * iOS: ❌ (returns false, GetErrorMessage() is "not supported on this platform")
   * Android: ❌ (returns false, GetErrorMessage() is "not supported on this platform")
   * OHOS: ❌ (returns false, GetErrorMessage() is "not supported on this platform")
   */
  bool InstallUpdate();

  /**
   * @brief The error message of the last failed action.
   *
   * English, for logs and for showing to the user as-is; an interface that localizes
   * should switch on GetErrorCode() instead.
   *
   * @return The message, e.g. "another operation is in progress"; "" when the last
   *   action succeeded.
   */
  std::string GetErrorMessage() const;

  /**
   * @brief The error code of the last failed action.
   *
   * A stable string, so an interface can pick text for the reason: "busy" (another
   * action is in progress), "unsupported-platform", "no-update" (the action was called
   * before the state it needs), "unknown-source" (SetSource() named a source that is not
   * registered), "invalid-version" (the current version does not follow the version
   * syntax), "network", "invalid-index", "signature", "cancelled", "permission",
   * "app-location" or "installer". New codes may be added; existing codes keep their
   * meaning.
   *
   * @return The error code; "" when the last action succeeded.
   */
  std::string GetErrorCode() const;

  // Prevent copying and moving
  Updater(const Updater&) = delete;
  Updater& operator=(const Updater&) = delete;
  Updater(Updater&&) = delete;
  Updater& operator=(Updater&&) = delete;

 private:
  Updater();
  ~Updater();

  /// Forgets the result of the last check, the last download and the last error. The
  /// caller holds mutex_.
  void ClearResultLocked();

  mutable std::mutex mutex_;

  // Configuration, as set through the public API.
  std::string source_;
  std::string current_version_;  // "" means the platform default.
  std::string public_key_;

  // The result of the last successful check, the download state, and the last error.
  // The release fields are a flattened copy of the source's internal Release, so that
  // this header stays free of internal types. Guarded by mutex_.
  bool update_available_ = false;
  std::string update_version_;
  std::string update_display_version_;
  std::string update_release_notes_;
  std::string update_release_notes_url_;
  std::string update_download_url_;
  std::string update_signature_;
  std::string update_format_;
  std::string update_installer_arguments_;
  int64_t update_size_ = 0;

  bool update_downloaded_ = false;
  std::string downloaded_path_;
  double download_progress_ = 0.0;
  bool cancel_requested_ = false;
  bool action_running_ = false;

  std::string error_message_;
  std::string error_code_;
};

}  // namespace dazzle_updater
