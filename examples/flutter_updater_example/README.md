# flutter_updater_example

A Flutter desktop app using the [Dart binding](../../bindings/dart/README.md): it points
dazzle updater at a release index, embeds the signing key, and walks the whole flow — check,
download, verify, install and restart — from one screen.

Beyond the console examples, it shows the binding working inside a real Flutter app: the
Dart binding's build hook compiles the dazzle updater core, and Flutter bundles it into the app
as `Contents/Frameworks/dazzle_updater.framework` with no manual step.

## Run

Requires a Flutter release with build hooks enabled by default (verified with Flutter
3.47), plus the dazzle updater toolchain: CMake and a C++17 compiler.

```bash
flutter run -d macos
```

## Platforms

Only macOS is set up. The Dart binding builds for the host OS only, so iOS and Android
need cross-compiling in its build hook first. To add Windows or Linux, run
`flutter create --platforms=windows,linux .` in this directory.
