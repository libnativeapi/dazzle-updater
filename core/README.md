# dazzle

A cross-platform software update framework with its own API: `Updater` finds a new version
through a data source, downloads and verifies it, and hands it to the platform installer.
The default source reads dazzle's own [release index](../specs/release-index.md); an
optional `AppcastSource` will read Sparkle appcasts. It is a C++17 library with a C ABI
for bindings.

**Status**: macOS works end to end (check, download, Ed25519 verification, zip install and
relaunch). Windows, Linux, iOS, Android and OHOS answer
`"not supported on this platform"` until their implementations land
([plan](../specs/updater.md)).

## Installation

Requires CMake 3.10+ and a C++17 compiler.

```cmake
add_subdirectory(dazzle)
target_link_libraries(your_app PRIVATE dazzle)
```

## Quick Start

```cpp
#include <iostream>
#include "dazzle.h"

int main() {
  dazzle::ReleaseIndexSource::GetInstance().SetUrl("https://example.com/myapp/releases.json");

  auto& updater = dazzle::Updater::GetInstance();
  updater.SetPublicKey("pfIShU4dEXqPd5ObYNfDBiQWcXozk7estwzTnF9BamQ=");

  // CheckForUpdates(), DownloadUpdate() and InstallUpdate() block, so call them from a
  // worker thread; read the result on the UI thread.
  if (updater.CheckForUpdates() && updater.IsUpdateAvailable()) {
    std::cout << updater.GetUpdateDisplayVersion() << "\n";
  }
}
```

## Examples

```bash
cmake -S . -B build
cmake --build build
./build/examples/updater_example/updater_example https://example.com/myapp/releases.json <public-key>
./build/examples/updater_c_example/updater_c_example https://example.com/myapp/releases.json <public-key>
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

This library is developed as the `core/` directory of the dazzle workspace, next to its
language bindings and design specs. See the [workspace README](../README.md).
