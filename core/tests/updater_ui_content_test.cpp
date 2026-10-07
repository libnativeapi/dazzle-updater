#include "test_support.h"

#include "../src/updater_ui_content.h"
#include "../src/updater_ui_state.h"
#include "../src/updater_ui_strings.h"

#include <string>
#include <vector>

// What each state puts on screen: the window it belongs to, its wording, its buttons and
// its controls. Pure functions, so specs/updater-ui.md's tables are checked directly,
// without a thread or a window.

using dazzle::internal::BuildContent;
using dazzle::internal::ContentInput;
using dazzle::internal::Format;
using dazzle::internal::FormatBytes;
using dazzle::internal::UiContent;
using dazzle::internal::UiErrorStep;
using dazzle::internal::UiState;
using dazzle::internal::UiStrings;
using dazzle::internal::UiStringsFor;
using dazzle::internal::UiWindow;

namespace {

const UiStrings& English() { return UiStringsFor({"en"}); }
const UiStrings& Chinese() { return UiStringsFor({"zh-Hans-CN"}); }

ContentInput Input(UiState state) {
  ContentInput input;
  input.strings = &English();
  input.state = state;
  input.app_name = "Test App";
  input.current_version = "1.0.0";
  input.update_version = "1.2.3";
  input.update_display_version = "1.2.3";
  return input;
}

UiContent Content(UiState state) { return BuildContent(Input(state)); }

std::vector<std::string> Buttons(const UiContent& content) {
  std::vector<std::string> labels;
  for (const dazzle::internal::UiButton& button : content.buttons) {
    labels.push_back(button.label);
  }
  return labels;
}

}  // namespace

DAZZLE_TEST(updater_ui_content_puts_every_state_in_its_window) {
  struct Expectation {
    UiState state;
    UiWindow window;
    const char* title;
  };
  const Expectation expectations[] = {
      {UiState::kChecking, UiWindow::kUpdateStatus, "Software Update"},
      {UiState::kAvailable, UiWindow::kUpdateAvailable, "Test App Update"},
      {UiState::kDownloaded, UiWindow::kUpdateAvailable, "Test App Update"},
      {UiState::kDownloading, UiWindow::kUpdateStatus, "Test App Update"},
      {UiState::kReady, UiWindow::kUpdateStatus, "Test App Update"},
      {UiState::kInstalling, UiWindow::kUpdateStatus, "Test App Update"},
      {UiState::kUpToDate, UiWindow::kUpdateStatus, "Software Update"},
      {UiState::kError, UiWindow::kUpdateStatus, "Software Update"},
      {UiState::kPermission, UiWindow::kUpdatePermission, "Software Update"},
  };
  for (const Expectation& expectation : expectations) {
    const UiContent content = BuildContent(Input(expectation.state));
    EXPECT_EQ(static_cast<int>(content.state), static_cast<int>(expectation.state));
    EXPECT_EQ(static_cast<int>(content.window), static_cast<int>(expectation.window));
    EXPECT_EQ(content.window_title, std::string(expectation.title));
  }
}

DAZZLE_TEST(updater_ui_content_gives_every_state_its_buttons) {
  EXPECT_EQ(Buttons(Content(UiState::kChecking)), (std::vector<std::string>{"Cancel"}));
  EXPECT_EQ(Buttons(Content(UiState::kAvailable)),
            (std::vector<std::string>{"Skip This Version", "Later", "Install Update"}));
  EXPECT_EQ(Buttons(Content(UiState::kDownloaded)),
            (std::vector<std::string>{"Skip This Version", "Later", "Install and Relaunch"}));
  EXPECT_EQ(Buttons(Content(UiState::kDownloading)), (std::vector<std::string>{"Cancel"}));
  EXPECT_EQ(Buttons(Content(UiState::kReady)),
            (std::vector<std::string>{"Later", "Install and Relaunch"}));
  EXPECT_EQ(Buttons(Content(UiState::kInstalling)).size(), static_cast<size_t>(0));
  EXPECT_EQ(Buttons(Content(UiState::kUpToDate)), (std::vector<std::string>{"OK"}));
  EXPECT_EQ(Buttons(Content(UiState::kError)), (std::vector<std::string>{"OK"}));
  EXPECT_EQ(Buttons(Content(UiState::kPermission)),
            (std::vector<std::string>{"Don’t Check", "Check Automatically"}));

  // The default button is the one the return key presses.
  const UiContent available = Content(UiState::kAvailable);
  const UiContent up_to_date = Content(UiState::kUpToDate);
  const UiContent permission = Content(UiState::kPermission);
  const UiContent checking = Content(UiState::kChecking);
  EXPECT_TRUE(available.buttons[2].is_default);
  EXPECT_FALSE(available.buttons[0].is_default);
  EXPECT_TRUE(up_to_date.buttons[0].is_default);
  EXPECT_TRUE(permission.buttons[1].is_default);
  EXPECT_FALSE(checking.buttons[0].is_default);
}

