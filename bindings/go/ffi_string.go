package dazzleupdater

/*
#include <stdlib.h>
#include <dazzle_updater.h>
*/
import "C"

import (
	"strings"
	"unsafe"
)

// Match the other bindings: C strings end at the first NUL.
func toCString(value string) *C.char {
	if end := strings.IndexByte(value, 0); end >= 0 {
		value = value[:end]
	}
	return C.CString(value)
}

func freeCString(value *C.char) {
	C.free(unsafe.Pointer(value))
}

// Returned strings belong to the core and must be freed by its allocator (which
// need not be cgo's C runtime, especially on Windows).
func takeCString(value *C.char) string {
	if value == nil {
		return ""
	}
	defer C.dazzle_free_c_str(value)
	return C.GoString(value)
}
