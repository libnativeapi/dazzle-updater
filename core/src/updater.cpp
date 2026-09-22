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
  {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!feed_url_.empty()) {
      return feed_url_;
    }
  }
  return pimpl_->GetConfiguredFeedUrl();
}

}  // namespace dazzle
