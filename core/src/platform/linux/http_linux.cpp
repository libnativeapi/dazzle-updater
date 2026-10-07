#include "../../foundation/http.h"

#include <curl/curl.h>

#include <cstdio>
#include <string>

// Linux's HTTP seam (../../specs/updater.md, "平台可用性"): one blocking GET through
// libcurl, which the build links only when it found the library. Without it this file is
// not compiled at all, HttpIsAvailable() answers false, and system_linux.cpp reports that
// the platform can neither check nor download.
//
// libcurl carries the parts the other platforms get from their own stacks: TLS with the
// system's CA bundle, redirects, and the response code of the *last* response after a
// redirect. What dazzle_updater keeps for itself is the same contract http.h declares — a body to
// memory or to a file, progress on the calling thread, and a cancel that aborts it.

namespace dazzle_updater {
namespace {

/// Shared between libcurl's callbacks and the calling thread; libcurl runs the callbacks
/// on that same thread, inside curl_easy_perform(), so nothing here needs a lock.
struct TransferState {
  const HttpProgress* progress = nullptr;
  std::string* body = nullptr;
  std::FILE* file = nullptr;
  int64_t received = 0;
  bool cancelled = false;
  bool write_failed = false;
};

/// Calls the progress callback, if the caller installed one: an empty HttpProgress (the
/// release index fetch) must not be invoked.
bool ReportProgress(const HttpProgress* progress, int64_t received, int64_t total) {
  if (progress == nullptr || !*progress) {
    return true;
  }
  return (*progress)(received, total);
}

size_t WriteCallback(char* data, size_t size, size_t count, void* userdata) {
  TransferState* state = static_cast<TransferState*>(userdata);
  const size_t length = size * count;
  if (state->file != nullptr) {
    const size_t written = std::fwrite(data, 1, length, state->file);
    if (written != length) {
      state->write_failed = true;
      return 0;  // stops the transfer with CURLE_WRITE_ERROR
    }
  } else if (state->body != nullptr) {
    state->body->append(data, length);
  }
  state->received += static_cast<int64_t>(length);
  return length;
}

/// The progress meter, which is also where a cancel is noticed: returning non-zero aborts
/// the transfer with CURLE_ABORTED_BY_CALLBACK. The size is the server's own
/// Content-Length, so it is 0 while the server does not announce one.
int ProgressCallback(void* userdata, curl_off_t download_total, curl_off_t download_now,
                     curl_off_t, curl_off_t) {
  TransferState* state = static_cast<TransferState*>(userdata);
  const int64_t total = download_total > 0 ? static_cast<int64_t>(download_total) : 0;
  const int64_t received =
      download_now > 0 ? static_cast<int64_t>(download_now) : state->received;
  if (!ReportProgress(state->progress, received, total)) {
    state->cancelled = true;
    return 1;
  }
  return 0;
}

/// curl_global_init() must run once, before any easy handle; libcurl is not initialized
/// for us and a static initializer would run before main().
void EnsureCurlInitialized() {
  static const bool initialized = [] {
    return curl_global_init(CURL_GLOBAL_DEFAULT) == CURLE_OK;
  }();
  (void)initialized;
}

class LinuxHttpImpl final : public HttpClient::Impl {
 public:
  HttpResult Get(const std::string& url, const std::string& destination_path,
                 const HttpProgress& progress) override {
    HttpResult result;
    EnsureCurlInitialized();

    TransferState state;
    state.progress = &progress;
    std::string body;
    if (destination_path.empty()) {
      state.body = &body;
    } else {
      state.file = std::fopen(destination_path.c_str(), "wb");
      if (state.file == nullptr) {
        result.error = "cannot write the download to " + destination_path;
        result.error_code = "permission";
        return result;
      }
    }

    CURL* curl = curl_easy_init();
    if (curl == nullptr) {
      if (state.file != nullptr) {
        std::fclose(state.file);
        std::remove(destination_path.c_str());
      }
      result.error = "cannot start an HTTP transfer";
      result.error_code = "network";
      return result;
    }

    char error_buffer[CURL_ERROR_SIZE] = {0};
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_MAXREDIRS, 10L);
    // No signal handler and no SIGALRM: a library must not take over either.
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 30L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3600L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "dazzle_updater");
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &state);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, &state);
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_ERRORBUFFER, error_buffer);
    // The status code is checked here rather than by libcurl, so a 404 is reported the
    // way the other platforms report it.
    curl_easy_setopt(curl, CURLOPT_FAILONERROR, 0L);

    const CURLcode code = curl_easy_perform(curl);
    long status = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
    curl_easy_cleanup(curl);

    if (state.file != nullptr) {
      std::fclose(state.file);
      state.file = nullptr;
    }

    const bool http_ok = status >= 200 && status < 300;
    result.status_code = static_cast<int>(status);
    result.ok = code == CURLE_OK && http_ok && !state.cancelled && !state.write_failed;

    if (state.cancelled) {
      result.error = "the transfer was cancelled";
      result.error_code = "cancelled";
    } else if (state.write_failed) {
      result.error = "cannot write the download to " + destination_path;
      result.error_code = "permission";
    } else if (code != CURLE_OK) {
      const std::string detail =
          error_buffer[0] != '\0' ? std::string(error_buffer) : curl_easy_strerror(code);
      result.error = "the transfer failed: " + detail;
      result.error_code = "network";
    } else if (!http_ok) {
      result.error = status != 0 ? "the server answered with HTTP " + std::to_string(status)
                                 : "the server did not answer";
      result.error_code = "network";
    }

    if (result.ok && state.body != nullptr) {
      result.body = std::move(body);
    }
    if (!result.ok && !destination_path.empty()) {
      std::remove(destination_path.c_str());  // never leave a partial download behind
    }
    return result;
  }
};

}  // namespace

bool HttpIsAvailable() {
  return true;
}

HttpClient::HttpClient() : pimpl_(std::make_unique<LinuxHttpImpl>()) {}

HttpClient::~HttpClient() = default;

}  // namespace dazzle_updater
