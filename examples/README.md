# examples

Complete apps built with a UI framework on top of one of the bindings. Each directory is
named `<framework>_<module>_example`.

| Example | Framework | Binding | Platforms |
| --- | --- | --- | --- |
| [flutter_updater_example](flutter_updater_example) | Flutter | [Dart](../bindings/dart) | macOS |
| [winui3_updater_ui_example](winui3_updater_ui_example) | WinUI 3 (Windows App SDK) | [C#](../bindings/csharp) | Windows |
| [mygo_updater_example](mygo_updater_example) | MyGo native UI | [Go](../bindings/go) | macOS verified; Linux/Windows need native dependencies |

The minimal per-module programs live next to the code they exercise:
`core/examples/` (C++ and C) and each binding's own `example`/`examples` directory.
`core/examples/updater_ui_example` is the one that shows the built-in windows: `make demo`
starts a local signed feed and runs it on macOS, `make demo-linux` the same on Linux (see
[its README](../core/examples/updater_ui_example/README.md)).

These apps need their framework's SDK, so `make test` does not build them; `make flutter`
builds the Flutter one. The WinUI 3 example is built with `dotnet build` on Windows —
[its README](winui3_updater_ui_example/README.md) shows how to do that on a remote host.
`make mygo` builds the MyGo example; `make mygo-smoke` exercises its real window with
a local signed feed and exits without installing an update.

For repeatable manual cases, run `make test-server` and open the printed console URL.
The [shared test server](../tools/test_server/README.md) provides 21 scenarios including
no update, HTTP errors, slow downloads and cancellation, interrupted transfers, invalid
signatures and redirects. Copy its release URL and public key into any example.
`make demo` and `make demo-linux` use this server with an installable application;
set e.g. `SCENARIO=slow-download` to select the initial case, then switch cases in the console.
