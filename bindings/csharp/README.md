# dazzle for C#

.NET bindings for [dazzle](../../README.md), a cross-platform software
update framework. dazzle has its own API; its feed format is the
[Sparkle](https://sparkle-project.org) appcast, so existing appcasts work unchanged.

🚧 **Work in Progress**: only the feed URL setting is implemented so far.

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

var updater = Updater.Instance;

Console.WriteLine(updater.FeedUrl ?? "(none)"); // (none)

updater.FeedUrl = "https://example.com/appcast.xml";
Console.WriteLine(updater.FeedUrl); // https://example.com/appcast.xml

// null (or "") clears it.
updater.FeedUrl = null;
```

`FeedUrl` is `null` when no URL has been set.


## Examples

```bash
dotnet run --project examples/UpdaterExample
```

| Example | Shows |
| --- | --- |
| [UpdaterExample](examples/UpdaterExample/Program.cs) | Setting and reading the appcast feed URL |

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
