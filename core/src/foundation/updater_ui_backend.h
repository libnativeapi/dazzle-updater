#pragma once

#include "../updater_ui_state.h"

// The seam behind UpdaterUI's windows: the cross-platform state machine asks for the
// platform's backend, and the build supplies exactly one implementation.
//
// The implementation lives in platform/<os>/updater_ui_backend_<os>.*; a platform with
// no such file gets the cross-platform default
// (foundation/updater_ui_backend_default.cpp), which reports itself unavailable. The
// same default is compiled when the library is built with DAZZLE_WITH_UPDATER_UI=OFF,
// which is why disabling the UI changes no source file, only CMakeLists.txt.
//
// This is a seam of object rather than of free functions because a backend owns windows:
// the functions in system.h take what they need per call, while these window objects
// outlive a call. The interface itself is internal either way — no public class holds a
// platform seam (see ../specs/README.md).

namespace dazzle_updater {
namespace internal {

/// The backend for this build and this platform; never null, never throws.
UiBackend& GetUiBackend();

}  // namespace internal
}  // namespace dazzle_updater
