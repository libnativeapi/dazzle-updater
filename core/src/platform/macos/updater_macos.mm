#include "../../updater.h"

namespace dazzle {
namespace {

// No platform-specific behavior yet.
class MacosUpdaterImpl final : public Updater::Impl {};

}  // namespace

Updater::Updater() : pimpl_(std::make_unique<MacosUpdaterImpl>()) {}

Updater::~Updater() = default;

}  // namespace dazzle
