# dazzle updater

A cross-platform software update framework. dazzle updater has its own API: the default source
reads dazzle updater's own [release index](specs/release-index.md), and an optional `AppcastSource`
will read [Sparkle](https://sparkle-project.org) appcasts. This
workspace holds the C++ core library, its language bindings and the design specs together.
The C++ namespace, native library and Rust/Dart packages are named `dazzle_updater`;
the C ABI uses the `dazzle_` prefix, the C# package and namespace are `DazzleUpdater`,
and the Go package is `dazzleupdater`.

| Directory | Contents |
| --- | --- |
| [core/](core) | The C++17 core library and its C ABI — see [core/README.md](core/README.md) |
| [bindings/](bindings) | Language bindings: Rust, Dart, C#, Go |
| [examples/](examples) | Complete apps on a UI framework (Flutter, WinUI 3, MyGo) |
| [specs/](specs) | Design rules for `core/` |

```bash
make test
```
