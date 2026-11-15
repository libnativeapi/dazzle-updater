# UpdaterUI example

A host application for the built-in update windows: a small window of its own, a "Check
for Updates…" action, and `UpdaterUI`'s three windows on top of it. It is the example for
[specs/updater-ui.md](../../../specs/updater-ui.md) on both platforms that have the
built-in windows — an AppKit application bundle on macOS (`main.mm`), a GTK 4 application
directory on Linux (`main_linux.cpp`).

| File | Platform |
|---|---|
| `main.mm` | macOS: an `NSApplication`, its menu, and its own window |
| `main_linux.cpp` | Linux: a `GtkApplication`, and its own window |
| `demo.sh` | macOS: a local signed feed, run end to end |
| `demo_linux.sh` | Linux: the same, around an application directory |
| `DazzleDemo.icns` | the macOS icon: the brand mark on its dark tile, on Apple's icon grid |
| `icons/` | the Linux icon on the GNOME grid, in hicolor layout (PNGs, scalable, symbolic), copied beside the binary by the demo |

## Run it

```bash
# anything macOS: `make demo`, or
cd core && ./examples/updater_ui_example/demo.sh

# on Linux (needs a desktop session; `make demo-linux`)
cd core && ./examples/updater_ui_example/demo_linux.sh
```

Both scripts build a throwaway copy of the application, write the feed URL and the public
key into it — an `Info.plist` on macOS, a `dazzle-demo.conf` beside the binary on Linux —
serve a signed 2.0.0 package of the same application at a higher version, and run it.
Clicking through **Install Update → Install and Relaunch** replaces the copy in place and
starts it again, and the copy that comes back reports 2.0.0. **Reset Demo** (a button, and
a menu item on macOS) calls `UpdaterUI::ResetPreferences()` and sets the version back to
1.0.0, so the next check finds 2.0.0 again — after an install, after Skip This Version, or
after answering the automatic-checks question.

Both demos use the [shared test server](../../../tools/test_server/README.md) and print
its browser console address. Switch between 21 cases there, then check again in the app:
no update, index errors, slow or interrupted downloads, invalid signatures, redirects,
and release-note layouts. To start in a particular case, run e.g.
`make demo SCENARIO=bad-signature` or `make demo-linux SCENARIO=slow-download`.

The icons (and the WinUI example's `DazzleDemo.ico`) are generated from the brand mark by
`branding/dazzle/make-icons.py`, one per platform's icon grid; rerun it after changing the mark.

On Linux the application is a directory (the executable plus whatever sits beside it), so
the demo copies the binary into its own directory and the release is a zip holding that
directory at a higher version.

Pictures of the windows are not kept in the repository. To look at them, render every
state with [`core/tools/updater_ui_preview`](../../tools/updater_ui_preview/README.md),
which needs no desktop session (its README has the Xvfb line), or run the demo on a real
session: there the accent colour, the header buttons and the fonts are the system's, which
is what "GTK 4's own controls, following the theme" looks like. The download, install and
restart behind the windows are covered by `core/tests/linux_install_e2e.sh`.

## What the Linux windows are

GTK 4's own controls — `GtkWindow` and `GtkHeaderBar`, `GtkLabel`, `GtkButton`,
`GtkCheckButton`, `GtkProgressBar`, and a read-only `GtkTextView` for the notes — so the
look, the dark theme, the accent colour, the fonts and accessibility come from the theme
rather than from dazzle updater's own drawing. `specs/updater-ui.md` has the metrics and the two
Linux-only differences: the windows are drawn on the application's own GTK main loop, and
the compositor places them (Wayland does not let a client set a window's position).

On [Omarchy](https://omarchy.org) the theme never reaches GTK beyond "light or dark", so
the windows read the theme's own palette (`~/.local/state/omarchy/current/theme/colors.toml`)
and draw themselves with Omarchy's own control kit — the monospace face, square
bordered buttons, a toggle instead of a check box, no title bar — scoped to their own
windows, the way Omarchy's bar and launcher look. GNOME is left exactly as
before. `specs/updater-ui.md` has the mapping; to look at a palette anywhere, point
`DAZZLE_OMARCHY_COLORS` at a `colors.toml` when running the demo or the preview tool.
