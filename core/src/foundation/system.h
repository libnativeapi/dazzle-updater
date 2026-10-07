#pragma once

#include <string>

// The platform characteristics the cross-platform layer cannot know by itself: what the
// machine calls itself, which version of the application is running, where a download may
// go, and how an installer is started.
//
// This is a seam, not a class: the implementation lives in platform/<os>/system_<os>.*,
// and a platform that provides no file uses the cross-platform default
// (foundation/system_default.cpp), which reports that it cannot update itself. The build
// picks exactly one of the two (see src/CMakeLists.txt), so no platform needs a stub.
//
// The functions are called from Updater under its own lock, on the thread that started
// the action. They never throw.

namespace dazzle_updater {

/// Whether this platform can look for an update at all (see the availability table in
/// ../specs/updater.md).
bool SystemSupportsCheck();

/// Whether this platform can download and verify an installer.
bool SystemSupportsDownload();

/// Whether this platform can install a downloaded update.
bool SystemSupportsInstall();

/// The platform name a release index asset is matched by: "macos", "windows", "linux",
/// "ios", "android" or "ohos"; "" on a platform with no implementation.
std::string SystemName();

/// The machine's architecture: "x64", "arm64" or "x86"; "" when unknown.
std::string SystemArch();

/// The operating system version, e.g. "14.5" on macOS, "10.0.22631" on Windows; "" when
/// the platform cannot tell.
std::string SystemVersion();

/// The running application's version, read from the platform's own metadata; "" when the
/// platform has no such metadata.
std::string AppVersionFromPlatform();

/// The running application's name, read from the platform's own metadata; "" when the
/// platform has no such metadata. UpdaterUI uses it in its wording when the application
/// has set none of its own.
std::string AppNameFromPlatform();

/// A directory dazzle_updater may write downloads into, without a trailing separator; created
/// when missing. "" when the platform provides none.
std::string DownloadDirectory();

/// Starts the installer of a downloaded update whose signature the caller has already
/// verified. `format` is "zip", "exe" or "msi" and `installer_arguments` the release's
/// arguments. On false, fills `error` and `error_code` with "permission", "app-location"
/// or "installer".
bool InstallDownloadedUpdate(const std::string& downloaded_path, const std::string& format,
                             const std::string& installer_arguments, std::string* error,
                             std::string* error_code);

}  // namespace dazzle_updater
