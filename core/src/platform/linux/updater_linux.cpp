#include "../../updater.h"

namespace dazzle {
namespace {

// No platform-specific behavior yet.
class LinuxUpdaterImpl final : public Updater::Impl {};

}  // namespace

Updater::Updater() : pimpl_(std::make_unique<LinuxUpdaterImpl>()) {}

Updater::~Updater() = default;

}  // namespace dazzle
