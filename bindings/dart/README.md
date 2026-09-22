# dazzle for Dart

Dart bindings for [dazzle](../../README.md), a Sparkle-compatible software update
framework. dazzle reads the [Sparkle](https://sparkle-project.org) appcast format and
honors the configuration keys Sparkle (macOS) and WinSparkle (Windows) established, so
existing feeds and apps work unchanged.

🚧 **Work in Progress**: only feed URL configuration is implemented so far.

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

  // Falls back to the app's configuration when no URL was set.
  print(updater.feedUrl);

  updater.feedUrl = 'https://example.com/appcast.xml';
  print(updater.feedUrl); // https://example.com/appcast.xml

  // null (or '') goes back to the app's configuration.
  updater.feedUrl = null;
}
```

`feedUrl` is `null` when nothing is configured.

## Configuration

Without setting `feedUrl`, the feed URL comes from the app's packaging, using the keys
Sparkle and WinSparkle already defined:

| Platform | Source |
| --- | --- |
| macOS | `SUFeedURL` in the app bundle's `Info.plist` |
| Windows | `FeedURL` resource of type `APPCAST` in the executable |
| Linux, Android, iOS, OHOS | none; only setting `feedUrl` applies |

## Examples

```bash
dart run example/updater_example.dart
```

| Example | Shows |
| --- | --- |
| [updater_example](example/updater_example.dart) | Reading and overriding the appcast feed URL |

## Development

```bash
dart pub get
dart test
dart analyze
```

The C ABI declarations in
[lib/src/dazzle_bindings.dart](lib/src/dazzle_bindings.dart) are hand-written: when a
header in [core/src/capi/](../../core/src/capi) changes, update them in the same commit.

## License

MIT
