# dazzle for Dart

Dart bindings for [dazzle](../../README.md), a cross-platform software
update framework with its own API. The default source reads dazzle's own
[release index](../../specs/release-index.md); an optional `AppcastSource` will read
[Sparkle](https://sparkle-project.org) appcasts.

**Status**: macOS works end to end (check, download, Ed25519 verification, install). Other
platforms answer `"not supported on this platform"` until their implementations land
([plan](../../specs/updater.md)).

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
import 'dart:isolate';

import 'package:dazzle/dazzle.dart';

Future<void> main() async {
  ReleaseIndexSource.instance.url = 'https://example.com/myapp/releases.json';

  final updater = Updater.instance;
  updater.publicKey = 'pfIShU4dEXqPd5ObYNfDBiQWcXozk7estwzTnF9BamQ=';

  // checkForUpdates, downloadUpdate and installUpdate block: run them in a worker
  // isolate so the UI keeps painting, and read the result on the UI isolate.
  final found = await Isolate.run(() => updater.checkForUpdates());
  if (found && updater.isUpdateAvailable) {
    print(updater.updateDisplayVersion);
  }
}
```

`checkForUpdates()` returns `false` for a failed check; `errorCode` then says why, and
`errorMessage` carries the English text. An empty string comes back as `null`.

## Examples

```bash
dart run example/updater_example.dart
```

| Example | Shows |
| --- | --- |
| [updater_example](example/updater_example.dart) | The configured source, version and key |

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
