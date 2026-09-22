# dazzle for C#

.NET bindings for [dazzle](../../README.md), a Sparkle-compatible software update
framework. dazzle reads the [Sparkle](https://sparkle-project.org) appcast format and
honors the configuration keys Sparkle (macOS) and WinSparkle (Windows) established, so
existing feeds and apps work unchanged.

🚧 **Work in Progress**: only feed URL configuration is implemented so far.

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

// Falls back to the app's configuration when no URL was set.
Console.WriteLine(updater.FeedUrl);

updater.FeedUrl = "https://example.com/appcast.xml";
Console.WriteLine(updater.FeedUrl); // https://example.com/appcast.xml

// null (or "") goes back to the app's configuration.
updater.FeedUrl = null;
```

`FeedUrl` is `null` when nothing is configured.

## Configuration

Without setting `FeedUrl`, the feed URL comes from the app's packaging, using the keys
Sparkle and WinSparkle already defined:

| Platform | Source |
| --- | --- |
| macOS | `SUFeedURL` in the app bundle's `Info.plist` |
| Windows | `FeedURL` resource of type `APPCAST` in the executable |
| Linux, Android, iOS, OHOS | none; only setting `FeedUrl` applies |

## Examples

```bash
dotnet run --project examples/UpdaterExample
```

| Example | Shows |
| --- | --- |
| [UpdaterExample](examples/UpdaterExample/Program.cs) | Reading and overriding the appcast feed URL |

## Development

```bash
dotnet build Dazzle.slnx
dotnet test Dazzle.slnx
```

The library targets `net8.0`; the example and tests target `net10.0`.

The C ABI declarations in [src/NativeMethods.cs](src/NativeMethods.cs) are
hand-written: when a header in [core/src/capi/](../../core/src/capi) changes, update
them in the same commit.

## License

MIT
