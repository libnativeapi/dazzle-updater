#include "../../updater.h"

namespace dazzle {
namespace {

// No platform-specific behavior yet.
class IosUpdaterImpl final : public Updater::Impl {};

}  // namespace

Updater::Updater() : pimpl_(std::make_unique<IosUpdaterImpl>()) {}

Updater::~Updater() = default;

}  // namespace dazzle
