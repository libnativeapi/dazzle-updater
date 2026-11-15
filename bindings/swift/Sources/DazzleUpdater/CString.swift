import CDazzleUpdater

/// Copy an owned UTF-8 C string before releasing it with the library's allocator.
func takeCString(_ pointer: UnsafeMutablePointer<CChar>?) -> String? {
    guard let pointer else { return nil }
    defer { dazzle_free_c_str(pointer) }
    return String(cString: pointer)
}
