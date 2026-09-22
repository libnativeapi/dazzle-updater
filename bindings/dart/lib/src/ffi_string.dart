import 'dart:convert';
import 'dart:ffi';

import 'package:ffi/ffi.dart';

import 'dazzle_bindings.dart';

/// Calls [body] with [value] as a NUL-terminated UTF-8 string that lives only
/// for the duration of the call. C strings end at the first NUL, so anything
/// after an interior NUL is dropped.
T withCString<T>(String value, T Function(Pointer<Char> ptr) body) {
  final bytes = utf8.encode(value);
  final end = bytes.indexOf(0);
  final length = end == -1 ? bytes.length : end;
  final ptr = malloc<Uint8>(length + 1);
  try {
    final buffer = ptr.asTypedList(length + 1);
    buffer.setRange(0, length, bytes);
    buffer[length] = 0;
    return body(ptr.cast());
  } finally {
    malloc.free(ptr);
  }
}

/// Takes ownership of a string returned by the C ABI and frees it.
/// NULL (the C ABI's "no value") becomes `null`.
String? takeCString(Pointer<Char> ptr) {
  if (ptr == nullptr) {
    return null;
  }
  final value = ptr.cast<Utf8>().toDartString();
  dazzle_free_c_str(ptr);
  return value;
}
