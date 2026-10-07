# dazzle updater

A cross-platform software update framework with its own API: `Updater` finds a new version
through a data source, downloads and verifies it, and hands it to the platform installer.
The default source reads dazzle updater's own [release index](../specs/release-index.md); an
optional `AppcastSource` will read Sparkle appcasts. It is a C++17 library with a C ABI
for bindings.

**Status**: macOS works end to end (check, download, Ed25519 verification, zip install and
relaunch). The optional built-in interface, `UpdaterUI`, has its macOS windows; Windows,
Linux, iOS, Android and OHOS answer `"not supported on this platform"` until their
implementations land ([plan](../specs/updater.md),
[interface plan](../specs/updater-ui.md)).

## Installation

Requires CMake 3.10+ and a C++17 compiler.

```cmake
add_subdirectory(dazzle_updater)
target_link_libraries(your_app PRIVATE dazzle_updater)
```

Options:

| Option | Default | Effect |
| --- | --- | --- |
| `DAZZLE_BUILD_SHARED` | `OFF` | Build a shared library instead of a static one |
| `DAZZLE_WITH_UPDATER_UI` | `ON` | Build `UpdaterUI`'s native windows. `OFF` leaves their implementation, the Markdown renderer and the system UI frameworks out of the library; the entry points stay and answer false, as they do on a platform without windows ([spec](../specs/updater-ui.md)) |

## Quick Start

```cpp
#include <iostream>
#include "dazzle_updater.h"

int main() {
  dazzle_updater::ReleaseIndexSource::GetInstance().SetUrl("https://example.com/myapp/releases.json");

  auto& updater = dazzle_updater::Updater::GetInstance();
  updater.SetPublicKey("pfIShU4dEXqPd5ObYNfDBiQWcXozk7estwzTnF9BamQ=");

  // CheckForUpdates(), DownloadUpdate() and InstallUpdate() block, so call them from a
  // worker thread; read the result on the UI thread.
  if (updater.CheckForUpdates() && updater.IsUpdateAvailable()) {
    std::cout << updater.GetUpdateDisplayVersion() << "\n";
  }
}
```

### Built-in interface

An application that would rather not draw the update interface itself can use `UpdaterUI`:
the same round, in the platform's own windows, with the same source and public key
configured on `Updater` ([spec](../specs/updater-ui.md)).

```cpp
dazzle_updater::UpdaterUI::GetInstance().CheckForUpdatesInBackground();  // after start-up
dazzle_updater::UpdaterUI::GetInstance().CheckForUpdates();              // "Check for Updates…"
```

## Examples

```bash
cmake -S . -B build
cmake --build build
./build/examples/updater_example/updater_example https://example.com/myapp/releases.json <public-key>
./build/examples/updater_c_example/updater_c_example https://example.com/myapp/releases.json <public-key>
```

`examples/updater_ui_example` is the one to run to *see* the module. It is a macOS
application bundle that shows the built-in windows; `demo.sh` next to it builds a local,
signed release of the example itself and runs it, so the whole round can be clicked
through — including "Install and Update", which replaces the demo bundle in place and
relaunches it:

```bash
make demo          # from the repository root; or, from core/:
./examples/updater_ui_example/demo.sh
```

## Tests

```bash
cd build && ctest --output-on-failure      # unit tests
cd .. && ./tests/macos_install_e2e.sh ./build/examples/updater_example/updater_example \
                                      ./build/tests/fixture_signer
```

The end-to-end script builds two throwaway application bundles, serves a signed release
index over HTTP, and checks that version 1.0.0 installs the signed 2.0.0 update in place.

## Contributing

This library is developed as the `core/` directory of the dazzle updater workspace, next to its
language bindings and design specs. See the [workspace README](../README.md).
