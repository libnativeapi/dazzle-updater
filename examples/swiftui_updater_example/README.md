# SwiftUI host for native UpdaterUI

A macOS 13+ SwiftUI application using the [Swift binding](../../bindings/swift/README.md)
to open dazzle updater’s built-in **AppKit update windows**. SwiftUI provides only the
host window and configuration form. `UpdaterUI.shared` owns checking, release notes,
download progress, cancellation, signature errors, installation and relaunch.

## Build and run

Requires macOS, Swift 6+, Xcode Command Line Tools, CMake 3.12+, Python 3.9+ and a full
repository checkout.

```sh
make swiftui
open examples/swiftui_updater_example/build/DazzleUpdaterSwiftUI.app
```

The build explicitly enables `DAZZLE_WITH_UPDATER_UI=ON`, packages the executable and
native dylib in a self-contained `.app`, applies an ad-hoc signature, and runs four host
model tests. This local example is not sandboxed or notarized; distribution requires
your own signing setup.

The build selects the macOS SDK from the active Xcode (`xcode-select`) for both Swift
and CMake while keeping the minimum OS at 13.0. It explicitly passes the SDK version
to the linker and verifies both Mach-O build-version records before packaging, so
AppKit can adopt the current system appearance instead of treating the host as an
application linked with the macOS 13 SDK. Select the latest installed Xcode to use
the latest SDK; the SDK version is not hard-coded in the script.

Enter a release index URL, current version and Ed25519 public key, then click
**Check for updates…**. The host calls:

```swift
ReleaseIndexSource.shared.url = feed
Updater.shared.currentVersion = version
Updater.shared.publicKey = publicKey
UpdaterUI.shared.checkForUpdates()
```

**Check in background** calls `checkForUpdatesInBackground()`, including the native
first-run permission prompt and saved automatic-check/download preferences.
**Reset preferences** calls `resetPreferences()`. While a native window is visible,
configuration and reset controls are disabled; **Show update window** brings it forward.
The example never calls `Updater`’s blocking actions alongside `UpdaterUI`.

The environment variables `DAZZLE_DEMO_FEED`, `DAZZLE_DEMO_PUBLIC_KEY` and
`DAZZLE_DEMO_VERSION` override the launch configuration when running the bundle’s
executable directly. Otherwise the current version comes from `CFBundleShortVersionString`.
Production apps should ship their trusted public key instead of exposing an editable key.

## Full native update flow

```sh
make swiftui-demo
# Or start with another shared server scenario:
make swiftui-demo SCENARIO=slow-download
```

The script prepares a disposable 1.0.0 app, a signed 2.0.0 replacement, and a loopback
server. The native update window opens automatically:

1. Read the release notes in the AppKit update window.
2. Click **Install Update** to download and verify the package. Native progress and
   cancellation controls manage the transfer.
3. Click **Install and Relaunch** after verification. UpdaterUI asks AppKit to quit,
   and the core replaces and relaunches the temporary application.
4. The SwiftUI host now shows **v2.0.0** and the native check reports it is up to date.

The script writes example-specific `DazzleUpdaterDemoFeedURL` and `DazzleUpdaterDemoPublicKey` entries
into both bundles, preserving configuration across relaunches. These are host settings,
not core API conventions. The workspace app in `build/` remains at 1.0.0.

Keep the terminal open across restarts. The printed
[shared test console](../../tools/test_server/README.md) switches between 21 scenarios,
including slow transfers and rejected signatures. Close the current update window and
check again after switching. Press Ctrl-C to close the temporary app and remove its
server, files, cache and update preferences. Rerun the demo to start again at 1.0.0.

## Verification

```sh
make swiftui        # Build, code-sign verification and four host model tests
make swiftui-smoke  # Interactive native-window install verification
```

The smoke target opens the real AppKit windows. **Click through Install Update and
Install and Relaunch within three minutes.** The script passes only after the on-disk
bundle reports 2.0.0 and a new process reports a successful launch. The test operates
only on its disposable application. It is intentionally separate from `make test`.

Model tests cover foreground/background forwarding to UpdaterUI, preserving configuration
while a native window is open, preference reset and unavailable-UI reporting. The core
and Swift binding tests cover underlying update and signature operations.

`Sources/UpdaterDemo/UpdaterModel.swift` is the host adapter; `Sources/SwiftUIUpdaterExample/`
contains the SwiftUI host. `build.py` packages the app and `demo.py` reuses the repository’s
scenario server and signer.
