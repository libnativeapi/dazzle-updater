#include "updater_ui.h"

#include "foundation/updater_ui_backend.h"
#include "updater_ui_port.h"
#include "updater_ui_runner.h"
#include "updater_ui_strings.h"

// The public class is a facade: it owns nothing, and forwards to the one state machine,
// which owns the module's worker thread and its memory. The state machine is not
// destroyed — an application quits through its own platform (see the note below) — so
// making it a leaked singleton is deliberate rather than a leak.

namespace dazzle_updater {
namespace {

internal::UiRunner& Runner() {
  static internal::UiRunner* runner = new internal::UiRunner(
      internal::GetUiBackend(), internal::GetUpdaterPort(), GetUiPreferencesStore(),
      internal::GetUiStrings());
  return *runner;
}

}  // namespace

UpdaterUI::UpdaterUI() = default;
UpdaterUI::~UpdaterUI() = default;

UpdaterUI& UpdaterUI::GetInstance() {
  static UpdaterUI instance;
  return instance;
}

bool UpdaterUI::CheckForUpdates() { return Runner().CheckForUpdates(); }

bool UpdaterUI::CheckForUpdatesInBackground() { return Runner().CheckForUpdatesInBackground(); }

bool UpdaterUI::IsVisible() const { return Runner().IsVisible(); }

void UpdaterUI::SetAppName(const std::string& name) { Runner().SetAppName(name); }

std::string UpdaterUI::GetAppName() const { return Runner().GetAppName(); }

void UpdaterUI::SetAutomaticChecksEnabled(bool enabled) {
  Runner().SetAutomaticChecksEnabled(enabled);
}

bool UpdaterUI::IsAutomaticChecksEnabled() const { return Runner().IsAutomaticChecksEnabled(); }

void UpdaterUI::SetAutomaticDownloadsEnabled(bool enabled) {
  Runner().SetAutomaticDownloadsEnabled(enabled);
}

bool UpdaterUI::IsAutomaticDownloadsEnabled() const { return Runner().IsAutomaticDownloadsEnabled(); }

void UpdaterUI::SetAutomaticDownloadsAllowed(bool allowed) {
  Runner().SetAutomaticDownloadsAllowed(allowed);
}

bool UpdaterUI::IsAutomaticDownloadsAllowed() const { return Runner().IsAutomaticDownloadsAllowed(); }

}  // namespace dazzle_updater
