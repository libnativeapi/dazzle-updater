#pragma once

#include <cstdint>
#include <string>

// The Updater methods the UpdaterUI state machine uses, behind an interface so that a
// test can drive every transition of ../specs/updater-ui.md without a network, a package
// or an installer. Internal, and never seen by the code generator.
//
// UpdaterUI adds no update logic: everything here is an existing Updater method, with
// the same meaning and the same error contract (../specs/updater.md).

namespace dazzle_updater {
namespace internal {

class UpdaterPort {
 public:
  virtual ~UpdaterPort() = default;

  virtual bool CheckForUpdates() = 0;
  virtual bool IsUpdateAvailable() const = 0;
  virtual std::string GetUpdateVersion() const = 0;
  virtual std::string GetUpdateDisplayVersion() const = 0;
  virtual std::string GetUpdateReleaseNotes() const = 0;
  virtual std::string GetUpdateReleaseNotesUrl() const = 0;
  virtual int64_t GetUpdateSize() const = 0;

  virtual bool DownloadUpdate() = 0;
  virtual double GetDownloadProgress() const = 0;
  virtual void CancelDownload() = 0;
  virtual bool IsUpdateDownloaded() const = 0;

  virtual bool InstallUpdate() = 0;

  virtual std::string GetErrorCode() const = 0;
  virtual std::string GetErrorMessage() const = 0;
  virtual std::string GetCurrentVersion() const = 0;
};

/// The port that forwards to Updater::GetInstance().
UpdaterPort& GetUpdaterPort();

}  // namespace internal
}  // namespace dazzle_updater
