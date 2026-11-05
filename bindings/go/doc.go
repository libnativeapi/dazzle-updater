// Package dazzleupdater provides Go bindings for the dazzle updater C++ core.
//
// Configure ReleaseIndexSourceInstance and UpdaterInstance before checking for
// updates. CheckForUpdates, DownloadUpdate and InstallUpdate block; applications
// with a UI should call them from a worker goroutine. Failed actions return false;
// ErrorCode and ErrorMessage describe the failure. Empty native strings become "".
//
// Build the native library with go generate before building or testing this package.
// A full repository checkout, cgo, CMake and a C++17 toolchain are required.
package dazzleupdater

//go:generate cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
//go:generate cmake --build build --target dazzle_updater --config Release
