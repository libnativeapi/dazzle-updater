// Walks through one update: check, download, verify, and, with --install, install.
//
// Usage:
//   updater_example <release-index-url> [public-key] [current-version] [--install]
//
// The current version defaults to the platform's own (on macOS the main bundle's
// CFBundleShortVersionString), so this program can run from inside an application
// bundle and install the update it finds there. Without arguments it prints its usage
// and exits, which is what the install helper's relaunch does.
#include <dazzle.h>

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

namespace {

void PrintError(const dazzle::Updater& updater, const char* action) {
  std::cerr << action << " failed: " << updater.GetErrorCode() << ": "
            << updater.GetErrorMessage() << "\n";
}

}  // namespace

int main(int argc, char** argv) {
  std::vector<std::string> arguments(argv + 1, argv + argc);
  bool install = false;
  for (auto it = arguments.begin(); it != arguments.end();) {
    if (*it == "--install") {
      install = true;
      it = arguments.erase(it);
    } else {
      ++it;
    }
  }
  if (arguments.empty()) {
    // `make examples` runs this program without arguments, which prints the usage and
    // succeeds; a real run needs a release index to check.
    std::cerr << "usage: updater_example <release-index-url> [public-key] [current-version] "
                 "[--install]\n";
    return 0;
  }

  dazzle::ReleaseIndexSource::GetInstance().SetUrl(arguments[0]);

  dazzle::Updater& updater = dazzle::Updater::GetInstance();
  if (arguments.size() > 1 && !arguments[1].empty()) {
    updater.SetPublicKey(arguments[1]);
  }
  if (arguments.size() > 2 && !arguments[2].empty()) {
    updater.SetCurrentVersion(arguments[2]);
  }

  std::cout << "source:          " << updater.GetSource() << "\n";
  std::cout << "current version: " << updater.GetCurrentVersion() << "\n";

  if (!updater.CheckForUpdates()) {
    PrintError(updater, "check");
    return 1;
  }
  if (!updater.IsUpdateAvailable()) {
    std::cout << "up to date\n";
    return 0;
  }

  std::cout << "update:          " << updater.GetUpdateDisplayVersion() << " (" << updater.GetUpdateVersion()
            << ", " << updater.GetUpdateSize() << " bytes)\n";
  if (!updater.GetUpdateReleaseNotes().empty()) {
    std::cout << "release notes:\n" << updater.GetUpdateReleaseNotes() << "\n";
  }

  if (!updater.DownloadUpdate()) {
    PrintError(updater, "download");
    return 1;
  }
  std::cout << "downloaded and signature verified\n";

  if (!install) {
    std::cout << "pass --install to install it\n";
    return 0;
  }
  if (!updater.InstallUpdate()) {
    PrintError(updater, "install");
    return 1;
  }
  std::cout << "installer started; this process must now exit\n";
  return 0;
}
