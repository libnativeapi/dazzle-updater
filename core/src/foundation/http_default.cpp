#include "http.h"

// The cross-platform default for the HTTP seam: this platform has no stack yet. It is
// compiled only when platform/<os>/ has no http_<os> file, so a platform that does not
// implement transfers needs no source file at all (see ../CMakeLists.txt).

namespace dazzle {
namespace {

class UnsupportedHttpImpl final : public HttpClient::Impl {
 public:
  HttpResult Get(const std::string&, const std::string&, const HttpProgress&) override {
    HttpResult result;
    result.error = "not supported on this platform";
    result.error_code = "unsupported-platform";
    return result;
  }
};

}  // namespace

bool HttpIsAvailable() {
  return false;
}

HttpClient::HttpClient() : pimpl_(std::make_unique<UnsupportedHttpImpl>()) {}

HttpClient::~HttpClient() = default;

}  // namespace dazzle
