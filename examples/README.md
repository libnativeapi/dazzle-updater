# examples

Complete apps built with a UI framework on top of one of the bindings. Each directory is
named `<framework>_<module>_example`.

| Example | Framework | Binding | Platforms |
| --- | --- | --- | --- |
| [flutter_updater_example](flutter_updater_example) | Flutter | [Dart](../bindings/dart) | macOS |

The minimal per-module programs live next to the code they exercise:
`core/examples/` (C++ and C) and each binding's own `example`/`examples` directory.

These apps need their framework's SDK, so `make test` does not build them; `make flutter`
builds the Flutter one.
