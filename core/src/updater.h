#pragma once

#include <memory>
#include <mutex>
#include <string>

namespace dazzle {

/**
 * @brief The application's software updater.
 *
 * Updater checks an appcast feed in the Sparkle format (an RSS 2.0 document
 * with `sparkle:` extensions) and installs the updates it announces. One
 * instance serves the whole process.
 *
 * Configuration follows the conventions Sparkle and WinSparkle established,
 * so an app that already ships them works unchanged:
 * - macOS: the main bundle's Info.plist (`SUFeedURL`, ...)
 * - Windows: the executable's resources (`FeedURL` of type `APPCAST`, ...)
 *
 * Typical usage:
 * @code
 * auto& updater = dazzle::Updater::GetInstance();
 * std::string feed_url = updater.GetFeedUrl();
 * @endcode
 */
class Updater {
 public:
  /**
   * @brief Get the singleton instance of Updater.
   *
   * @return Reference to the singleton Updater instance
   */
  static Updater& GetInstance();

  /**
   * @brief Set the URL of the appcast feed, overriding the app's
   * configuration.
   *
   * Pass "" to go back to the URL the app's configuration provides.
   *
   * @param feed_url The appcast URL, e.g. "https://example.com/appcast.xml"
   */
  void SetFeedUrl(const std::string& feed_url);

  /**
   * @brief Get the URL of the appcast feed.
   *
   * Returns the URL set with SetFeedUrl(), falling back to the one the app's
   * configuration provides.
   *
   * @return The appcast URL, or "" if none is configured.
   *
   * @note Platform availability:
   * - macOS: ✅ Fully supported - Falls back to Info.plist `SUFeedURL`.
   * - Windows: ✅ Fully supported - Falls back to the `FeedURL` resource of
   *   type `APPCAST` (the WinSparkle convention).
   * - Linux: ⚠️ Partial - No configuration source; only SetFeedUrl() applies.
   * - Android: ⚠️ Partial - No configuration source; only SetFeedUrl() applies.
   * - iOS: ⚠️ Partial - No configuration source; only SetFeedUrl() applies.
   * - OHOS: ⚠️ Partial - No configuration source; only SetFeedUrl() applies.
   */
  std::string GetFeedUrl() const;

  // Prevent copying and moving
  Updater(const Updater&) = delete;
  Updater& operator=(const Updater&) = delete;
  Updater(Updater&&) = delete;
  Updater& operator=(Updater&&) = delete;

  class Impl {
   public:
    virtual ~Impl() = default;

    /// The feed URL the app's platform configuration provides, or "".
    virtual std::string GetConfiguredFeedUrl() const = 0;
  };

 private:
  Updater();
  ~Updater();

  std::unique_ptr<Impl> pimpl_;
  mutable std::mutex mutex_;
  std::string feed_url_;
};

}  // namespace dazzle
