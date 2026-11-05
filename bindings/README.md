# bindings

Language bindings over the C ABI in `core/src/capi/`. Each binding is a single package
that builds `core/` from source with CMake, so a working C++17 toolchain and CMake are
required.

| Directory | Package | Status |
| --- | --- | --- |
| [rust/](rust/README.md) | crate `dazzle_updater`; links the static library | ✅ |
| [dart/](dart/README.md) | package `dazzle_updater` (`dart:ffi`); a build hook bundles the shared library as a native asset | ✅ host OS only |
| [csharp/](csharp/README.md) | NuGet package `DazzleUpdater` (`LibraryImport`); MSBuild builds the shared library and copies it next to the assembly | ✅ host OS only |
| [go/](go/README.md) | package `dazzleupdater` (`cgo`); `go generate` builds the shared library | ✅ host OS only |

Each binding's C ABI declarations (`rust/src/ffi.rs`, `dart/lib/src/dazzle_updater_bindings.dart`,
`csharp/src/NativeMethods.cs`) and wrapper classes (including Go's `go/<module>.go`)
are generated from the C++ headers by
`./codegen` at the repository root; see [tools/codegen](../tools/codegen/README.md). The
string helpers and each package's entry file are hand-written.

Examples sit where each language expects them, one per module:

| Example | Rust | Dart | C# | Go |
| --- | --- | --- | --- | --- |
| Configure the updater and release index | `rust/examples/updater_example.rs` | `dart/example/updater_example.dart` | `csharp/examples/UpdaterExample` | `go/examples/updater_example` |

.NET needs a separate project per program, so the C# example and tests are their own
projects under `csharp/examples/` and `csharp/tests/`, referencing `DazzleUpdater.csproj`.

Each binding's README covers installation, usage and development:
[rust/README.md](rust/README.md), [dart/README.md](dart/README.md),
[csharp/README.md](csharp/README.md), [go/README.md](go/README.md).
