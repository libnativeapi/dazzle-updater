# dazzle

A Sparkle-compatible software update framework for all major platforms. It reads the
[Sparkle](https://sparkle-project.org) appcast format and honors the configuration keys
Sparkle (macOS) and WinSparkle (Windows) established, so existing feeds and apps work
unchanged. It is a C++17 library with a C ABI for bindings.

🚧 **Work in Progress**: only feed URL configuration is implemented so far.

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
  // Falls back to SUFeedURL (macOS Info.plist) or the FeedURL APPCAST
  // resource (Windows) when no URL was set explicitly.
  std::cout << dazzle::Updater::GetInstance().GetFeedUrl() << "\n";
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
