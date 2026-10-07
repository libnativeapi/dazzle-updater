#pragma once

#include <cstdint>
#include <functional>
#include <memory>
#include <string>

// Internal HTTP GET, shared by ReleaseIndexSource (the release index) and Updater (the
// installer). It is not listed in API_HEADERS, so codegen ignores it and it reaches no
// binding.
//
// The platform seam is `HttpClient::Impl`: each src/platform/<os>/http_<os> file defines
// the subclass, the constructor and the destructor, on the platform's own stack
// (NSURLSession on macOS and iOS, WinHTTP on Windows, stub elsewhere for now). See
// ../specs/updater.md ("实现分层").

namespace dazzle {

/**
 * @brief Reports the progress of a transfer and lets the caller cancel it.
 *
 * `received` is the number of body bytes received so far, `total` the size the server
 * announced (0 when it does not announce one). Returning false aborts the transfer, which
 * then fails with the `"cancelled"` error code.
 *
 * Called on the thread that called HttpClient::Get().
 */
using HttpProgress = std::function<bool(int64_t received, int64_t total)>;

/**
 * @brief The outcome of one HttpClient::Get().
 */
struct HttpResult {
  bool ok = false;         ///< true when the transfer finished with a 2xx status.
  int status_code = 0;     ///< The HTTP status; 0 when no response arrived.
  std::string body;        ///< The response body, when the request went to memory.
  std::string error;       ///< English description when `ok` is false.
  std::string error_code;  ///< "network", "cancelled", "permission" or "unsupported-platform".
};

/**
 * @brief The platform HTTP stack, behind one blocking GET.
 *
 * One process-wide instance. Updater already serializes the actions that use it, so the
 * seam only has to handle one transfer at a time.
 */
class HttpClient {
 public:
  /**
   * @brief Get the singleton instance of HttpClient.
   *
   * @return Reference to the singleton HttpClient instance
   */
  static HttpClient& GetInstance();

  /**
   * @brief GET a URL on the calling thread.
   *
   * Redirects are followed. When `destination_path` is empty the body is returned in
   * HttpResult::body; otherwise it is written to that path, replacing the file, and a
   * partially written file is removed when the transfer fails.
   *
   * @param url The HTTP(S) address to fetch.
   * @param destination_path The file to write the body to, or "" for memory.
   * @param progress Called as the body arrives; may be empty.
   * @return The transfer's outcome; check `ok` before using `body`.
   */
  HttpResult Get(const std::string& url, const std::string& destination_path,
                 const HttpProgress& progress);

  // Prevent copying and moving
  HttpClient(const HttpClient&) = delete;
  HttpClient& operator=(const HttpClient&) = delete;
  HttpClient(HttpClient&&) = delete;
  HttpClient& operator=(HttpClient&&) = delete;

  /// The platform seam: each src/platform/<os>/http_<os> file defines the subclass, the
  /// constructor and the destructor.
  class Impl {
   public:
    virtual ~Impl() = default;

    /// One blocking GET. The caller has already checked `url`.
    virtual HttpResult Get(const std::string& url, const std::string& destination_path,
                           const HttpProgress& progress) = 0;
  };

 private:
  HttpClient();
  ~HttpClient();

  std::unique_ptr<Impl> pimpl_;
};

/**
 * @brief Whether this build has an HTTP stack behind HttpClient::Get().
 *
 * `system_<os>` reports capabilities from it: a platform whose HTTP implementation the
 * build left out — Linux without libcurl today — can neither check nor download, exactly
 * as if it had no implementation at all. The cross-platform default answers false.
 *
 * @return true when a GET on this platform can reach the network.
 */
bool HttpIsAvailable();

/**
 * @brief Whether `url` is an absolute HTTP or HTTPS address.
 *
 * Sources use it to reject a malformed address before starting a transfer, and to keep
 * the platform stacks away from other schemes (a `file:` URL would let a release index
 * point at local files).
 *
 * @param url The address to check.
 * @return true when it starts with "http://" or "https://" and has a host.
 */
bool IsHttpUrl(const std::string& url);

}  // namespace dazzle
