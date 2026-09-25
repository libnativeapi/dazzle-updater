#pragma once

#include <mutex>
#include <string>

namespace dazzle {

/**
 * @brief The release index data source.
 *
 * Reads dazzle's own release index: a `releases.json` JSON document at an HTTP(S)
 * address that lists the application's versions and, for each version, the installer of
 * every platform it ships. Updater asks it for the release to offer this machine; the
 * address lives here, not on Updater, which knows no data format.
 *
 * The format is specified in `specs/release-index.md`.
 *
 * Typical usage:
 * @code
 * auto& source = dazzle::ReleaseIndexSource::GetInstance();
 * source.SetUrl("https://example.com/myapp/releases.json");
 * @endcode
 */
class ReleaseIndexSource {
 public:
  /**
   * @brief Get the singleton instance of ReleaseIndexSource.
   *
   * @return Reference to the singleton ReleaseIndexSource instance
   */
  static ReleaseIndexSource& GetInstance();

  /**
   * @brief Set the address of the release index.
   *
   * @param url The HTTP(S) address of a release index, e.g.
   *   "https://example.com/myapp/releases.json"; "" clears it, and a check then fails
   *   with the `"invalid-index"` error code.
   */
  void SetUrl(const std::string& url);

  /**
   * @brief Get the address of the release index.
   *
   * @return The address set with SetUrl(), or "" if none has been set.
   */
  std::string GetUrl() const;

  // Prevent copying and moving
  ReleaseIndexSource(const ReleaseIndexSource&) = delete;
  ReleaseIndexSource& operator=(const ReleaseIndexSource&) = delete;
  ReleaseIndexSource(ReleaseIndexSource&&) = delete;
  ReleaseIndexSource& operator=(ReleaseIndexSource&&) = delete;

 private:
  ReleaseIndexSource();
  ~ReleaseIndexSource();

  mutable std::mutex mutex_;
  std::string url_;
};

}  // namespace dazzle
