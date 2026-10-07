#include "updater_ui_preferences.h"

#include <mutex>

// The cross-platform default for the preferences seam: the answer is kept in memory for
// the life of the process.
//
// A platform without its own store still behaves the same within one run: the user is
// asked once, the answer holds until the application exits, and nothing is written
// anywhere. UpdaterUI never persists anything itself, so this default is also what
// DAZZLE_WITH_UPDATER_UI=OFF compiles. Tests inject their own store for the same reason
// (see ../tests/updater_ui_test.cpp).

namespace dazzle {
namespace {

class MemoryPreferencesStore final : public UiPreferencesStore {
 public:
  UiPreferences Read() override {
    std::lock_guard<std::mutex> lock(mutex_);
    return preferences_;
  }

  void Write(const UiPreferences& preferences) override {
    std::lock_guard<std::mutex> lock(mutex_);
    preferences_ = preferences;
  }

 private:
  std::mutex mutex_;
  UiPreferences preferences_;
};

}  // namespace

UiPreferencesStore& GetUiPreferencesStore() {
  static MemoryPreferencesStore store;
  return store;
}

std::vector<std::string> UiPreferredLanguages() {
  return {};  // the windows are worded in English
}

}  // namespace dazzle
