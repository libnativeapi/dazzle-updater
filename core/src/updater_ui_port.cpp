#include "updater_ui_port.h"

#include "updater.h"

// The port UpdaterUI uses in a real build: every call is an Updater method with the same
// meaning. Internal (see updater_ui_port.h); tests substitute their own.

namespace dazzle_updater {
namespace internal {
namespace {

class RealUpdaterPort final : public UpdaterPort {
 public:
  bool CheckForUpdates() override { return Updater::GetInstance().CheckForUpdates(); }
  bool IsUpdateAvailable() const override { return Updater::GetInstance().IsUpdateAvailable(); }
  std::string GetUpdateVersion() const override { return Updater::GetInstance().GetUpdateVersion(); }
  std::string GetUpdateDisplayVersion() const override {
    return Updater::GetInstance().GetUpdateDisplayVersion();
  }
  std::string GetUpdateReleaseNotes() const override {
    return Updater::GetInstance().GetUpdateReleaseNotes();
  }
  std::string GetUpdateReleaseNotesUrl() const override {
    return Updater::GetInstance().GetUpdateReleaseNotesUrl();
  }
  int64_t GetUpdateSize() const override { return Updater::GetInstance().GetUpdateSize(); }

  bool DownloadUpdate() override { return Updater::GetInstance().DownloadUpdate(); }
  double GetDownloadProgress() const override { return Updater::GetInstance().GetDownloadProgress(); }
  void CancelDownload() override { Updater::GetInstance().CancelDownload(); }
  bool IsUpdateDownloaded() const override { return Updater::GetInstance().IsUpdateDownloaded(); }

  bool InstallUpdate() override { return Updater::GetInstance().InstallUpdate(); }

  std::string GetErrorCode() const override { return Updater::GetInstance().GetErrorCode(); }
  std::string GetErrorMessage() const override { return Updater::GetInstance().GetErrorMessage(); }
  std::string GetCurrentVersion() const override { return Updater::GetInstance().GetCurrentVersion(); }
};

}  // namespace

UpdaterPort& GetUpdaterPort() {
  static RealUpdaterPort port;
  return port;
}

}  // namespace internal
}  // namespace dazzle_updater
