#include "../../foundation/system.h"

#include <windows.h>
#include <shellapi.h>

#include <string>
#include <vector>

#include "../../foundation/http.h"

// Windows's system seam (../../specs/updater.md, "平台可用性" and "安装"): check and
// download through the WinHTTP stack, install by starting the downloaded installer — an
// `exe` with the release's installerArguments, an `msi` through `msiexec /i` — which then
// decides for itself when to replace the application and whether to start it again.
//
// The application's own name and version come from the executable's VERSIONINFO, the
// metadata every Windows program carries (ProductName, ProductVersion).

namespace dazzle_updater {
namespace {

std::wstring Widen(const std::string& value) {
  if (value.empty()) {
    return std::wstring();
  }
  const int length = MultiByteToWideChar(CP_UTF8, 0, value.data(),
                                         static_cast<int>(value.size()), nullptr, 0);
  std::wstring wide(static_cast<size_t>(length), L'\0');
  MultiByteToWideChar(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), wide.data(),
                      length);
  return wide;
}

std::string Narrow(const std::wstring& value) {
  if (value.empty()) {
    return std::string();
  }
  const int length = WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()),
                                         nullptr, 0, nullptr, nullptr);
  std::string utf8(static_cast<size_t>(length), '\0');
  WideCharToMultiByte(CP_UTF8, 0, value.data(), static_cast<int>(value.size()), utf8.data(),
                      length, nullptr, nullptr);
  return utf8;
}

std::wstring ExecutablePath() {
  std::vector<wchar_t> buffer(MAX_PATH);
  for (;;) {
    const DWORD length =
        GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
    if (length == 0) {
      return std::wstring();
    }
    if (length < buffer.size()) {
      return std::wstring(buffer.data(), length);
    }
    buffer.resize(buffer.size() * 2);
  }
}

/// One string of the executable's VERSIONINFO, from its first translation; "" when the
/// executable has none (a program built without a version resource).
std::string VersionInfoString(const wchar_t* name) {
  const std::wstring path = ExecutablePath();
  if (path.empty()) {
    return "";
  }
  DWORD ignored = 0;
  const DWORD size = GetFileVersionInfoSizeW(path.c_str(), &ignored);
  if (size == 0) {
    return "";
  }
  std::vector<unsigned char> data(size);
  if (!GetFileVersionInfoW(path.c_str(), 0, size, data.data())) {
    return "";
  }
  struct Translation {
    WORD language;
    WORD code_page;
  };
  Translation* translations = nullptr;
  UINT translations_size = 0;
  if (!VerQueryValueW(data.data(), L"\\VarFileInfo\\Translation",
                      reinterpret_cast<void**>(&translations), &translations_size) ||
      translations_size < sizeof(Translation)) {
    return "";
  }
  wchar_t key[64];
  swprintf(key, 64, L"\\StringFileInfo\\%04x%04x\\%s", translations[0].language,
           translations[0].code_page, name);
  wchar_t* value = nullptr;
  UINT value_length = 0;
  if (!VerQueryValueW(data.data(), key, reinterpret_cast<void**>(&value), &value_length) ||
      value == nullptr || value_length == 0) {
    return "";
  }
  std::wstring text(value);
  while (!text.empty() && text.back() == L' ') {
    text.pop_back();
  }
  return Narrow(text);
}

std::string EnvironmentVariable(const wchar_t* name) {
  wchar_t buffer[MAX_PATH] = {};
  const DWORD length = GetEnvironmentVariableW(name, buffer, MAX_PATH);
  if (length == 0 || length >= MAX_PATH) {
    return "";
  }
  return Narrow(std::wstring(buffer, length));
}

