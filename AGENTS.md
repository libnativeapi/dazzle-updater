# dazzle workspace

A Sparkle-compatible software update framework. The workspace holds the C++ core library,
its language bindings and the design specs together. `core/` and `bindings/*` are plain
directories; each can be split into its own repository later
(`git subtree split --prefix=core`) without changing the layout.

## Layout

```
core/               # the C++ core library (layers described in core/AGENTS.md)
bindings/           # language bindings: rust/, dart/, csharp/
specs/              # design rules for core/
Makefile            # make build | examples | rust | dart | csharp | test | clean
```

Planned but not yet present: `tools/codegen/` and a `./codegen` entry point that generate
the C ABI and the bindings from the C++ headers. Until then `core/src/capi/` and the
bindings' FFI layers are hand-written; a change to a C header in `core/src/capi/` must
update `bindings/rust/src/ffi.rs`, `bindings/dart/lib/src/dazzle_bindings.dart` and
`bindings/csharp/src/NativeMethods.cs` in the same commit.

## Architecture

- `core` — the source of truth for the public API. Interface layer `core/src/*.h`,
  per-platform implementations under `core/src/platform/<os>/` (macOS, Windows, Linux,
  Android, iOS, OHOS), C ABI under `core/src/capi/`, single public include
  `core/include/dazzle.h`.
- `bindings/*` — each binding wraps the C ABI from `core/` and builds `core/` from source
  with CMake: Rust links the static library; Dart (as a native asset) and C# (next to the
  assembly) ship the shared library (`-DDAZZLE_BUILD_SHARED=ON`). See bindings/README.md.

## Design specs

Start at [specs/README.md](specs/README.md) before adding or reshaping public API in
`core/src/`.

## Conventions

- Verify a core change with `make test`: it builds and runs the core examples and the
  Rust, Dart and C# tests.
- Do not add Co-Authored-By trailers to commits.
