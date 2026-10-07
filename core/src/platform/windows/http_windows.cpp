#include "../../foundation/http.h"

#include <windows.h>
#include <winhttp.h>

#include <cstdio>
#include <string>
#include <vector>

// Windows's HTTP seam (../../specs/updater.md, "平台可用性"): one blocking GET through
// WinHTTP, which every Windows ships. WinHTTP carries what the other platforms get from
// their own stacks — TLS against the system's certificate store, the system's proxy
// settings, and redirects (followed by default) — and dazzle_updater keeps for itself the contract
// http.h declares: a body to memory or to a file, progress on the calling thread, and a
// cancel that aborts it.

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

/// Closes a WinHTTP handle when the transfer leaves its scope, whichever way it leaves.
class Handle {
 public:
  explicit Handle(HINTERNET handle) : handle_(handle) {}
  ~Handle() {
    if (handle_ != nullptr) {
      WinHttpCloseHandle(handle_);
    }
  }
  Handle(const Handle&) = delete;
  Handle& operator=(const Handle&) = delete;
  HINTERNET get() const { return handle_; }
  explicit operator bool() const { return handle_ != nullptr; }

 private:
  HINTERNET handle_;
};

/// Calls the progress callback, if the caller installed one: an empty HttpProgress (the
/// release index fetch) must not be invoked.
bool ReportProgress(const HttpProgress& progress, int64_t received, int64_t total) {
  return !progress || progress(received, total);
}

/// The system's text for a WinHTTP or Win32 error, in English words where it has them.
std::string Describe(DWORD code) {
  wchar_t* buffer = nullptr;
  const DWORD length = FormatMessageW(
      FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_HMODULE | FORMAT_MESSAGE_FROM_SYSTEM |
          FORMAT_MESSAGE_IGNORE_INSERTS,
      GetModuleHandleW(L"winhttp.dll"), code, MAKELANGID(LANG_ENGLISH, SUBLANG_ENGLISH_US),
      reinterpret_cast<wchar_t*>(&buffer), 0, nullptr);
  std::string text = "error " + std::to_string(code);
  if (length > 0 && buffer != nullptr) {
    std::wstring wide(buffer, length);
    while (!wide.empty() && (wide.back() == L'\r' || wide.back() == L'\n' || wide.back() == L' ')) {
      wide.pop_back();
    }
    const int size = WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()),
                                         nullptr, 0, nullptr, nullptr);
    std::string utf8(static_cast<size_t>(size), '\0');
    WideCharToMultiByte(CP_UTF8, 0, wide.data(), static_cast<int>(wide.size()), utf8.data(), size,
                        nullptr, nullptr);
    text += " (" + utf8 + ")";
  }
  if (buffer != nullptr) {
    LocalFree(buffer);
  }
  return text;
}

class WindowsHttpImpl final : public HttpClient::Impl {
 public:
  HttpResult Get(const std::string& url, const std::string& destination_path,
                 const HttpProgress& progress) override {
    HttpResult result;
    auto network_failure = [&result](const std::string& what) {
      result.error = what + ": " + Describe(GetLastError());
      result.error_code = "network";
      return result;
    };

    const std::wstring wide_url = Widen(url);
    URL_COMPONENTS parts = {};
    parts.dwStructSize = sizeof(parts);
    parts.dwHostNameLength = static_cast<DWORD>(-1);
    parts.dwUrlPathLength = static_cast<DWORD>(-1);
    parts.dwExtraInfoLength = static_cast<DWORD>(-1);
    if (!WinHttpCrackUrl(wide_url.c_str(), 0, 0, &parts)) {
      return network_failure("the address cannot be parsed");
    }
    const std::wstring host(parts.lpszHostName, parts.dwHostNameLength);
    std::wstring path(parts.lpszUrlPath, parts.dwUrlPathLength);
    path.append(parts.lpszExtraInfo, parts.dwExtraInfoLength);
    if (path.empty()) {
      path = L"/";
    }

    Handle session(WinHttpOpen(L"dazzle_updater", WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                               WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0));
    if (!session) {
      return network_failure("cannot start an HTTP session");
    }
    // Connect, send and receive the headers within 30 s each; the body may take longer,
    // and a stalled body is still noticed within a minute.
    WinHttpSetTimeouts(session.get(), 30000, 30000, 30000, 60000);
    Handle connection(WinHttpConnect(session.get(), host.c_str(), parts.nPort, 0));
    if (!connection) {
      return network_failure("cannot connect to the server");
    }
    const DWORD flags = parts.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0;
    Handle request(WinHttpOpenRequest(connection.get(), L"GET", path.c_str(), nullptr,
                                      WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags));
    if (!request) {
      return network_failure("cannot start an HTTP request");
    }
    if (!WinHttpSendRequest(request.get(), WINHTTP_NO_ADDITIONAL_HEADERS, 0,
                            WINHTTP_NO_REQUEST_DATA, 0, 0, 0) ||
        !WinHttpReceiveResponse(request.get(), nullptr)) {
      return network_failure("the transfer failed");
    }

    DWORD status = 0;
    DWORD status_size = sizeof(status);
    WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                        WINHTTP_HEADER_NAME_BY_INDEX, &status, &status_size,
                        WINHTTP_NO_HEADER_INDEX);
    result.status_code = static_cast<int>(status);
    if (status < 200 || status >= 300) {
      result.error = status != 0 ? "the server answered with HTTP " + std::to_string(status)
                                 : "the server did not answer";
      result.error_code = "network";
      return result;
    }

