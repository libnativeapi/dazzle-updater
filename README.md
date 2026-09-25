# dazzle workspace

A cross-platform software update framework. dazzle has its own API: the default source
reads dazzle's own [release index](specs/release-index.md), and an optional `AppcastSource`
will read [Sparkle](https://sparkle-project.org) appcasts. This
workspace holds the C++ core library, its language bindings and the design specs together.

| Directory | Contents |
| --- | --- |
| [core/](core) | The C++17 core library and its C ABI — see [core/README.md](core/README.md) |
| [bindings/](bindings) | Language bindings: Rust, Dart, C# |
| [examples/](examples) | Complete apps on a UI framework (Flutter) |
| [specs/](specs) | Design rules for `core/` |

```bash
make test
```
