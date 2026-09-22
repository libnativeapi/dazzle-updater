//! Declarations of the dazzle C ABI (`core/src/capi/`).
//!
//! Hand-written until the code generator is in place; keep every declaration in
//! step with the C headers.

use std::os::raw::c_char;

// string_utils_c.h
extern "C" {
    /// Free a string returned by the dazzle C API. Accepts NULL.
    pub fn dazzle_free_c_str(str_: *mut c_char);
}

// updater_c.h
extern "C" {
    /// Pass NULL or "" to go back to the URL the app's configuration provides.
    pub fn dazzle_updater_set_feed_url(feed_url: *const c_char);

    /// Returns NULL when no feed URL is configured. Free the result with
    /// `dazzle_free_c_str`.
    pub fn dazzle_updater_get_feed_url() -> *mut c_char;
}
