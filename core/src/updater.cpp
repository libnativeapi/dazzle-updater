#include "updater.h"

#include "ed25519.h"
#include "foundation/http.h"
#include "foundation/system.h"
#include "update_source.h"

#include <cstdio>
#include <string>

namespace dazzle {
namespace {

constexpr const char* kBusy = "another operation is in progress";
constexpr const char* kUnsupported = "not supported on this platform";
constexpr const char* kDefaultSource = "release-index";

/// Clears the "an action is running" flag when the action ends, however it ends.
class ActionScope {
 public:
  ActionScope(std::mutex& mutex, bool& running) : mutex_(mutex), running_(running) {}
  ~ActionScope() {
    std::lock_guard<std::mutex> lock(mutex_);
    running_ = false;
  }
  ActionScope(const ActionScope&) = delete;
  ActionScope& operator=(const ActionScope&) = delete;

 private:
  std::mutex& mutex_;
  bool& running_;
};

/// The last path component of a URL, reduced to characters that are safe in a file name,
/// for use as the download's name in the cache directory.
std::string FileNameFromUrl(const std::string& url) {
  std::string path = url;
  const size_t query = path.find_first_of("?#");
  if (query != std::string::npos) {
    path = path.substr(0, query);
  }
  const size_t slash = path.find_last_of('/');
  const std::string name = slash == std::string::npos ? path : path.substr(slash + 1);
  std::string safe;
  for (char character : name) {
    const bool allowed = (character >= 'a' && character <= 'z') ||
                         (character >= 'A' && character <= 'Z') ||
                         (character >= '0' && character <= '9') || character == '.' ||
                         character == '-' || character == '_';
    if (allowed) {
      safe.push_back(character);
    }
  }
  if (safe.empty() || safe[0] == '.') {
    return "update";
  }
  return safe;
}

}  // namespace

Updater::Updater() = default;

Updater::~Updater() = default;

Updater& Updater::GetInstance() {
  static Updater instance;
  return instance;
}

void Updater::ClearResultLocked() {
  update_available_ = false;
  update_version_.clear();
  update_display_version_.clear();
  update_release_notes_.clear();
  update_release_notes_url_.clear();
  update_download_url_.clear();
  update_signature_.clear();
  update_format_.clear();
  update_installer_arguments_.clear();
  update_size_ = 0;
  update_downloaded_ = false;
  downloaded_path_.clear();
  download_progress_ = 0.0;
  cancel_requested_ = false;
  error_message_.clear();
  error_code_.clear();
}

void Updater::SetSource(const std::string& name) {
  std::lock_guard<std::mutex> lock(mutex_);
  source_ = name;
}

std::string Updater::GetSource() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return source_.empty() ? std::string(kDefaultSource) : source_;
}

void Updater::SetCurrentVersion(const std::string& version) {
  std::lock_guard<std::mutex> lock(mutex_);
  current_version_ = version;
}

std::string Updater::GetCurrentVersion() const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (!current_version_.empty()) {
    return current_version_;
  }
  try {
    return AppVersionFromPlatform();
  } catch (...) {
    return "";
  }
}

void Updater::SetPublicKey(const std::string& public_key) {
  std::lock_guard<std::mutex> lock(mutex_);
  public_key_ = public_key;
}

std::string Updater::GetPublicKey() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return public_key_;
}

bool Updater::CheckForUpdates() {
  UpdateSource* source = nullptr;
  UpdateQuery query;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (action_running_) {
      error_message_ = kBusy;
      error_code_ = "busy";
      return false;
    }
    if (!SystemSupportsCheck()) {
      error_message_ = kUnsupported;
      error_code_ = "unsupported-platform";
      return false;
    }
    ClearResultLocked();
    const std::string name = source_.empty() ? std::string(kDefaultSource) : source_;
    source = FindUpdateSource(name);
    if (source == nullptr) {
      error_message_ = "no update source is registered as \"" + name + "\"";
      error_code_ = "unknown-source";
      return false;
    }
    // A snapshot: changing the source or its configuration during the query does not
    // affect this check. The public API never throws, so a failing platform seam is
    // reported like any other failure.
    try {
      query.current_version =
          current_version_.empty() ? AppVersionFromPlatform() : current_version_;
      query.platform = SystemName();
      query.arch = SystemArch();
      query.system_version = SystemVersion();
    } catch (...) {
      error_message_ = "the platform could not report the machine's version";
      error_code_ = "unsupported-platform";
      return false;
    }
    if (query.current_version.empty()) {
      error_message_ = "the current version is unknown; set it with SetCurrentVersion()";
      error_code_ = "invalid-version";
      return false;
    }
    action_running_ = true;
  }
  ActionScope scope(mutex_, action_running_);

  Release release;
  bool found = false;
  std::string error;
  std::string error_code;
  bool queried = false;
  try {
    queried = source->FindUpdate(query, &release, &found, &error, &error_code);
  } catch (...) {
    error = "the update source failed unexpectedly";
    error_code = "invalid-index";
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (!queried) {
    error_message_ = error.empty() ? "the update query failed" : error;
    error_code_ = error_code.empty() ? "invalid-index" : error_code;
    return false;
  }
  if (found) {
    update_available_ = true;
    update_version_ = release.version;
    update_display_version_ =
        release.display_version.empty() ? release.version : release.display_version;
    update_release_notes_ = release.release_notes;
    update_release_notes_url_ = release.release_notes_url;
    update_download_url_ = release.download_url;
    update_signature_ = release.signature;
    update_format_ = release.format;
    update_installer_arguments_ = release.installer_arguments;
    update_size_ = release.size;
  }
  return true;
}

