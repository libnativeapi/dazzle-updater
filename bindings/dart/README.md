# dazzle for Dart

Dart bindings for [dazzle](../../README.md), a cross-platform software
update framework. dazzle has its own API; its feed format is the
[Sparkle](https://sparkle-project.org) appcast, so existing appcasts work unchanged.

🚧 **Work in Progress**: only the feed URL setting is implemented so far.

## Requirements

- Dart 3.10+ (the package uses [build hooks](https://dart.dev/tools/hooks) to ship its
  native library)
- CMake 3.10+
- A C++17 compiler (Xcode Command Line Tools on macOS, MSVC on Windows, GCC or Clang on Linux)

The build hook compiles the dazzle C++ core from source the first time you `dart run` or
`dart test`, and bundles it as a native asset. There is no library to copy by hand.

Only the host OS is supported for now: building for iOS or Android from a desktop fails
with an explicit error.

## Installation

The package is not on pub.dev yet. Depend on it from git:

```yaml
dependencies:
  dazzle:
    git:
      url: https://github.com/fastforgedev/dazzle
      path: bindings/dart
```

## Usage

```dart
import 'package:dazzle/dazzle.dart';

void main() {
  final updater = Updater.instance;

  print(updater.feedUrl); // null

  updater.feedUrl = 'https://example.com/appcast.xml';
  print(updater.feedUrl); // https://example.com/appcast.xml

  // null (or '') clears it.
  updater.feedUrl = null;
}
```

`feedUrl` is `null` when no URL has been set.


## Examples

```bash
dart run example/updater_example.dart
```

| Example | Shows |
| --- | --- |
| [updater_example](example/updater_example.dart) | Setting and reading the appcast feed URL |

## Development

```bash
dart pub get
dart test
dart analyze
```

[lib/dazzle.dart](lib/dazzle.dart),
[lib/src/dazzle_bindings.dart](lib/src/dazzle_bindings.dart) and one
`lib/src/<module>.dart` per class are generated from the C++ headers; do not edit them.
Change the header in [core/src/](../../core/src) and run `./codegen` from the repository
root. The string helpers ([lib/src/ffi_string.dart](lib/src/ffi_string.dart)) are
hand-written.

## License

MIT
