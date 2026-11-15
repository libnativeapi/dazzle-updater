# WinUI 3 updater UI example

A host application for dazzle updater's update windows on Windows. **The windows are the module's**:
`core/src/platform/windows/updater_ui_backend_winui3.cpp` implements the same `UiBackend`
seam macOS (AppKit) and Linux (GTK 4) implement, driven by the shared state machine, the
shared content builder and core's own wording — Simplified Chinese included. This example is
what an application that uses that module looks like: its own name, its own window, and one
call.

```
App.xaml / App.xaml.cs   the application: starts WinUI and calls UpdaterUI
HostWindow.cs            the host window: the application's name and "Check for Updates…"
Core.cs                  every touch of the generated C# binding, in one place
Startup.cs               the log, and what core answers through the C ABI at startup
WinUI3UpdaterUiExample.csproj
```

The project reference is what builds `core/` with CMake; a plain reference beside it is what
makes the binding a runtime asset of this app (`CopyLocalLockFileAssemblies`).

## Build and run

Needs the .NET SDK (9 or 8), CMake, and — for the first restore — network access to NuGet:
the Windows App SDK and the Windows SDK build tools are
`PackageReference`s. Nothing has to be installed: the app is **unpackaged**
(`WindowsPackageType=None`) and **self-contained** (`WindowsAppSDKSelfContained=true`), so the
Windows App Runtime travels in the output.

```powershell
dotnet build WinUI3UpdaterUiExample.csproj -c Release
.\bin\Release\net9.0-windows10.0.26100.0\win-x64\DazzleUpdaterUiExample.exe
```

`DAZZLE_DEMO_CHECK=1` makes the host check on launch, `DAZZLE_DEMO_CHECK=background` makes it
check silently (which is what shows the permission question the first time),
`DAZZLE_DEMO_THEME=dark` asks for the dark theme, and `DAZZLE_WINDOWS_UI_LOG=1` makes the
Windows backend write what it did to `%TEMP%\dazzle-winui3-backend.log`. The app's own log is
`%TEMP%\dazzle-winui3-example.log`.

### Against a local feed

Without a feed a check ends in the Error window. Three variables point the updater at a signed
release index (`specs/release-index.md`). The [shared test server](../../tools/test_server/README.md)
provides a browser console with 21 scenarios, and can run locally with `--feed` or on another
machine with `--host` and `--base-url`. Copy its printed URL and public key:

```powershell
$env:DAZZLE_DEMO_FEED = "http://127.0.0.1:8765/releases.json"
$env:DAZZLE_DEMO_PUBLIC_KEY = "<the Ed25519 public key, Base64>"
$env:DAZZLE_DEMO_VERSION = "1.0.0"      # the version the index is compared with
```

An `exe` or `msi` asset for `windows`/`x64` then goes through every window: Available,
Downloading, Ready, and — on Install and Relaunch — the installer is started and the host is
asked to close.

### On a remote machine

With the [`remote-hosts`](../../.agents/skills/remote-hosts/SKILL.md) skill, GUI work needs
`--gui`: the plain SSH session is Windows session 0, where nothing can create a window.

```bash
R=.agents/skills/remote-hosts/scripts/remote.py
$R win-box setup
$R win-box run --gui --timeout 1800 build.ps1     # dotnet build → CMake builds core/ → the app
$R win-box run --gui --timeout 300 run.ps1        # starts the .exe and captures the screen
$R win-box pull winui3-<state>.png ./            # the capture, back here
```

Building core on Windows needs the Windows App SDK payload and C++/WinRT's projections:
`core/src/CMakeLists.txt` finds the component packages under the NuGet global packages
folder, generates the projections with the newest Windows SDK's `cppwinrt`, and links
`windowsapp.lib` and the components' native libraries. It leaves the backend file out when
the payload is not there, exactly as Linux does without GTK 4 — and then the cross-platform
backend answers that this build has no windows.

## What the host must do, and why

XAML in a process belongs to the thread that started the `Application`, so the first check
runs on the host's XAML thread. Core discovers that thread and creates its
own message-only Win32 window for dispatch. Check results, download progress and installation
completion reach that thread through its normal message loop. The example needs no timer,
manual redraw calls or COM handles. Later checks can run on any thread.

`HostWindow.cs` builds only the example application's window and calls the C# binding.
The update windows' layout, styling, Markdown rendering and scheduling all live in core.

## Looking at the windows

Screenshots are not kept in the repository: `run.ps1` captures each window to
`winui3-<state>.png` beside this file (ignored by git), and the Available, Checking,
Downloading, Error and Permission windows can be compared with the prototype and with the
macOS and Linux renderings state by state.

## Known limits

- **The host must be DPI aware.** `app.manifest` declares `PerMonitorV2`, as the WinUI 3
  template does; without it Windows renders the process at 100 % and stretches it, and every
  window is blurry.
- **No minimum size for the update window yet.** It resizes when it has notes, but the
  backend does not set the spec's 462 × 150 minimum.
- **`core/tools/updater_ui_preview` has no Windows driver.** The states are reached with a real
  feed instead (above).
- **The answers are not remembered yet.** Windows has no preference store (the registry one in
  the spec is not written), so the permission question is asked on every launch.
