# platform

One directory per platform: `macos`, `windows`, `linux`, `android`, `ios`, `ohos`.
The build compiles only the current platform's directory (see `../CMakeLists.txt`).

A platform file implements one *seam*, and the seam decides its name:

| File | Implements | Declared in |
|---|---|---|
| `<seam>_<os>.<ext>` | the platform seam of that name | `../foundation/<seam>.h` |

Every seam has a cross-platform default (`../foundation/<seam>_default.cpp`) that the build
compiles when this directory has no file for it. So a platform only needs files for the
behavior it really has: Android, iOS and OHOS are empty today, and Windows and Linux are
empty until their implementations land
([plan](../../../specs/updater.md)).

## What is here

| Platform | Files |
|---|---|
| macOS | `http_macos.mm` — NSURLSession transfers; `system_macos.mm` — bundle version, architecture, system version, cache directory, and the zip install with its detached helper |

## Adding a platform

Provide only what the platform implements. For Windows that means, for example:

- `windows/http_windows.cpp` — WinHTTP, following the contract in `../foundation/http.h`
- `windows/system_windows.cpp` — `ProductVersion` from VERSIONINFO, the system version from
  `RtlGetVersion`, a cache directory, and starting an `exe` or `msi` installer

`SystemSupportsCheck()`, `SystemSupportsDownload()` and `SystemSupportsInstall()` say which
actions the platform reports as available, so a partial implementation is fine: Linux is
meant to find and download updates without installing them, iOS to find them only.
