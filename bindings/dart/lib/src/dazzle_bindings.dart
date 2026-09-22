// Raw bindings to the dazzle C ABI (core/src/capi/).
//
// Hand-written until the code generator is in place; keep every declaration in
// step with the C headers. The library is the code asset built by
// hook/build.dart, resolved through this file's default asset id.
//
// Declarations keep the C symbol names.
// ignore_for_file: non_constant_identifier_names
@DefaultAsset('package:dazzle/src/dazzle_bindings.dart')
library;

import 'dart:ffi';

// string_utils_c.h

@Native<Void Function(Pointer<Char>)>()
external void dazzle_free_c_str(Pointer<Char> str);

// updater_c.h

@Native<Void Function(Pointer<Char>)>()
external void dazzle_updater_set_feed_url(Pointer<Char> feed_url);

@Native<Pointer<Char> Function()>()
external Pointer<Char> dazzle_updater_get_feed_url();
