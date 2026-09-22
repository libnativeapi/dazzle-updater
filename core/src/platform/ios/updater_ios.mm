#include "../../updater.h"

namespace dazzle {
namespace {

class IosUpdaterImpl final : public Updater::Impl {
 public:
  // There is no standard configuration source on this platform.
  std::string GetConfiguredFeedUrl() const override { return ""; }
};

}  // namespace

Updater::Updater() : pimpl_(std::make_unique<IosUpdaterImpl>()) {}

Updater::~Updater() = default;

}  // namespace dazzle
