# dazzle updater for Swift

Swift Package Manager bindings for [dazzle updater](../../README.md). The
`DazzleUpdater` module wraps the generated C ABI and exposes `Updater`,
`ReleaseIndexSource` and `UpdaterUI`.

## Requirements

- Swift 6.0+
- CMake 3.12+ and a C++17 toolchain
- A full repository checkout: the native build uses `../../core`
- Python 3.9+ for the loopback HTTP test fixture

The build helper targets the host OS and architecture: macOS 13+ or Linux. macOS
needs Xcode Command Line Tools. Linux uses libcurl for HTTP and optionally GTK 4
for built-in windows (for example, `pkg-config`, `libcurl4-openssl-dev` and
`libgtk-4-dev` on Debian/Ubuntu). Without libcurl the core builds, but update
checks return `"unsupported-platform"`. Windows, iOS and cross-compilation are
not configured by this package's build helper.

## Build and use locally

From the repository root:

```sh
make swift
```

Or from this directory:

```sh
./build-native.sh
swift test
swift run updater-example
```

`build-native.sh` builds the shared native library in `build/lib` using CMake.
Run it again after changing native sources. SwiftPM does not run CMake during
`swift build` or `swift test`. The Swift wrappers are generated separately by
the repository's `./codegen`.

To consume this package, build the native library in a full checkout, then add
a local package dependency to your application's `Package.swift`:

```swift
dependencies: [
    .package(path: "/path/to/dazzle-updater/bindings/swift"),
],
targets: [
    .executableTarget(
        name: "MyApp",
        dependencies: [.product(name: "DazzleUpdater", package: "swift")]
    ),
],
```

Set the consuming package's macOS minimum to 13 or later. In Xcode, add
`bindings/swift` as a local package and select the `DazzleUpdater` library product.
The native library must already be built for the application's architecture.

This is a local workspace package, not a standalone remote SwiftPM dependency:
the repository root has no `Package.swift`, and this subdirectory needs the
sibling core sources. Its linker search and runtime paths use SwiftPM
[`unsafeFlags`](https://docs.swift.org/package-manager/PackageDescription/PackageDescription.html),
which also restrict its use as a versioned dependency. Local path consumption
is covered by the development workflow.

```swift
import DazzleUpdater

ReleaseIndexSource.shared.url = "https://example.com/myapp/releases.json"
let updater = Updater.shared
updater.currentVersion = "1.4.0"
updater.publicKey = "11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo="

// Run blocking actions on a worker queue in a UI application.
if updater.checkForUpdates() {
    if updater.isUpdateAvailable {
        print("Update available:", updater.updateDisplayVersion ?? "")
    }
} else {
    print(updater.errorCode ?? "", updater.errorMessage ?? "")
}
```

The wrappers are stateless `Sendable` final classes, with one `shared` instance
per class. The core synchronizes native state. `checkForUpdates()`,
`downloadUpdate()` and `installUpdate()` are synchronous and block the calling
thread; dispatch them to a worker queue in a UI application. Their Boolean
results and `errorCode` / `errorMessage` preserve the core's API.

Strings use UTF-8. Empty/native NULL results become `nil`; setting a string
property to `nil` passes an empty string. Input strings stop at the first NUL.
Native results are copied and freed with `dazzle_free_c_str`. Installer sizes
use `Int64`, progress uses `Double`, and names use Swift casing (`url`,
`updateReleaseNotesURL`, `isUpdateAvailable`).

`UpdaterUI.shared` exposes the optional built-in windows. The application must
own the platform UI event loop: AppKit on macOS, or an initialized GTK 4 host
and its main loop on Linux. Follow the core's
[UpdaterUI requirements](../../specs/updater-ui.md). The binding does not create
an application or event loop.

To build without native windows:

```sh
./build-native.sh -DDAZZLE_WITH_UPDATER_UI=OFF
```

CMake retains the option; pass `-DDAZZLE_WITH_UPDATER_UI=ON` to enable it again.
The helper defaults the macOS deployment target to 13.0 to match `Package.swift`.

## Distributing an application

Development executables find the shared library in this checkout's `build/lib`.
Distribute the library with your executable and configure a runtime search path:

- **macOS:** embed and sign `libdazzle_updater.dylib` in the app's
  `Contents/Frameworks`, and add `@executable_path/../Frameworks` to the app's
  Runpath Search Paths. Its install name is `@rpath/libdazzle_updater.dylib`.
  For a command-line program, place it beside the executable and build with
  `swift build -c release -Xlinker -rpath -Xlinker @executable_path`.
- **Linux:** place `libdazzle_updater.so` beside the executable and build with
  `swift build -c release -Xlinker -rpath -Xlinker '$ORIGIN'`. The destination
  also needs Swift's runtime and the system libraries linked by CMake,
  including libcurl and GTK 4 when enabled.

## Development

The [SwiftUI example](../../examples/swiftui_updater_example/README.md) demonstrates
the native UpdaterUI flow in a macOS SwiftUI host, including download cancellation,
signature verification, installation and relaunch. Run `make swiftui-demo` from the repository root.

`Sources/DazzleUpdater/Updater.swift`, `ReleaseIndexSource.swift` and
`UpdaterUI.swift` are generated from the C++ headers. Change the headers or
[Swift emitter](../../tools/codegen/emit_swift.py), then run `./codegen` at the
repository root. The package manifest, C module map and shim, string helper,
CMake project, build script, example and tests are hand-written.

Tests cover singleton configuration, UTF-8/NULL/NUL handling, concurrent callers,
action errors, in-memory UI settings, release metadata above 32 bits, a signed
download and rejection with the wrong key. HTTP tests use a local Python server
and RFC 8032 test vectors; they never install an update or open windows. Tests
requiring HTTP skip when the native core lacks an HTTP implementation.

## License

MIT