bool EndsWithIgnoringCase(const std::string& value, const std::string& suffix) {
  if (value.size() < suffix.size()) {
    return false;
  }
  for (size_t i = 0; i < suffix.size(); ++i) {
    const char a = value[value.size() - suffix.size() + i];
    if (static_cast<char>(tolower(static_cast<unsigned char>(a))) != suffix[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool SystemSupportsCheck() {
  return HttpIsAvailable();
}

bool SystemSupportsDownload() {
  return HttpIsAvailable();
}

bool SystemSupportsInstall() {
  return true;
}

std::string SystemName() {
  return "windows";
}

std::string SystemArch() {
  // The architecture this process runs as, which is the one the update must be built for:
  // an x64 program on an Arm machine is updated with an x64 package.
#if defined(_M_ARM64)
  return "arm64";
#elif defined(_M_X64) || defined(_M_AMD64)
  return "x64";
#elif defined(_M_IX86)
  return "x86";
#else
  return "";
#endif
}

std::string SystemVersion() {
  // RtlGetVersion rather than GetVersionEx, which answers what the manifest claims to
  // support rather than what is running.
  using RtlGetVersionFunction = LONG(WINAPI*)(OSVERSIONINFOW*);
  const HMODULE ntdll = GetModuleHandleW(L"ntdll.dll");
  if (ntdll == nullptr) {
    return "";
  }
  const auto rtl_get_version =
      reinterpret_cast<RtlGetVersionFunction>(GetProcAddress(ntdll, "RtlGetVersion"));
  if (rtl_get_version == nullptr) {
    return "";
  }
  OSVERSIONINFOW info = {};
  info.dwOSVersionInfoSize = sizeof(info);
  if (rtl_get_version(&info) != 0) {
    return "";
  }
  return std::to_string(info.dwMajorVersion) + "." + std::to_string(info.dwMinorVersion) + "." +
         std::to_string(info.dwBuildNumber);
}

std::string AppVersionFromPlatform() {
  return VersionInfoString(L"ProductVersion");
}

std::string AppNameFromPlatform() {
  const std::string name = VersionInfoString(L"ProductName");
  if (!name.empty()) {
    return name;
  }
  // No version resource: the executable's name without its extension, which is what
  // Windows itself shows for such a program.
  std::wstring path = ExecutablePath();
  const size_t separator = path.find_last_of(L"\\/");
  if (separator != std::wstring::npos) {
    path = path.substr(separator + 1);
  }
  const size_t dot = path.find_last_of(L'.');
  if (dot != std::wstring::npos) {
    path = path.substr(0, dot);
  }
  return Narrow(path);
}

std::string DownloadDirectory() {
  std::string base = EnvironmentVariable(L"LOCALAPPDATA");
  if (base.empty()) {
    base = EnvironmentVariable(L"TEMP");
  }
  if (base.empty()) {
    return "";
  }
  const std::string directory = base + "\\dazzle_updater";
  if (!CreateDirectoryW(Widen(directory).c_str(), nullptr) &&
      GetLastError() != ERROR_ALREADY_EXISTS) {
    return "";
  }
  return directory;
}

bool InstallDownloadedUpdate(const std::string& downloaded_path, const std::string& format,
                             const std::string& installer_arguments, std::string* error,
                             std::string* error_code) {
  auto fail = [error, error_code](const std::string& code, const std::string& message) {
    if (error != nullptr) {
      *error = message;
    }
    if (error_code != nullptr) {
      *error_code = code;
    }
    return false;
  };

  const std::wstring path = Widen(downloaded_path);
  if (GetFileAttributesW(path.c_str()) == INVALID_FILE_ATTRIBUTES) {
    return fail("installer", "the downloaded installer is missing");
  }

  std::wstring file;
  std::wstring parameters;
  if (format == "msi" || (format.empty() && EndsWithIgnoringCase(downloaded_path, ".msi"))) {
    file = L"msiexec.exe";
    parameters = L"/i \"" + path + L"\"";
    if (!installer_arguments.empty()) {
      parameters += L" " + Widen(installer_arguments);
    }
  } else if (format == "exe" || (format.empty() && EndsWithIgnoringCase(downloaded_path, ".exe"))) {
    file = path;
    parameters = Widen(installer_arguments);
  } else {
    return fail("installer",
                "the installer format \"" + format + "\" is not supported on Windows");
  }

  // ShellExecuteEx rather than CreateProcess: an installer that asks for elevation in its
  // manifest gets the UAC prompt instead of ERROR_ELEVATION_REQUIRED.
  SHELLEXECUTEINFOW info = {};
  info.cbSize = sizeof(info);
  info.fMask = SEE_MASK_NOASYNC | SEE_MASK_FLAG_NO_UI;
  info.lpVerb = L"open";
  info.lpFile = file.c_str();
  info.lpParameters = parameters.empty() ? nullptr : parameters.c_str();
  info.nShow = SW_SHOWNORMAL;
  if (!ShellExecuteExW(&info)) {
    const DWORD code = GetLastError();
    if (code == ERROR_CANCELLED) {
      return fail("permission", "the installer was not allowed to run");
    }
    return fail("installer", "cannot start the installer (error " + std::to_string(code) + ")");
  }
  return true;
}

}  // namespace dazzle_updater
