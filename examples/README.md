# examples

Complete apps built with a UI framework on top of one of the bindings. Each directory is
named `<framework>_<module>_example`.

| Example | Framework | Binding | Platforms |
| --- | --- | --- | --- |
| [flutter_updater_example](flutter_updater_example) | Flutter | [Dart](../bindings/dart) | macOS |
| [winui3_updater_ui_example](winui3_updater_ui_example) | WinUI 3 (Windows App SDK) | [C#](../bindings/csharp) | Windows |

The minimal per-module programs live next to the code they exercise:
`core/examples/` (C++ and C) and each binding's own `example`/`examples` directory.
`core/examples/updater_ui_example` is the one that shows the built-in windows: `make demo`
starts a local signed feed and runs it on macOS, `make demo-linux` the same on Linux (see
[its README](../core/examples/updater_ui_example/README.md)).

These apps need their framework's SDK, so `make test` does not build them; `make flutter`
builds the Flutter one. The WinUI 3 example is built with `dotnet build` on Windows —
[its README](winui3_updater_ui_example/README.md) shows how to do that on a remote host.
