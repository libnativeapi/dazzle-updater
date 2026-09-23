#include "updater.h"

namespace dazzle {

Updater& Updater::GetInstance() {
  static Updater instance;
  return instance;
}

void Updater::SetFeedUrl(const std::string& feed_url) {
  std::lock_guard<std::mutex> lock(mutex_);
  feed_url_ = feed_url;
}

std::string Updater::GetFeedUrl() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return feed_url_;
}

}  // namespace dazzle
