#include "updater_ui_content.h"

#include <cstdio>
#include <vector>

namespace dazzle {
namespace internal {
namespace {

/// Whether a string has anything but whitespace in it — the same test the prototype
/// makes before it shows the notes box.
bool HasText(const std::string& value) {
  return value.find_first_not_of(" \t\r\n") != std::string::npos;
}

void AddButton(UiContent* content, const std::string& label, UiEventKind event, bool is_default) {
  UiButton button;
  button.label = label;
  button.event = event;
  button.is_default = is_default;
  content->buttons.push_back(button);
}

/// The Error state's headline, by the call that failed.
const char* ErrorHeadline(const UiStrings& strings, UiErrorStep step) {
  switch (step) {
    case UiErrorStep::kDownload:
      return strings.error_download;
    case UiErrorStep::kInstall:
      return strings.error_install;
    case UiErrorStep::kCheck:
      break;
  }
  return strings.error_check;
}

/// The Error state's text, by GetErrorCode() (../specs/updater-ui.md, "错误文案").
/// Every code the table does not list reads as an unknown error.
std::string ErrorText(const UiStrings& strings, const std::string& code, const std::string& app_name) {
  if (code == "network") {
    return strings.error_network;
  }
  if (code == "invalid-index") {
    return strings.error_invalid_index;
  }
  if (code == "signature") {
    return strings.error_signature;
  }
  if (code == "permission") {
    return Format(strings.error_permission, {app_name});
  }
  if (code == "app-location") {
    return Format(strings.error_app_location, {app_name});
  }
  if (code == "installer") {
    return strings.error_installer;
  }
  return strings.error_other;
}

}  // namespace

std::string FormatBytes(int64_t bytes) {
  const int64_t size = bytes > 0 ? bytes : 0;
  char buffer[64];
  if (size < 1000000) {
    std::snprintf(buffer, sizeof(buffer), "%lld KB", static_cast<long long>((size + 500) / 1000));
  } else if (size < 1000000000) {
    std::snprintf(buffer, sizeof(buffer), "%.1f MB", static_cast<double>(size) / 1e6);
  } else {
    std::snprintf(buffer, sizeof(buffer), "%.2f GB", static_cast<double>(size) / 1e9);
  }
  return buffer;
}

UiContent BuildContent(const ContentInput& input) {
  const UiStrings& t = input.strings ? *input.strings : GetUiStrings();
  const std::string& app = input.app_name;
  const std::string version =
      input.update_display_version.empty() ? input.update_version : input.update_display_version;

  UiContent content;
  content.state = input.state;

  switch (input.state) {
    case UiState::kChecking:
      content.window = UiWindow::kUpdateStatus;
      content.window_title = t.software_update;
      content.heading = t.checking;
      content.progress.visible = true;
      content.progress.determinate = false;
      AddButton(&content, t.cancel, UiEventKind::kCancel, false);
      break;

    case UiState::kAvailable:
    case UiState::kDownloaded: {
      const bool downloaded = input.state == UiState::kDownloaded;
      content.window = UiWindow::kUpdateAvailable;
      content.window_title = Format(t.app_update, {app});
      content.heading = Format(downloaded ? t.downloaded_title : t.available_title, {app});
      content.detail = downloaded ? Format(t.downloaded_detail, {app, version})
                                  : Format(t.available_detail, {app, version, input.current_version});
      // Whitespace-only notes are no notes: the window shows the link, not an empty box.
      content.notes_markdown = HasText(input.release_notes) ? input.release_notes : std::string();
      if (!HasText(input.release_notes) && !input.release_notes_url.empty()) {
        content.notes_url = input.release_notes_url;
        content.notes_link_label = t.view_release_notes;
      }
      content.resizable = !content.notes_markdown.empty();
      content.checkbox.visible = input.show_automatic_downloads;
      content.checkbox.checked = input.automatic_downloads_checked;
      content.checkbox.label = t.automatic_downloads;
      AddButton(&content, t.skip, UiEventKind::kSkip, false);
      AddButton(&content, t.later, UiEventKind::kSecondary, false);
      AddButton(&content, downloaded ? t.install_and_relaunch : t.install_update,
                UiEventKind::kPrimary, true);
      break;
    }

    case UiState::kDownloading: {
      content.window = UiWindow::kUpdateStatus;
      content.window_title = Format(t.app_update, {app});
      content.heading = t.downloading;
      content.progress.visible = true;
      // A bar is determinate only when the release index states the package size.
      content.progress.determinate = input.update_size > 0 && input.progress >= 0.0;
      if (content.progress.determinate) {
        const double fraction = input.progress > 1.0 ? 1.0 : input.progress;
        content.progress.fraction = fraction;
        content.progress.status_text = Format(
            t.downloaded_of,
            {FormatBytes(static_cast<int64_t>(fraction * static_cast<double>(input.update_size))),
             FormatBytes(input.update_size)});
      }
      AddButton(&content, t.cancel, UiEventKind::kCancel, false);
      break;
    }

    case UiState::kReady:
      content.window = UiWindow::kUpdateStatus;
      content.window_title = Format(t.app_update, {app});
      content.heading = t.ready_title;
      content.progress.visible = true;
      content.progress.determinate = true;
      content.progress.fraction = 1.0;
      AddButton(&content, t.later, UiEventKind::kSecondary, false);
      AddButton(&content, t.install_and_relaunch, UiEventKind::kPrimary, true);
      break;

    case UiState::kInstalling:
      content.window = UiWindow::kUpdateStatus;
      content.window_title = Format(t.app_update, {app});
      content.heading = t.installing;
      content.progress.visible = true;
      content.progress.determinate = false;
      content.closable = false;  // the installer has been started; there is no way back
      break;

    case UiState::kUpToDate:
      content.window = UiWindow::kUpdateStatus;
      content.window_title = t.software_update;
      content.heading = t.up_to_date_title;
      content.detail = Format(t.up_to_date_detail, {app, input.current_version});
      AddButton(&content, t.ok, UiEventKind::kPrimary, true);
      break;

    case UiState::kError:
      content.window = UiWindow::kUpdateStatus;
      content.window_title = t.software_update;
      content.heading = ErrorHeadline(t, input.error_step);
      content.detail = ErrorText(t, input.error_code, app);
      content.error_message = input.error_message;
      AddButton(&content, t.ok, UiEventKind::kPrimary, true);
      break;

    case UiState::kPermission:
      content.window = UiWindow::kUpdatePermission;
      content.window_title = t.software_update;
      content.heading = t.permission_title;
      content.detail = Format(t.permission_detail, {app});
      content.checkbox.visible = input.show_automatic_downloads;
      content.checkbox.checked = input.automatic_downloads_checked;
      content.checkbox.label = t.permission_downloads;
      AddButton(&content, t.dont_check, UiEventKind::kSecondary, false);
      AddButton(&content, t.check_automatically, UiEventKind::kPrimary, true);
      break;
  }

  return content;
}

}  // namespace internal
}  // namespace dazzle
