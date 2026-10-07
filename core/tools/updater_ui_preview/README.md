# UpdaterUI preview

A development tool, not part of the library and not built by CMake: it drives the real
`UpdaterUI` state machine with a scripted `Updater` (no network, no package, no installer)
and writes one PNG per state, so the native windows can be compared with `prototype/`
state by state — and, when the Windows backend lands, its windows can be compared with
the macOS ones.

```bash
cd core
clang++ -std=c++17 -fobjc-arc -I src -I third_party \
  tools/updater_ui_preview/updater_ui_preview.mm build/src/libdazzle.a \
  -framework AppKit -framework Foundation -o build/preview/updater_ui_preview
./build/preview/updater_ui_preview
```

The pictures land in `build/preview/` (`01-checking.png` … `12-downloaded.png`); that
directory is a build artifact and is not committed. Three environment variables select
what is rendered, and the file prefix:

```bash
DAZZLE_PREVIEW_DARK=1 ./build/preview/updater_ui_preview               # dark appearance
DAZZLE_PREVIEW_LANG=zh ./build/preview/updater_ui_preview              # Simplified Chinese
DAZZLE_PREVIEW_PREFIX=dark- ./build/preview/updater_ui_preview         # name the files
```

Render the library before the tool: the tool links `build/src/libdazzle.a`, so a change
in `src/` needs `cmake --build build` before the line above.

Notes on what the pictures show:

- Only a window's content view is captured, so the title bar is missing. The window
  titles are printed next to each file name.
- The buttons, check boxes and progress bars are dazzle's own, drawn from the same
  tokens on both sides of the comparison, so they are exact. The window around them
  (title bar, traffic lights, shadow, corners) is the real NSWindow, which a command-line
  tool still gets; it is the one part the prototype does not model.
- The states are reached by the real flow: the check and the download are held on the
  scripted port so the short-lived states (Checking, Downloading, Installing) can be
  looked at.
