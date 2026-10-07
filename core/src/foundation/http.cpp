#include "http.h"

#include <cctype>
#include <cstring>

namespace dazzle_updater {
namespace {

bool StartsWithScheme(const std::string& url, const char* scheme) {
  const size_t length = std::strlen(scheme);
  if (url.size() <= length) {
    return false;
  }
  for (size_t i = 0; i < length; ++i) {
    if (std::tolower(static_cast<unsigned char>(url[i])) != scheme[i]) {
      return false;
    }
  }
  return true;
}

}  // namespace

bool IsHttpUrl(const std::string& url) {
  const char* scheme = nullptr;
  if (StartsWithScheme(url, "https://")) {
    scheme = "https://";
  } else if (StartsWithScheme(url, "http://")) {
    scheme = "http://";
  } else {
    return false;
  }
  const std::string rest = url.substr(std::strlen(scheme));
  const size_t authority_end = rest.find_first_of("/?#");
  return !rest.substr(0, authority_end).empty();
}

HttpClient& HttpClient::GetInstance() {
  static HttpClient instance;
  return instance;
}

HttpResult HttpClient::Get(const std::string& url, const std::string& destination_path,
                           const HttpProgress& progress) {
  if (!IsHttpUrl(url)) {
    HttpResult result;
    result.error = "the address is not an HTTP or HTTPS URL";
    result.error_code = "network";
    return result;
  }
  if (!pimpl_) {
    HttpResult result;
    result.error = "the platform HTTP stack is not available";
    result.error_code = "unsupported-platform";
    return result;
  }
  return pimpl_->Get(url, destination_path, progress);
}

}  // namespace dazzle_updater
