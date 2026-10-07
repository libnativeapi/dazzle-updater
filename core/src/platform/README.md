# platform

One directory per platform: `macos`, `windows`, `linux`, `android`, `ios`, `ohos`.
The build compiles only the current platform's directory (see `../CMakeLists.txt`).

A platform file implements one *seam*, and the seam decides its name:

| File | Implements | Declared in |
|---|---|---|
| `<seam>_<os>.<ext>` | the platform seam of that name | `../foundation/<seam>.h` |

Every seam has a cross-platform default (`../foundation/<seam>_default.cpp`) that the build
compiles when this directory has no file for it. So a platform only needs files for the
behavior it really has: Android, iOS and OHOS are empty today, and Windows is empty until
its implementation lands ([plan](../../../specs/updater.md)).

## What is here

| Platform | Files |
|---|---|
| macOS | `http_macos.mm` — NSURLSession transfers; `system_macos.mm` — bundle version, architecture, system version, cache directory, and the zip install with its detached helper; `updater_ui_backend_macos.mm`, `updater_ui_controls_macos.*`, `updater_ui_preferences_macos.mm` — the AppKit windows and `NSUserDefaults` |
| Linux | `http_linux.cpp` — one blocking GET through libcurl; `system_linux.cpp` — uname architecture, `/etc/os-release` version, XDG cache directory, and the zip install that replaces the application's own directory with a detached helper; `updater_ui_backend_linux.cpp` and `updater_ui_notes_linux.*` — the GTK 4 windows and Markdown → `GtkTextBuffer`; `updater_ui_preferences_linux.cpp` — `$XDG_CONFIG_HOME/dazzle/<application>.conf` |

`http_linux.cpp` and the two GTK 4 files are compiled only when the build found their
library (libcurl, GTK 4); without it the cross-platform defaults answer for that seam, so
a Linux build with neither still links and runs (see `../CMakeLists.txt`).

## Adding a platform

Provide only what the platform implements. For Windows that means, for example:

- `windows/http_windows.cpp` — WinHTTP, following the contract in `../foundation/http.h`
- `windows/system_windows.cpp` — `ProductVersion` from VERSIONINFO, the system version from
  `RtlGetVersion`, a cache directory, and starting an `exe` or `msi` installer

`SystemSupportsCheck()`, `SystemSupportsDownload()` and `SystemSupportsInstall()` say which
actions the platform reports as available, so a partial implementation is fine: Linux is
meant to find and download updates without installing them, iOS to find them only.
