//! A Sparkle-compatible software update framework.
//!
//! ```no_run
//! let updater = dazzle::Updater::instance();
//! println!("{:?}", updater.feed_url());
//! ```

mod ffi;
mod ffi_string;
mod updater;

pub use updater::Updater;
