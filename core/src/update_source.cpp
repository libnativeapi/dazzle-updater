#include "update_source.h"

#include <map>
#include <mutex>

namespace dazzle_updater {
namespace {

std::mutex& RegistryMutex() {
  static std::mutex mutex;
  return mutex;
}

std::map<std::string, UpdateSource*>& Registry() {
  static std::map<std::string, UpdateSource*> registry;
  return registry;
}

}  // namespace

void RegisterUpdateSource(const std::string& name, UpdateSource* source) {
  if (name.empty() || source == nullptr) {
    return;
  }
  std::lock_guard<std::mutex> lock(RegistryMutex());
  Registry()[name] = source;
}

UpdateSource* FindUpdateSource(const std::string& name) {
  std::lock_guard<std::mutex> lock(RegistryMutex());
  const auto found = Registry().find(name);
  return found == Registry().end() ? nullptr : found->second;
}

}  // namespace dazzle_updater
