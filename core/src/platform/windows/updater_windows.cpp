#include "../../updater.h"

namespace dazzle {
namespace {

// No platform-specific behavior yet.
class WindowsUpdaterImpl final : public Updater::Impl {};

}  // namespace

Updater::Updater() : pimpl_(std::make_unique<WindowsUpdaterImpl>()) {}

Updater::~Updater() = default;

}  // namespace dazzle
