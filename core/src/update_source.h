#pragma once

#include <cstdint>
#include <string>

// Internal interface between Updater and the data sources. It is not listed in
// API_HEADERS, so codegen ignores it and it reaches no binding; applications cannot
// register their own source yet.
//
// See ../specs/updater.md ("内部接口：UpdateSource").

namespace dazzle_updater {

/**
 * @brief One version a data source offers, with its format removed.
 *
 * A source translates the entries of its own format into a Release; Updater only ever
 * sees this. Every field is empty (or 0) when the source has no value for it.
 */
struct Release {
  std::string version;              ///< The version used for comparison, e.g. "1.4.0".
  std::string display_version;      ///< Shown to the user; falls back to `version`.
  std::string release_notes;        ///< Markdown.
  std::string release_notes_url;    ///< Web address of the full release notes.
  std::string download_url;         ///< The installer to download.
  int64_t size = 0;                 ///< Installer size in bytes; 0 when unknown.
  std::string signature;            ///< Ed25519 over the installer bytes, Base64.
  std::string format;               ///< "zip" (macOS), "exe" or "msi" (Windows).
  std::string installer_arguments;  ///< Command line for an exe or msi installer.
};

/**
 * @brief What the calling machine looks like, filled in by Updater.
 *
 * It is everything a source needs to decide which release applies; the source never
 * asks the platform itself.
 */
struct UpdateQuery {
  std::string current_version;  ///< The running application's version.
  std::string platform;         ///< "macos", "windows", "linux", "ios", "android" or "ohos".
  std::string arch;             ///< "x64", "arm64" or "x86".
  std::string system_version;   ///< e.g. "14.5" on macOS, "10.0.22631" on Windows.
};

/**
 * @brief The seam between Updater and one data format.
 *
 * Each source module implements it on top of its own parser and registers the instance
 * with RegisterUpdateSource(). A source owns its format's semantics, such as which
 * entries apply to a machine and how versions compare.
 */
class UpdateSource {
 public:
  virtual ~UpdateSource() = default;

  /**
   * @brief Look the feed up and pick the release to offer this machine.
   *
   * Runs on the thread that called Updater::CheckForUpdates().
   *
   * @param query The calling machine and the version to compare against.
   * @param release Filled in when an update was found.
   * @param found Set to true when the query completed and the source found an update
   *   newer than `query.current_version`; set to false when the application is up to
   *   date. Either way the caller considers the check successful.
   * @param error Filled in with the reason when false is returned.
   * @param error_code Filled in with the stable code for that reason, from the list in
   *   Updater::GetErrorCode(). The source knows why its own format failed, and Updater
   *   must report it unchanged.
   * @return true if the query completed; false if it failed, with `error` saying why.
   */
  virtual bool FindUpdate(const UpdateQuery& query, Release* release, bool* found,
                          std::string* error, std::string* error_code) = 0;
};

/**
 * @brief Register a source under the name Updater::SetSource() takes.
 *
 * Source modules call it during static initialization.
 *
 * @param name The name the source is selected by, e.g. "release-index".
 * @param source The source instance; it must outlive the process.
 */
void RegisterUpdateSource(const std::string& name, UpdateSource* source);

/**
 * @brief The source registered under `name`, or nullptr when there is none.
 *
 * @param name The name passed to RegisterUpdateSource().
 * @return The registered source, or nullptr.
 */
UpdateSource* FindUpdateSource(const std::string& name);

}  // namespace dazzle_updater
