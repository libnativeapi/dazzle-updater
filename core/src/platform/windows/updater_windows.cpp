#include "../../updater.h"

#include <windows.h>

namespace dazzle {
namespace {

// WinSparkle convention: the feed URL is stored in the executable as
//   FeedURL APPCAST {"https://example.com/appcast.xml"}
std::string LoadAppcastResource(const wchar_t* name) {
  HRSRC resource = FindResourceW(nullptr, name, L"APPCAST");
  if (!resource) {
    return "";
  }
  HGLOBAL handle = LoadResource(nullptr, resource);
  if (!handle) {
    return "";
  }
  const char* data = static_cast<const char*>(LockResource(handle));
  DWORD size = SizeofResource(nullptr, resource);
  if (!data || size == 0) {
    return "";
  }
  std::string value(data, size);
  // The resource compiler keeps the string literal's terminating null.
  while (!value.empty() && value.back() == '\0') {
    value.pop_back();
  }
  return value;
}

class WindowsUpdaterImpl final : public Updater::Impl {
 public:
  std::string GetConfiguredFeedUrl() const override { return LoadAppcastResource(L"FeedURL"); }
};

}  // namespace

Updater::Updater() : pimpl_(std::make_unique<WindowsUpdaterImpl>()) {}

Updater::~Updater() = default;

}  // namespace dazzle
