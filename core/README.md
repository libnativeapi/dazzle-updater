# dazzle

A cross-platform software update framework. dazzle has its own API; its feed format is the
[Sparkle](https://sparkle-project.org) appcast, so existing appcasts work unchanged. It is a
C++17 library with a C ABI for bindings.

🚧 **Work in Progress**: only the feed URL setting is implemented so far.

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
  auto& updater = dazzle::Updater::GetInstance();
  updater.SetFeedUrl("https://example.com/appcast.xml");
  std::cout << updater.GetFeedUrl() << "\n";
}
```

## Examples

```bash
cmake -S . -B build
cmake --build build
./build/examples/updater_example/updater_example
./build/examples/updater_c_example/updater_c_example
```

## Contributing

This library is developed as the `core/` directory of the dazzle workspace, next to its
language bindings and design specs. See the [workspace README](../README.md).
