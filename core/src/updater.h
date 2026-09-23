#pragma once

#include <memory>
#include <mutex>
#include <string>

namespace dazzle {

/**
 * @brief The application's software updater.
 *
 * Updater checks an appcast feed and installs the updates it announces. The
 * feed format is the Sparkle appcast (an RSS 2.0 document with `sparkle:`
 * extensions), so existing appcasts work unchanged; the API itself is
 * dazzle's own. One instance serves the whole process.
 *
 * Typical usage:
 * @code
 * auto& updater = dazzle::Updater::GetInstance();
 * updater.SetFeedUrl("https://example.com/appcast.xml");
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
   * @brief Set the URL of the appcast feed.
   *
   * @param feed_url The appcast URL, e.g. "https://example.com/appcast.xml";
   *   "" clears it.
   */
  void SetFeedUrl(const std::string& feed_url);

  /**
   * @brief Get the URL of the appcast feed.
   *
   * @return The URL set with SetFeedUrl(), or "" if none has been set.
   */
  std::string GetFeedUrl() const;

  // Prevent copying and moving
  Updater(const Updater&) = delete;
  Updater& operator=(const Updater&) = delete;
  Updater(Updater&&) = delete;
  Updater& operator=(Updater&&) = delete;

  /// The platform seam: each src/platform/<os>/updater_<os> file defines the
  /// subclass. It has no members yet; platform-specific behavior goes here.
  class Impl {
   public:
    virtual ~Impl() = default;
  };

 private:
  Updater();
  ~Updater();

  std::unique_ptr<Impl> pimpl_;
  mutable std::mutex mutex_;
  std::string feed_url_;
};

}  // namespace dazzle
