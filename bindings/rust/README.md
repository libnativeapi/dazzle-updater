# dazzle for Rust

Rust bindings for [dazzle](../../README.md), a cross-platform software
update framework. dazzle has its own API; its feed format is the
[Sparkle](https://sparkle-project.org) appcast, so existing appcasts work unchanged.

🚧 **Work in Progress**: only the feed URL setting is implemented so far.

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

    assert_eq!(updater.feed_url(), None);

    updater.set_feed_url("https://example.com/appcast.xml");
    assert_eq!(
        updater.feed_url().as_deref(),
        Some("https://example.com/appcast.xml")
    );

    // "" clears it.
    updater.set_feed_url("");
}
```

`feed_url()` returns `None` when no URL has been set.


## Examples

```bash
cargo run --example updater_example
```

| Example | Shows |
| --- | --- |
| [updater_example](examples/updater_example.rs) | Setting and reading the appcast feed URL |

## Development

```bash
cargo test
```

[src/ffi.rs](src/ffi.rs), [src/modules.rs](src/modules.rs) and one `src/<module>.rs` per
class are generated from the C++ headers; do not edit them. Change the header in
[core/src/](../../core/src) and run `./codegen` from the repository root. The string
helpers ([src/ffi_string.rs](src/ffi_string.rs)) and [src/lib.rs](src/lib.rs) are
hand-written.

## License

MIT
