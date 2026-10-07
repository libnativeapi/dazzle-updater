#include "../updater_ui_state.h"
#include "updater_ui_backend.h"

// The cross-platform default for the window seam: no window at all.
//
// It is compiled in two cases — a platform that has no backend file (Linux, iOS,
// Android, OHOS) and a build with DAZZLE_WITH_UPDATER_UI=OFF — which is why UpdaterUI's
// entry points answer false the same way in both, without a single #ifdef in the
// sources. See src/CMakeLists.txt.

namespace dazzle {
namespace internal {
namespace {

class NoUiBackend final : public UiBackend {
 public:
  bool IsAvailable() const override { return false; }
  void Draw(const UiContent&) override {}
  void Hide() override {}
  void BringToFront() override {}
  void QuitApplication() override {}
  void SetEventSink(std::function<void(const UiEvent&)>) override {}
};

}  // namespace

UiBackend& GetUiBackend() {
  static NoUiBackend backend;
  return backend;
}

}  // namespace internal
}  // namespace dazzle
