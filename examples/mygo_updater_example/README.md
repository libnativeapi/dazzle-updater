# MyGo updater example

A desktop application using [MyGo v0.7.4](https://github.com/egoist/mygo/tree/v0.7.4)
and the [Go binding](../../bindings/go/README.md). MyGo draws the form, progress bar
and Markdown release notes. dazzle updater checks the release index, downloads and
verifies the installer, and starts installation.

## Build and run

Requires Go 1.27.3+ (MyGo's minimum), cgo, CMake 3.10+ and a C++17 compiler. Keep the
full repository checkout: `go.mod` uses `../../bindings/go` through a local replacement.

From the workspace root:

```sh
make mygo
cd examples/mygo_updater_example
go run .
```

Enter the release index URL, current version and Ed25519 public key in the window, or
provide them at startup:

```sh
go run . \
  --feed https://example.com/myapp/releases.json \
  --public-key '<your Base64 Ed25519 public key>' \
  --version 1.0.0
```

`DAZZLE_DEMO_FEED` and `DAZZLE_DEMO_PUBLIC_KEY` supply the defaults for those flags.
No server is contacted until you check for updates. In a real application, ship the
public key in the application rather than accepting it from the release feed.

The example uses MyGo's native UI and `Updater` directly. It does not start
`UpdaterUI`, whose GTK 4 and WinUI 3 host requirements differ from MyGo's event loops.
macOS has been built and smoke-tested locally. Linux needs MyGo's GTK 3 desktop
dependencies and a dazzle core built with libcurl; Windows needs a cgo C compiler and
the native DLL on `PATH` or beside the executable. See the binding README for native
build and runtime-library setup. On Windows, run from PowerShell after building the
binding:

```powershell
$env:PATH = "$(Resolve-Path ../../bindings/go/build/lib);$env:PATH"
go run .
```

## Update flow

- **Check for updates** configures the source, current version and signing key. An
  available release shows its version, size and Markdown notes.
- **Download update** downloads and verifies the package. Progress is sampled every
  100 ms; **Cancel** requests cancellation through the core.
- **Install and quit** starts the installer, then calls `mygo.App.Quit()` after the
  core accepts the request. Installation errors leave the application running.

The three blocking actions run on goroutines. All UI state changes return through
`mygo.RunOnMain`; `main` locks its OS thread before starting MyGo. Editing any
configuration field invalidates the displayed release so a new check is required.
The form is disabled during an action. Closing during a download cancels it; closing
during a check or installer launch waits for the action to finish before exiting.

To try installation, package an actual replacement application: a `.app` inside a zip
on macOS, an application directory inside a zip on Linux, or an exe/msi installer on
Windows. Running with `go run` is sufficient for checking and downloading; it is not
a distributable installation target. A packaged app must include the dazzle native
library as described in the [binding's distribution notes](../../bindings/go/README.md#distributing-an-application).

## Verification

From this directory, after building the binding:

```sh
go test -race -count=1 ./...
go vet ./...
go run -race . --smoke
```

The headless UI tests edit the actual form and check that changing configuration
invalidates a downloaded release and that a busy form cannot be edited.

`--smoke` opens a real window and starts a temporary loopback HTTP server. It checks
release metadata, downloads a signed fixture, rejects the same fixture with a wrong
key, and checks the up-to-date state, then exits. It never starts an installer or
changes saved preferences. A desktop session is required. From the workspace root,
`make mygo-smoke` builds and runs this check; it is separate from `make test`.

`main.go` contains the application, `smoke.go` the local smoke fixture, and
`main_test.go` the headless interaction tests.
