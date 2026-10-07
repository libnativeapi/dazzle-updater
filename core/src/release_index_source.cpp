#include "release_index_source.h"

#include "foundation/http.h"
#include "release_index_parse.h"
#include "update_source.h"

#include <string>

namespace dazzle_updater {
namespace {

/// The UpdateSource the release index module registers as "release-index": fetch the
/// document, then let the parser pick the release.
class ReleaseIndexFinder final : public UpdateSource {
 public:
  bool FindUpdate(const UpdateQuery& query, Release* release, bool* found, std::string* error,
                  std::string* error_code) override {
    *found = false;
    const std::string url = ReleaseIndexSource::GetInstance().GetUrl();
    if (url.empty()) {
      *error = "no release index url is set";
      *error_code = "invalid-index";
      return false;
    }
    const HttpResult response = HttpClient::GetInstance().Get(url, "", HttpProgress());
    if (!response.ok) {
      *error = response.error;
      *error_code = response.error_code.empty() ? "network" : response.error_code;
      return false;
    }
    return ParseReleaseIndex(response.body, query, release, found, error, error_code);
  }
};

/// Registers the module during static initialization, so Updater::SetSource() can name it.
struct ReleaseIndexSourceRegistration {
  ReleaseIndexSourceRegistration() {
    static ReleaseIndexFinder finder;
    RegisterUpdateSource("release-index", &finder);
  }
};

const ReleaseIndexSourceRegistration kRegistration;

}  // namespace

ReleaseIndexSource& ReleaseIndexSource::GetInstance() {
  static ReleaseIndexSource instance;
  return instance;
}

ReleaseIndexSource::ReleaseIndexSource() = default;

ReleaseIndexSource::~ReleaseIndexSource() = default;

void ReleaseIndexSource::SetUrl(const std::string& url) {
  std::lock_guard<std::mutex> lock(mutex_);
  url_ = url;
}

std::string ReleaseIndexSource::GetUrl() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return url_;
}

}  // namespace dazzle_updater
