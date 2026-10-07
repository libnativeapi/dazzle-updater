# UpdaterUI preview

A development tool, not part of the library and not built by CMake: it drives the real
`UpdaterUI` state machine with a scripted `Updater` (no network, no package, no installer)
and writes one PNG per state, so the native windows can be compared with `prototype/`
state by state — and with each other, macOS against Linux.

There is one source file per platform: `updater_ui_preview.mm` (AppKit) and
`updater_ui_preview_linux.cpp` (GTK 4). Both drive the same states; the Linux one presses
the buttons by their drawn labels, so it can render the Simplified Chinese wording as
well as the English one.

```bash
cd core
clang++ -std=c++17 -fobjc-arc -I src -I third_party \
  tools/updater_ui_preview/updater_ui_preview.mm build/src/libdazzle_updater.a \
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

Render the library before the tool: the tool links `build/src/libdazzle_updater.a`, so a change
in `src/` needs `cmake --build build` before the line above.

### On Linux (GTK 4)

GTK needs a display, and the tool renders each window through `GtkWidgetPaintable` rather
than reading the screen, so a virtual one is enough — no window manager, no screenshot
permission, no desktop session:

```bash
cd core
g++ -std=c++17 -I src -I third_party \
  $(pkg-config --cflags gtk4) $(pkg-config --cflags libcurl) \
  tools/updater_ui_preview/updater_ui_preview_linux.cpp build/src/libdazzle_updater.a \
  $(pkg-config --libs gtk4) $(pkg-config --libs libcurl) -o build/preview-linux/preview

Xvfb :99 -screen 0 1280x900x24 &          # or any real display
DISPLAY=:99 GDK_BACKEND=x11 GSK_RENDERER=cairo DAZZLE_PREVIEW_OUT=build/preview-linux \
  ./build/preview-linux/preview
```

The environment variables are the macOS ones: `DAZZLE_PREVIEW_OUT` for the directory, `DAZZLE_PREVIEW_DARK=1`, `DAZZLE_PREVIEW_LANG=zh`,
`DAZZLE_PREVIEW_PREFIX=dark-`. `DAZZLE_OMARCHY_COLORS=<path to a colors.toml>` renders the
windows the way they come out on Omarchy with that theme (the backend reads the palette
from the Omarchy state directory on a real Omarchy desktop; the variable points it at any
palette, on any machine). The tool also builds against Homebrew's GTK 4 on macOS, which is
a quick way to look at a palette without a Linux host. Each captured window reports its own size, and the window
is the real `GtkWindow` — client-side decorations included — while the picture itself is
GTK's own rendering of it.

Notes on what the pictures show:

- The whole window is captured — its background and title bar included, which is where
  the traffic lights and the window's corners come from. The window titles are printed
  next to each file name.
- The buttons, check boxes and progress bars are AppKit's own, laid out as Sparkle 2.x lays
  out its windows (specs/updater-ui.md, "外观"), so a picture is what the system draws on
  this macOS release — compare it with Sparkle's windows, not with the prototype's tokens.
  The window around them (title bar, traffic lights, shadow, corners) is the real NSWindow,
  which a command-line tool still gets.
- The states are reached by the real flow: the check and the download are held on the
  scripted port so the short-lived states (Checking, Downloading, Installing) can be
  looked at.
