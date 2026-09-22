# dazzle for Rust

Rust bindings for [dazzle](../../README.md), a Sparkle-compatible software update
framework. dazzle reads the [Sparkle](https://sparkle-project.org) appcast format and
honors the configuration keys Sparkle (macOS) and WinSparkle (Windows) established, so
existing feeds and apps work unchanged.

🚧 **Work in Progress**: only feed URL configuration is implemented so far.

## Requirements

The crate compiles the dazzle C++ core from source during `cargo build`, so it needs:

- CMake 3.10+
- A C++17 compiler (Xcode Command Line Tools on macOS, MSVC on Windows, GCC or Clang on Linux)

## Installation

The crate is not on crates.io yet. Depend on it from git; Cargo finds the `dazzle`
package inside the repository:

```toml
[dependencies]
dazzle = { git = "https://github.com/fastforgedev/dazzle" }
```

## Usage

```rust
use dazzle::Updater;

fn main() {
    let updater = Updater::instance();

    // Falls back to the app's configuration when no URL was set.
    println!("{:?}", updater.feed_url());

    updater.set_feed_url("https://example.com/appcast.xml");
    assert_eq!(
        updater.feed_url().as_deref(),
        Some("https://example.com/appcast.xml")
    );

    // "" goes back to the app's configuration.
    updater.set_feed_url("");
}
```

`feed_url()` returns `None` when nothing is configured.

## Configuration

Without `set_feed_url`, the feed URL comes from the app's packaging, using the keys
Sparkle and WinSparkle already defined:

| Platform | Source |
| --- | --- |
| macOS | `SUFeedURL` in the app bundle's `Info.plist` |
| Windows | `FeedURL` resource of type `APPCAST` in the executable |
| Linux, Android, iOS, OHOS | none; only `set_feed_url` applies |

## Examples

```bash
cargo run --example updater_example
```

| Example | Shows |
| --- | --- |
| [updater_example](examples/updater_example.rs) | Reading and overriding the appcast feed URL |

## Development

```bash
cargo test
```

The C ABI declarations in [src/ffi.rs](src/ffi.rs) are hand-written: when a header in
[core/src/capi/](../../core/src/capi) changes, update them in the same commit.

## License

MIT
