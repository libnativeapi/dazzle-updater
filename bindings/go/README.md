# dazzle updater for Go

Go bindings for [dazzle updater](../../README.md), using cgo over the generated C ABI.
The package is `dazzleupdater`; the module is
`github.com/fastforgedev/dazzle/bindings/go`.

## Requirements

- Go 1.22+ with `CGO_ENABLED=1` and a cgo-compatible C compiler
- CMake 3.10+ and a C++17 toolchain
- A full repository checkout: this module builds `../../core` from source

The native build targets the host OS and architecture. macOS needs Xcode Command Line
Tools. Linux uses libcurl for HTTP and optionally GTK 4 for the built-in windows (for
example, `pkg-config`, `libcurl4-openssl-dev` and `libgtk-4-dev` on Debian/Ubuntu).
Without libcurl the core builds, but checks return `"unsupported-platform"`.
Windows needs a cgo compiler such as MinGW-w64 in addition to the CMake C++ toolchain;
the DLL can be built with MSVC because only the C ABI crosses into Go. WinUI 3 windows
add the core's Windows App SDK requirements.

## Build and use locally

From the repository root:

```sh
make go
```

Or from this directory:

```sh
go generate ./...
go test -count=1 ./...
go run ./examples/updater_example
```

`go generate` builds the shared native library in `build/lib` with CMake. Run it again
after changing native sources. Go does not run this step during `go build` or `go test`;
the API wrappers themselves are generated separately by the repository's `./codegen`.

On Windows, add `build/lib` to `PATH` before running tests or programs, so Windows can
load `dazzle_updater.dll`. For example, in PowerShell from this directory:

```powershell
go generate ./...
$env:PATH = "$PWD\build\lib;$env:PATH"
go test -count=1 ./...
go run ./examples/updater_example
```

To use it in another Go module, build the library in a full checkout and add a local
replacement in the consuming module's `go.mod`:

```go
require github.com/fastforgedev/dazzle/bindings/go v0.0.0

replace github.com/fastforgedev/dazzle/bindings/go => /path/to/dazzle/bindings/go
```

A standalone `go get` is not supported yet: Go's module download contains this
subdirectory, without the sibling `core/` sources and headers.

```go
package main

import (
    "log"

    dazzleupdater "github.com/fastforgedev/dazzle/bindings/go"
)

func main() {
    source := dazzleupdater.ReleaseIndexSourceInstance()
    source.SetURL("https://example.com/myapp/releases.json")

    updater := dazzleupdater.UpdaterInstance()
    updater.SetCurrentVersion("1.4.0")
    updater.SetPublicKey("11qYAYKxCrfVS/7TyWQHOg7hcvPapiMlrwIaaPcHURo=")

    if !updater.CheckForUpdates() {
        log.Fatalf("%s: %s", updater.ErrorCode(), updater.ErrorMessage())
    }
    if updater.IsUpdateAvailable() {
        log.Printf("Update available: %s", updater.UpdateDisplayVersion())
    }
}
```

`CheckForUpdates`, `DownloadUpdate` and `InstallUpdate` block. Call them from a worker
goroutine in UI applications. Their `bool` results and `ErrorCode`/`ErrorMessage`
preserve the core's API. Strings are UTF-8, empty/native NULL strings become `""`, and
input strings stop at the first NUL. The binding frees native strings after copying
them. All wrapper values refer to the process-wide native singletons.

`UpdaterUIInstance()` exposes the optional built-in windows. The application must own
the platform UI event loop; a goroutine alone does not provide one. Follow the core's
[UpdaterUI requirements](../../specs/updater-ui.md), including GTK initialization on
Linux and the first call on the WinUI 3 XAML thread on Windows. Go UI applications must
keep their UI event loop on the appropriate OS thread (typically with
`runtime.LockOSThread` in their UI framework's entry point).

To build without native windows, configure once before `go generate`; CMake retains
the option:

```sh
cmake -S . -B build -DDAZZLE_WITH_UPDATER_UI=OFF
go generate ./...
```

## Distributing an application

The executable needs the shared native library. Development builds locate it in this
checkout's `build/lib`. For distribution:

- **macOS:** copy `libdazzle_updater.dylib` beside the executable and add its runtime
  search path when building: `go build -ldflags='-extldflags=-Wl,-rpath,@executable_path'`.
  The dylib's install name is `@rpath/libdazzle_updater.dylib`. For an app bundle, place
  the dylib in `Contents/Frameworks` and use `@executable_path/../Frameworks` instead.
- **Linux:** copy `libdazzle_updater.so` beside the executable; the binding includes
  `$ORIGIN` in the runtime search path. The destination also needs the system libraries
  linked by CMake, including libcurl and GTK 4 when enabled.
- **Windows:** copy `dazzle_updater.dll` beside the executable, together with its native
  runtime dependencies.

Cross-compilation is not configured by these build commands. Use matching Go and
native toolchains when building for another architecture or OS.

## Development

The [MyGo desktop example](../../examples/mygo_updater_example/README.md) shows the
full update flow with a responsive native UI, download cancellation and Markdown notes.

`updater.go`, `release_index_source.go` and `updater_ui.go` are generated from the C++
headers. Change the headers or [Go emitter](../../tools/codegen/emit_go.py) and run
`./codegen` from the repository root. `native.go`, `ffi_string.go`, `doc.go`, the CMake
project, tests and example are hand-written.

```sh
go test -race -count=1 ./...
go vet ./...
```

Use `-count=1` after rebuilding the native library: Go's test cache does not track
changes to the external shared library.

Tests use a local HTTP server to check release metadata, signed downloads and signature
rejection. They also cover configuration, string ownership/conversion, action errors,
concurrent callers and in-memory UI settings. They do not install updates or open
windows. Tests that need HTTP skip when the native build lacks an HTTP implementation.

## License

MIT
