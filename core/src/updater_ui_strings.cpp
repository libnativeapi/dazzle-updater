#include "updater_ui_strings.h"

#include <cctype>

#include "foundation/updater_ui_preferences.h"

namespace dazzle {
namespace internal {
namespace {

// The wording of prototype/src/features/updater-ui/strings.ts, without the Sparkle
// reference block: specs/updater-ui.md has not taken those states up, so the windows do
// not draw them.
const UiStrings kEnglish = {
    /* software_update */ "Software Update",
    /* app_update */ "{0} Update",
    /* checking */ "Checking for updates…",
    /* available_title */ "A new version of {0} is available!",
    /* available_detail */ "{0} {1} is now available—you have {2}.",
    /* view_release_notes */ "View Release Notes",
    /* downloading */ "Downloading update…",
    /* downloaded_of */ "{0} of {1}",
    /* downloaded */ "{0} downloaded",
    /* ready_title */ "Ready to Install",
    /* installing */ "Installing…",
    /* up_to_date_title */ "You’re up to date!",
    /* up_to_date_detail */ "{0} {1} is currently the newest version available.",
    /* error_check */ "An error occurred while checking for updates.",
    /* error_download */ "An error occurred while downloading the update.",
    /* error_install */ "An error occurred while installing the update.",
    /* error_network */
    "Couldn’t connect to the update server. Check your internet connection and try again.",
    /* error_invalid_index */
    "The update information isn’t in the expected format. Try again later, or contact the developer.",
    /* error_signature */
    "The update isn’t signed correctly, so it wasn’t installed. Try again later, or contact the "
    "developer.",
    /* error_permission */
    "{0} can’t be replaced. Allow {0} to modify apps in System Settings › Privacy & Security › App "
    "Management, or use an account that can.",
    /* error_app_location */
    "{0} is running from a read-only or temporary location and can’t be updated there. Move it to "
    "the Applications folder, open it from there and try again.",
    /* error_installer */ "The installer couldn’t be started. Try again later.",
    /* error_other */ "An unknown error occurred. Try again later.",
    /* skip */ "Skip This Version",
    /* automatic_downloads */ "Automatically download and install updates in the future",
    /* downloaded_title */ "A new version of {0} is ready to install!",
    /* downloaded_detail */
    "{0} {1} has been downloaded. Would you like to install it and relaunch {0} now?",
    /* permission_title */ "Check for updates automatically?",
    /* permission_detail */
    "{0} can check for new versions automatically. You can always check for updates manually from "
    "the {0} menu.",
    /* permission_downloads */ "Automatically download and install updates",
    /* dont_check */ "Don’t Check",
    /* check_automatically */ "Check Automatically",
    /* cancel */ "Cancel",
    /* later */ "Later",
    /* install_update */ "Install Update",
    /* install_and_relaunch */ "Install and Relaunch",
    /* ok */ "OK",
};

const UiStrings kSimplifiedChinese = {
    /* software_update */ "软件更新",
    /* app_update */ "{0} 更新",
    /* checking */ "正在检查更新…",
    /* available_title */ "{0} 有新版本可用",
    /* available_detail */ "{0} {1} 已发布，你当前使用的是 {2}。",
    /* view_release_notes */ "查看更新说明",
    /* downloading */ "正在下载更新…",
    /* downloaded_of */ "已下载 {0} / {1}",
    /* downloaded */ "已下载 {0}",
    /* ready_title */ "更新已准备就绪",
    /* installing */ "正在安装…",
    /* up_to_date_title */ "已是最新版本",
    /* up_to_date_detail */ "{0} {1} 已是最新版本。",
    /* error_check */ "检查更新时出错",
    /* error_download */ "下载更新时出错",
    /* error_install */ "安装更新时出错",
    /* error_network */ "无法连接到更新服务器。请检查网络连接后再试。",
    /* error_invalid_index */ "更新信息的格式不正确。请稍后再试，或联系开发者。",
    /* error_signature */ "更新包的签名无效，已停止安装。请稍后再试，或联系开发者。",
    /* error_permission */
    "没有权限替换 {0}。请在\"系统设置 → 隐私与安全性 → App 管理\"中允许 {0} 修改应用，或以有权限的"
    "账户运行。",
    /* error_app_location */
    "{0} 正在从只读或临时位置运行，无法原地更新。请将它移到\"应用程序\"文件夹，从那里重新打开后再试。",
    /* error_installer */ "无法启动安装程序。请稍后再试。",
    /* error_other */ "发生了未知错误。请稍后再试。",
    /* skip */ "跳过此版本",
    /* automatic_downloads */ "以后自动下载并安装更新",
    /* downloaded_title */ "{0} 的新版本已准备好安装",
    /* downloaded_detail */ "{0} {1} 已下载完成。要现在安装并重新启动 {0} 吗？",
    /* permission_title */ "要自动检查更新吗？",
    /* permission_detail */ "{0} 可以自动检查新版本。你随时可以在 {0} 菜单中手动检查更新。",
    /* permission_downloads */ "自动下载并安装更新",
    /* dont_check */ "不检查",
    /* check_automatically */ "自动检查",
    /* cancel */ "取消",
    /* later */ "以后再说",
    /* install_update */ "安装更新",
    /* install_and_relaunch */ "安装并重新启动",
    /* ok */ "好",
};

std::string ToLower(const std::string& value) {
  std::string lower = value;
  for (char& character : lower) {
    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
  }
  return lower;
}

/// Whether a BCP-47 tag asks for Simplified Chinese. Traditional Chinese locales are
/// not worded for, so they fall back to English rather than showing Simplified wording.
bool IsSimplifiedChinese(const std::string& tag) {
  const std::string lower = ToLower(tag);
  if (lower.compare(0, 2, "zh") != 0) {
    return false;
  }
  if (lower.find("hant") != std::string::npos) {
    return false;
  }
  return lower.find("-tw") == std::string::npos && lower.find("-hk") == std::string::npos &&
         lower.find("-mo") == std::string::npos;
}

}  // namespace

const UiStrings& UiStringsFor(const std::vector<std::string>& preferred_languages) {
  for (const std::string& language : preferred_languages) {
    if (IsSimplifiedChinese(language)) {
      return kSimplifiedChinese;
    }
    const std::string lower = ToLower(language);
    if (lower.compare(0, 2, "en") == 0) {
      return kEnglish;
    }
  }
  return kEnglish;
}

const UiStrings& GetUiStrings() {
  static const UiStrings& strings = UiStringsFor(UiPreferredLanguages());
  return strings;
}

std::string Format(const char* pattern, const std::vector<std::string>& values) {
  std::string result = pattern ? pattern : "";
  for (size_t index = 0; index < values.size(); ++index) {
    const std::string placeholder = "{" + std::to_string(index) + "}";
    size_t at = 0;
    while ((at = result.find(placeholder, at)) != std::string::npos) {
      result.replace(at, placeholder.size(), values[index]);
      at += values[index].size();
    }
  }
  return result;
}

}  // namespace internal
}  // namespace dazzle