DAZZLE_TEST(updater_ui_content_installing_has_no_way_out) {
  const UiContent content = Content(UiState::kInstalling);
  EXPECT_FALSE(content.closable);
  EXPECT_EQ(content.heading, "Installing…");
  EXPECT_TRUE(content.progress.visible);
  EXPECT_FALSE(content.progress.determinate);
}

DAZZLE_TEST(updater_ui_content_states_that_can_be_closed_say_so) {
  EXPECT_TRUE(Content(UiState::kChecking).closable);
  EXPECT_TRUE(Content(UiState::kAvailable).closable);
  EXPECT_TRUE(Content(UiState::kReady).closable);
  EXPECT_TRUE(Content(UiState::kUpToDate).closable);
  EXPECT_TRUE(Content(UiState::kError).closable);
  EXPECT_TRUE(Content(UiState::kPermission).closable);
}

DAZZLE_TEST(updater_ui_content_shows_the_progress_only_when_the_size_is_known) {
  ContentInput downloading = Input(UiState::kDownloading);
  UiContent content = BuildContent(downloading);
  EXPECT_TRUE(content.progress.visible);
  EXPECT_FALSE(content.progress.determinate);  // no size: an indeterminate bar
  EXPECT_EQ(content.progress.status_text, "");

  downloading.update_size = 46000000;
  downloading.progress = 0.5;
  content = BuildContent(downloading);
  EXPECT_TRUE(content.progress.determinate);
  EXPECT_EQ(content.progress.fraction, 0.5);
  EXPECT_EQ(content.progress.status_text, "23.0 MB of 46.0 MB");

  // -1.0 is GetDownloadProgress()'s "the total size is unknown".
  downloading.progress = -1.0;
  content = BuildContent(downloading);
  EXPECT_FALSE(content.progress.determinate);
  EXPECT_EQ(content.progress.status_text, "");

  // Ready's bar is full, and the states without a bar have none.
  const UiContent ready = Content(UiState::kReady);
  EXPECT_TRUE(ready.progress.visible);
  EXPECT_TRUE(ready.progress.determinate);
  EXPECT_EQ(ready.progress.fraction, 1.0);
  EXPECT_FALSE(Content(UiState::kUpToDate).progress.visible);
  EXPECT_FALSE(Content(UiState::kError).progress.visible);
  EXPECT_FALSE(Content(UiState::kAvailable).progress.visible);
}

DAZZLE_TEST(updater_ui_content_shows_the_notes_box_only_when_there_are_notes) {
  ContentInput input = Input(UiState::kAvailable);
  input.release_notes = "# Fixes\n";
  UiContent content = BuildContent(input);
  EXPECT_EQ(content.notes_markdown, "# Fixes\n");
  EXPECT_TRUE(content.resizable);
  EXPECT_EQ(content.notes_link_label, "");

  // Empty notes with a page to read them on: a link instead of a box, and no resizing.
  input.release_notes = "  \n ";
  input.release_notes_url = "https://example.com/notes";
  content = BuildContent(input);
  EXPECT_TRUE(content.notes_markdown.empty());
  EXPECT_EQ(content.notes_url, "https://example.com/notes");
  EXPECT_EQ(content.notes_link_label, "View Release Notes");
  EXPECT_FALSE(content.resizable);

  // Neither: no box, no link, no resizing.
  input.release_notes_url.clear();
  content = BuildContent(input);
  EXPECT_EQ(content.notes_link_label, "");
  EXPECT_EQ(content.notes_url, "");
  EXPECT_FALSE(content.resizable);
}