    // Content-Length as a string: the numeric query is 32-bit, and an installer may not be.
    int64_t total = 0;
    wchar_t length_text[32] = {};
    DWORD length_size = sizeof(length_text);
    if (WinHttpQueryHeaders(request.get(), WINHTTP_QUERY_CONTENT_LENGTH,
                            WINHTTP_HEADER_NAME_BY_INDEX, length_text, &length_size,
                            WINHTTP_NO_HEADER_INDEX)) {
      total = _wcstoi64(length_text, nullptr, 10);
      if (total < 0) {
        total = 0;
      }
    }

    std::FILE* file = nullptr;
    if (!destination_path.empty()) {
      if (_wfopen_s(&file, Widen(destination_path).c_str(), L"wb") != 0 || file == nullptr) {
        result.error = "cannot write the download to " + destination_path;
        result.error_code = "permission";
        return result;
      }
    }
    auto discard_file = [&file, &destination_path] {
      if (file != nullptr) {
        std::fclose(file);
        file = nullptr;
        _wremove(Widen(destination_path).c_str());  // never leave a partial download behind
      }
    };

    std::string body;
    std::vector<char> buffer(64 * 1024);
    int64_t received = 0;
    if (!ReportProgress(progress, received, total)) {
      discard_file();
      result.error = "the transfer was cancelled";
      result.error_code = "cancelled";
      return result;
    }
    for (;;) {
      DWORD read = 0;
      if (!WinHttpReadData(request.get(), buffer.data(), static_cast<DWORD>(buffer.size()),
                           &read)) {
        discard_file();
        return network_failure("the transfer failed");
      }
      if (read == 0) {
        break;
      }
      if (file != nullptr) {
        if (std::fwrite(buffer.data(), 1, read, file) != read) {
          discard_file();
          result.error = "cannot write the download to " + destination_path;
          result.error_code = "permission";
          return result;
        }
      } else {
        body.append(buffer.data(), read);
      }
      received += read;
      if (!ReportProgress(progress, received, total)) {
        discard_file();
        result.error = "the transfer was cancelled";
        result.error_code = "cancelled";
        return result;
      }
    }

    if (file != nullptr) {
      const bool closed = std::fclose(file) == 0;
      file = nullptr;
      if (!closed) {
        _wremove(Widen(destination_path).c_str());
        result.error = "cannot write the download to " + destination_path;
        result.error_code = "permission";
        return result;
      }
    }
    result.ok = true;
    if (destination_path.empty()) {
      result.body = std::move(body);
    }
    return result;
  }
};

}  // namespace

bool HttpIsAvailable() {
  return true;
}

HttpClient::HttpClient() : pimpl_(std::make_unique<WindowsHttpImpl>()) {}

HttpClient::~HttpClient() = default;

}  // namespace dazzle_updater