bool Updater::IsUpdateAvailable() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return update_available_;
}

std::string Updater::GetUpdateVersion() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return update_available_ ? update_version_ : std::string();
}

std::string Updater::GetUpdateDisplayVersion() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return update_available_ ? update_display_version_ : std::string();
}

std::string Updater::GetUpdateReleaseNotes() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return update_available_ ? update_release_notes_ : std::string();
}

std::string Updater::GetUpdateReleaseNotesUrl() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return update_available_ ? update_release_notes_url_ : std::string();
}

int64_t Updater::GetUpdateSize() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return update_available_ ? update_size_ : 0;
}

bool Updater::DownloadUpdate() {
  std::string url;
  std::string signature;
  std::string public_key;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (action_running_) {
      error_message_ = kBusy;
      error_code_ = "busy";
      return false;
    }
    if (!update_available_) {
      error_message_ = "there is no update to download";
      error_code_ = "no-update";
      return false;
    }
    if (!SystemSupportsDownload()) {
      error_message_ = kUnsupported;
      error_code_ = "unsupported-platform";
      return false;
    }
    // Verification is mandatory, so both the key and the release's signature must exist.
    if (public_key_.empty()) {
      error_message_ = "no public key is set, so the update cannot be verified";
      error_code_ = "signature";
      return false;
    }
    if (update_signature_.empty()) {
      error_message_ = "the update carries no signature";
      error_code_ = "signature";
      return false;
    }
    if (update_download_url_.empty()) {
      error_message_ = "the update has no download url";
      error_code_ = "invalid-index";
      return false;
    }
    update_downloaded_ = false;
    downloaded_path_.clear();
    download_progress_ = 0.0;
    cancel_requested_ = false;
    error_message_.clear();
    error_code_.clear();
    url = update_download_url_;
    signature = update_signature_;
    public_key = public_key_;
    action_running_ = true;
  }
  ActionScope scope(mutex_, action_running_);

  std::string directory;
  try {
    directory = DownloadDirectory();
  } catch (...) {
    directory.clear();
  }
  if (directory.empty()) {
    std::lock_guard<std::mutex> lock(mutex_);
    error_message_ = "the platform provides no download directory";
    error_code_ = "permission";
    return false;
  }
  const std::string path = directory + "/" + FileNameFromUrl(url);

  auto progress = [this](int64_t received, int64_t total) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (cancel_requested_) {
      return false;
    }
    download_progress_ =
        total > 0 ? static_cast<double>(received) / static_cast<double>(total) : -1.0;
    return true;
  };

  HttpResult response;
  try {
    response = HttpClient::GetInstance().Get(url, path, progress);
  } catch (...) {
    response.ok = false;
    response.error = "the download failed unexpectedly";
    response.error_code = "network";
  }
  if (!response.ok) {
    std::lock_guard<std::mutex> lock(mutex_);
    download_progress_ = 0.0;
    error_message_ = response.error.empty() ? "the download failed" : response.error;
    error_code_ = response.error_code.empty() ? "network" : response.error_code;
    return false;
  }

  bool verified = false;
  try {
    verified = VerifyFileSignature(public_key, signature, path);
  } catch (...) {
    verified = false;
  }
  if (!verified) {
    std::lock_guard<std::mutex> lock(mutex_);
    download_progress_ = 0.0;
    error_message_ = "the downloaded update does not match its signature";
    error_code_ = "signature";
    std::remove(path.c_str());
    return false;
  }

  std::lock_guard<std::mutex> lock(mutex_);
  update_downloaded_ = true;
  downloaded_path_ = path;
  download_progress_ = 1.0;
  return true;
}

double Updater::GetDownloadProgress() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return download_progress_;
}

void Updater::CancelDownload() {
  std::lock_guard<std::mutex> lock(mutex_);
  cancel_requested_ = true;
}

bool Updater::IsUpdateDownloaded() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return update_downloaded_;
}

bool Updater::InstallUpdate() {
  std::string path;
  std::string format;
  std::string installer_arguments;
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (action_running_) {
      error_message_ = kBusy;
      error_code_ = "busy";
      return false;
    }
    if (!update_downloaded_) {
      error_message_ = "there is no downloaded update to install";
      error_code_ = "no-update";
      return false;
    }
    if (!SystemSupportsInstall()) {
      error_message_ = kUnsupported;
      error_code_ = "unsupported-platform";
      return false;
    }
    error_message_.clear();
    error_code_.clear();
    path = downloaded_path_;
    format = update_format_;
    installer_arguments = update_installer_arguments_;
    action_running_ = true;
  }
  ActionScope scope(mutex_, action_running_);

  std::string error;
  std::string error_code;
  bool installed = false;
  try {
    installed = InstallDownloadedUpdate(path, format, installer_arguments, &error, &error_code);
  } catch (...) {
    installed = false;
    error = "the installer failed unexpectedly";
    error_code = "installer";
  }

  std::lock_guard<std::mutex> lock(mutex_);
  if (!installed) {
    error_message_ = error.empty() ? "the installer could not be started" : error;
    error_code_ = error_code.empty() ? "installer" : error_code;
    return false;
  }
  return true;
}

std::string Updater::GetErrorMessage() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return error_message_;
}

std::string Updater::GetErrorCode() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return error_code_;
}

}  // namespace dazzle