DAZZLE_TEST(updater_ui_content_shows_the_checkbox_only_where_it_belongs) {
  ContentInput input = Input(UiState::kAvailable);
  EXPECT_FALSE(BuildContent(input).checkbox.visible);

  input.show_automatic_downloads = true;
  input.automatic_downloads_checked = true;
  UiContent content = BuildContent(input);
  EXPECT_TRUE(content.checkbox.visible);
  EXPECT_TRUE(content.checkbox.checked);
  EXPECT_EQ(content.checkbox.label, "Automatically download and install updates in the future");

  // The permission prompt words its own check box, and starts unchecked.
  ContentInput permission = Input(UiState::kPermission);
  permission.show_automatic_downloads = true;
  content = BuildContent(permission);
  EXPECT_TRUE(content.checkbox.visible);
  EXPECT_FALSE(content.checkbox.checked);
  EXPECT_EQ(content.checkbox.label, "Automatically download and install updates");

  // No other state has one, even when the application allows automatic downloads.
  for (const UiState state : {UiState::kChecking, UiState::kDownloading, UiState::kReady,
                              UiState::kInstalling, UiState::kUpToDate, UiState::kError}) {
    ContentInput other = Input(state);
    other.show_automatic_downloads = true;
    EXPECT_FALSE(BuildContent(other).checkbox.visible);
  }
}

DAZZLE_TEST(updater_ui_content_names_the_error_and_explains_it_by_code) {
  struct Expectation {
    const char* code;
    const char* text;
  };
  const Expectation expectations[] = {
      {"network",
       "Couldn’t connect to the update server. Check your internet connection and try again."},
      {"invalid-index",
       "The update information isn’t in the expected format. Try again later, or contact the "
       "developer."},
      {"signature",
       "The update isn’t signed correctly, so it wasn’t installed. Try again later, or contact the "
       "developer."},
      {"permission",
       "Test App can’t be replaced. Allow Test App to modify apps in System Settings › Privacy & "
       "Security › App Management, or use an account that can."},
      {"app-location",
       "Test App is running from a read-only or temporary location and can’t be updated there. Move "
       "it to the Applications folder, open it from there and try again."},
      {"installer", "The installer couldn’t be started. Try again later."},
      // Anything the spec's table does not list reads as an unknown error.
      {"busy", "An unknown error occurred. Try again later."},
      {"cancelled", "An unknown error occurred. Try again later."},
      {"unsupported-platform", "An unknown error occurred. Try again later."},
  };
  for (const Expectation& expectation : expectations) {
    ContentInput input = Input(UiState::kError);
    input.error_code = expectation.code;
    const UiContent content = BuildContent(input);
    EXPECT_EQ(content.detail, std::string(expectation.text));
  }
}

DAZZLE_TEST(updater_ui_content_names_the_error_after_the_step_that_failed) {
  ContentInput input = Input(UiState::kError);
  input.error_code = "network";

  input.error_step = UiErrorStep::kCheck;
  EXPECT_EQ(BuildContent(input).heading, "An error occurred while checking for updates.");
  input.error_step = UiErrorStep::kDownload;
  EXPECT_EQ(BuildContent(input).heading, "An error occurred while downloading the update.");
  input.error_step = UiErrorStep::kInstall;
  EXPECT_EQ(BuildContent(input).heading, "An error occurred while installing the update.");
}

DAZZLE_TEST(updater_ui_content_keeps_the_raw_error_message_for_a_bug_report) {
  ContentInput input = Input(UiState::kError);
  input.error_code = "signature";
  input.error_message = "signature mismatch";
  EXPECT_EQ(BuildContent(input).error_message, "signature mismatch");
}

