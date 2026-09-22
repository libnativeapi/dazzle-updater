use std::ffi::{CStr, CString};
use std::os::raw::c_char;

/// Converts a Rust string for the C ABI. C strings end at the first NUL, so
/// anything after an interior NUL is dropped.
pub(crate) fn to_c_string(value: &str) -> CString {
    let end = value.find('\0').unwrap_or(value.len());
    CString::new(&value[..end]).expect("interior NUL was stripped")
}

/// Takes ownership of a string returned by the C ABI and frees it.
/// NULL (the C ABI's "no value") becomes `None`.
pub(crate) unsafe fn take_c_string(ptr: *mut c_char) -> Option<String> {
    if ptr.is_null() {
        return None;
    }
    let value = CStr::from_ptr(ptr).to_string_lossy().into_owned();
    crate::ffi::dazzle_free_c_str(ptr);
    Some(value)
}
