#include "system.h"

// The cross-platform default for the system seam: this machine cannot update itself. It
// is compiled only when platform/<os>/ has no system_<os> file, so a platform that never
// implements updates needs no source file at all (see ../CMakeLists.txt). The error text
// and code match the availability table in ../../../specs/updater.md.

namespace dazzle {
namespace {

constexpr const char* kUnsupported = "not supported on this platform";

}  // namespace

bool SystemSupportsCheck() {
  return false;
}

bool SystemSupportsDownload() {
  return false;
}

bool SystemSupportsInstall() {
  return false;
}

std::string SystemName() {
  return "";
}

std::string SystemArch() {
  return "";
}

std::string SystemVersion() {
  return "";
}

std::string AppVersionFromPlatform() {
  return "";
}

std::string DownloadDirectory() {
  return "";
}

bool InstallDownloadedUpdate(const std::string&, const std::string&, const std::string&,
                             std::string* error, std::string* error_code) {
  if (error != nullptr) {
    *error = kUnsupported;
  }
  if (error_code != nullptr) {
    *error_code = "unsupported-platform";
  }
  return false;
}

}  // namespace dazzle
