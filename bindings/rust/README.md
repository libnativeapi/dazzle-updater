# dazzle for Rust

Rust bindings for [dazzle](../../README.md), a cross-platform software
update framework with its own API. The default source reads dazzle's own
[release index](../../specs/release-index.md); an optional `AppcastSource` will read
[Sparkle](https://sparkle-project.org) appcasts.

**Status**: macOS works end to end (check, download, Ed25519 verification, install). Other
platforms answer `"not supported on this platform"` until their implementations land
([plan](../../specs/updater.md)).

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
use dazzle::{ReleaseIndexSource, Updater};

fn main() {
    ReleaseIndexSource::instance().set_url("https://example.com/myapp/releases.json");

    let updater = Updater::instance();
    updater.set_public_key("pfIShU4dEXqPd5ObYNfDBiQWcXozk7estwzTnF9BamQ=");

    // CheckForUpdates, DownloadUpdate and InstallUpdate block, so run them on a worker
    // thread and read the result on the UI thread.
    if updater.check_for_updates() && updater.is_update_available() {
        println!("{:?}", updater.update_display_version());
    }
}
```

`check_for_updates()` returns `false` for a failed check; `error_code()` then says why, and
`error_message()` carries the English text. An empty string comes back as `None`.

## Examples

```bash
cargo run --example updater_example
```

| Example | Shows |
| --- | --- |
| [updater_example](examples/updater_example.rs) | The configured source, version and key |

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
