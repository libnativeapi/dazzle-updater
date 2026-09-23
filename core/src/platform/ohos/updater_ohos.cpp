#include "../../updater.h"

namespace dazzle {
namespace {

// No platform-specific behavior yet.
class OhosUpdaterImpl final : public Updater::Impl {};

}  // namespace

Updater::Updater() : pimpl_(std::make_unique<OhosUpdaterImpl>()) {}

Updater::~Updater() = default;

}  // namespace dazzle
