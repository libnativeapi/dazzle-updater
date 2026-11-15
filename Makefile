.PHONY: build examples demo demo-linux test-server test-server-test core-tests rust dart csharp go swift swiftui swiftui-demo swiftui-smoke flutter mygo mygo-smoke codegen check test clean

BUILD_DIR := core/build
SCENARIO ?= update
PORT ?= 8765

# Configure and build the core library and its examples
build:
	cmake -S core -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

# Run the core unit tests (version comparison, release index parsing, Ed25519, Updater)
core-tests: build
	cd $(BUILD_DIR) && ctest --output-on-failure

# Build, then run every example
examples: build
	$(BUILD_DIR)/examples/updater_example/updater_example
	$(BUILD_DIR)/examples/updater_c_example/updater_c_example

# Walk through the built-in update windows: starts a local signed feed and runs the
# UpdaterUI example application (macOS; not part of `test`, it needs a desktop session)
demo: build
	cd core && DAZZLE_DEMO_SCENARIO=$(SCENARIO) ./examples/updater_ui_example/demo.sh

# The same walk on Linux, around an application directory rather than a bundle (GTK 4,
# also needs a desktop session)
demo-linux: build
	cd core && DAZZLE_DEMO_SCENARIO=$(SCENARIO) ./examples/updater_ui_example/demo_linux.sh

# A shared scenario server for every example (Python 3.9+, no pip dependencies).
test-server:
	cmake -S core -B $(BUILD_DIR) -DBUILD_TESTING=ON
	cmake --build $(BUILD_DIR) --target fixture_signer
	python3 tools/test_server/server.py --port $(PORT) --scenario $(SCENARIO)

# Exercise the HTTP server and the real core check/download/signature/error paths.
test-server-test:
	cmake -S core -B $(BUILD_DIR) -DBUILD_TESTING=ON
	cmake --build $(BUILD_DIR) --target fixture_signer updater_example
	python3 tools/test_server/test_server.py

# Test the Rust binding and run its example
rust:
	cd bindings/rust && cargo test && cargo run --example updater_example

# Test the Dart binding and run its example
dart:
	cd bindings/dart && dart pub get && dart test && dart run example/updater_example.dart

# Test the C# binding and run its example
csharp:
	cd bindings/csharp && dotnet test DazzleUpdater.slnx && dotnet run --project examples/UpdaterExample

# Build the native library, test the Go binding and run its example
go:
	cd bindings/go && go generate ./... && go test -count=1 ./... && go run ./examples/updater_example

# Build the native library, test the Swift binding and run its example
swift:
	cd bindings/swift && ./build-native.sh && swift test && swift run updater-example

# Build the SwiftUI macOS app bundle and run its model tests (needs the macOS SDK)
swiftui:
	python3 examples/swiftui_updater_example/build.py
	cd examples/swiftui_updater_example && swift test

# Serve an installable 2.0.0 app and open a disposable 1.0.0 copy
swiftui-demo: swiftui
	cmake -S core -B $(BUILD_DIR) -DBUILD_TESTING=ON
	cmake --build $(BUILD_DIR) --target fixture_signer
	python3 examples/swiftui_updater_example/demo.py --scenario $(SCENARIO)

# Open native windows; click through installation to verify replacement and relaunch
swiftui-smoke: swiftui
	cmake -S core -B $(BUILD_DIR) -DBUILD_TESTING=ON
	cmake --build $(BUILD_DIR) --target fixture_signer
	python3 examples/swiftui_updater_example/demo.py --smoke

# Build the Flutter example app (needs the Flutter SDK; not part of `test`)
flutter:
	cd examples/flutter_updater_example && flutter pub get && flutter analyze && flutter build macos --debug

# Build the MyGo desktop example (needs MyGo's Go toolchain; no desktop session)
mygo:
	cd bindings/go && go generate ./...
	cd examples/mygo_updater_example && go build -o build/mygo_updater_example .

# Exercise the live MyGo window against a signed local feed (needs a desktop session)
mygo-smoke: mygo
	cd examples/mygo_updater_example && go run . --smoke

# Regenerate the C ABI and the bindings from the C++ headers
codegen:
	./codegen

# Fail if a generated file is out of date
check:
	./codegen check

# Everything: generated files, core tests, core examples and every binding
test: check core-tests test-server-test examples rust dart csharp go swift

clean:
	rm -rf $(BUILD_DIR) bindings/rust/target bindings/dart/.dart_tool bindings/go/build
	rm -rf bindings/swift/build bindings/swift/.build
	rm -rf examples/swiftui_updater_example/build examples/swiftui_updater_example/.build
	rm -rf examples/flutter_updater_example/build examples/flutter_updater_example/.dart_tool
	rm -rf examples/mygo_updater_example/build
	find bindings/csharp -type d \( -name bin -o -name obj \) -prune -exec rm -rf {} +
