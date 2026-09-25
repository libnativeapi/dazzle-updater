# dazzle for C#

.NET bindings for [dazzle](../../README.md), a cross-platform software
update framework with its own API. The default source reads dazzle's own
[release index](../../specs/release-index.md); an optional `AppcastSource` will read
[Sparkle](https://sparkle-project.org) appcasts.

**Status**: macOS works end to end (check, download, Ed25519 verification, install). Other
platforms answer `"not supported on this platform"` until their implementations land
([plan](../../specs/updater.md)).

## Requirements

- .NET 8 or later
- CMake 3.10+
- A C++17 compiler (Xcode Command Line Tools on macOS, MSVC on Windows, GCC or Clang on Linux)

Building `Dazzle.csproj` compiles the dazzle C++ core from source with CMake and copies
the shared library next to your app, where `[LibraryImport("dazzle")]` finds it. There is
no library to copy by hand.

Only the host OS is supported for now: the native library is built for the machine you
build on.

## Installation

The package is not on NuGet yet. Clone the repository and reference the project:

```xml
<ItemGroup>
  <ProjectReference Include="path/to/dazzle/bindings/csharp/Dazzle.csproj" />
</ItemGroup>
```

## Usage

```csharp
using Dazzle;

ReleaseIndexSource.Instance.Url = "https://example.com/myapp/releases.json";

var updater = Updater.Instance;
updater.PublicKey = "pfIShU4dEXqPd5ObYNfDBiQWcXozk7estwzTnF9BamQ=";

// CheckForUpdates, DownloadUpdate and InstallUpdate block: run them with Task.Run so the
// UI thread stays free, and read the result on the UI thread.
if (updater.CheckForUpdates() && updater.IsUpdateAvailable)
{
    Console.WriteLine(updater.UpdateDisplayVersion);
}
```

`CheckForUpdates()` returns `false` for a failed check; `ErrorCode` then says why, and
`ErrorMessage` carries the English text. An empty string comes back as `null`.

## Examples

```bash
dotnet run --project examples/UpdaterExample
```

| Example | Shows |
| --- | --- |
| [UpdaterExample](examples/UpdaterExample/Program.cs) | The configured source, version and key |

## Development

```bash
dotnet build Dazzle.slnx
dotnet test Dazzle.slnx
```

The library targets `net8.0`; the example and tests target `net10.0`.

[src/NativeMethods.cs](src/NativeMethods.cs) and one `src/<Class>.cs` per class are
generated from the C++ headers; do not edit them. Change the header in
[core/src/](../../core/src) and run `./codegen` from the repository root. The string
helpers ([src/CString.cs](src/CString.cs)) are hand-written.

## License

MIT
