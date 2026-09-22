#import <Foundation/Foundation.h>

#include "../../updater.h"

namespace dazzle {
namespace {

class MacosUpdaterImpl final : public Updater::Impl {
 public:
  std::string GetConfiguredFeedUrl() const override {
    @autoreleasepool {
      id value = [[NSBundle mainBundle] objectForInfoDictionaryKey:@"SUFeedURL"];
      if (![value isKindOfClass:[NSString class]]) {
        return "";
      }
      const char* utf8 = [(NSString*)value UTF8String];
      return utf8 ? utf8 : "";
    }
  }
};

}  // namespace

Updater::Updater() : pimpl_(std::make_unique<MacosUpdaterImpl>()) {}

Updater::~Updater() = default;

}  // namespace dazzle
