#include "../../updater.h"

namespace dazzle {
namespace {

// No platform-specific behavior yet.
class AndroidUpdaterImpl final : public Updater::Impl {};

}  // namespace

Updater::Updater() : pimpl_(std::make_unique<AndroidUpdaterImpl>()) {}

Updater::~Updater() = default;

}  // namespace dazzle
