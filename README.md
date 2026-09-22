# dazzle workspace

A Sparkle-compatible software update framework for all major platforms. This workspace
holds the C++ core library, its language bindings and the design specs together.

| Directory | Contents |
| --- | --- |
| [core/](core) | The C++17 core library and its C ABI — see [core/README.md](core/README.md) |
| [bindings/](bindings) | Language bindings: Rust, Dart, C# |
| [specs/](specs) | Design rules for `core/` |

```bash
make test
```