DAZZLE_TEST(updater_ui_content_words_the_windows_in_simplified_chinese) {
  ContentInput input = Input(UiState::kAvailable);
  input.strings = &Chinese();
  input.release_notes = "# 修复\n";
  const UiContent content = BuildContent(input);
  EXPECT_EQ(content.window_title, "Test App 更新");
  EXPECT_EQ(content.heading, "Test App 有新版本可用");
  EXPECT_EQ(content.detail, "Test App 1.2.3 已发布，你当前使用的是 1.0.0。");
  EXPECT_EQ(content.buttons[0].label, "跳过此版本");
  EXPECT_EQ(content.buttons[1].label, "以后再说");
  EXPECT_EQ(content.buttons[2].label, "安装更新");
  EXPECT_EQ(content.window_title, "Test App 更新");

  ContentInput downloading = Input(UiState::kDownloading);
  downloading.strings = &Chinese();
  downloading.update_size = 46000000;
  downloading.progress = 0.5;
  EXPECT_EQ(BuildContent(downloading).progress.status_text, "已下载 23.0 MB / 46.0 MB");

  ContentInput error = Input(UiState::kError);
  error.strings = &Chinese();
  error.error_code = "permission";
  EXPECT_EQ(BuildContent(error).detail,
            "没有权限替换 Test App。请在\"系统设置 → 隐私与安全性 → App 管理\"中允许 Test App 修改应用，"
            "或以有权限的账户运行。");

  ContentInput permission = Input(UiState::kPermission);
  permission.strings = &Chinese();
  permission.show_automatic_downloads = true;
  const UiContent permission_content = BuildContent(permission);
  EXPECT_EQ(permission_content.heading, "要自动检查更新吗？");
  EXPECT_EQ(permission_content.buttons[0].label, "不检查");
  EXPECT_EQ(permission_content.buttons[1].label, "自动检查");
  EXPECT_EQ(permission_content.checkbox.label, "自动下载并安装更新");
}

DAZZLE_TEST(updater_ui_strings_follow_the_system_language) {
  // The first language the windows are worded in wins.
  EXPECT_EQ(std::string(UiStringsFor({"zh-Hans-CN"}).ok), "好");
  EXPECT_EQ(std::string(UiStringsFor({"zh-CN"}).ok), "好");
  EXPECT_EQ(std::string(UiStringsFor({"fr", "zh-Hans"}).ok), "好");
  EXPECT_EQ(std::string(UiStringsFor({"en-US"}).ok), "OK");
  EXPECT_EQ(std::string(UiStringsFor({"fr", "de"}).ok), "OK");
  EXPECT_EQ(std::string(UiStringsFor({}).ok), "OK");
  // Traditional Chinese is not worded for: English rather than Simplified wording.
  EXPECT_EQ(std::string(UiStringsFor({"zh-Hant-TW"}).ok), "OK");
  EXPECT_EQ(std::string(UiStringsFor({"zh-HK"}).ok), "OK");
}

DAZZLE_TEST(updater_ui_strings_replace_every_placeholder) {
  EXPECT_EQ(Format("{0} {1} has been downloaded. Relaunch {0}?", {"Firefox", "1.2.3"}),
            "Firefox 1.2.3 has been downloaded. Relaunch Firefox?");
  // A placeholder with no value is left alone, so a mistake in the table is visible.
  EXPECT_EQ(Format("{0} and {1}", {"a"}), "a and {1}");
  EXPECT_EQ(Format("no placeholders", {"a"}), "no placeholders");
}

DAZZLE_TEST(updater_ui_format_bytes_matches_the_windows) {
  EXPECT_EQ(FormatBytes(0), "0 KB");
  EXPECT_EQ(FormatBytes(999), "1 KB");
  EXPECT_EQ(FormatBytes(512000), "512 KB");
  EXPECT_EQ(FormatBytes(12300000), "12.3 MB");
  EXPECT_EQ(FormatBytes(46000000), "46.0 MB");
  EXPECT_EQ(FormatBytes(1250000000), "1.25 GB");
  EXPECT_EQ(FormatBytes(-1), "0 KB");
}
