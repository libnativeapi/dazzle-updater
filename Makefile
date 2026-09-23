.PHONY: build examples rust dart csharp flutter codegen check test clean

BUILD_DIR := core/build

# Configure and build the core library and its examples
build:
	cmake -S core -B $(BUILD_DIR)
	cmake --build $(BUILD_DIR)

# Build, then run every example
examples: build
	$(BUILD_DIR)/examples/updater_example/updater_example
	$(BUILD_DIR)/examples/updater_c_example/updater_c_example

# Test the Rust binding and run its example
rust:
	cd bindings/rust && cargo test && cargo run --example updater_example

# Test the Dart binding and run its example
dart:
	cd bindings/dart && dart pub get && dart test && dart run example/updater_example.dart

# Test the C# binding and run its example
csharp:
	cd bindings/csharp && dotnet test Dazzle.slnx && dotnet run --project examples/UpdaterExample

# Build the Flutter example app (needs the Flutter SDK; not part of `test`)
flutter:
	cd examples/flutter_updater_example && flutter pub get && flutter analyze && flutter build macos --debug

# Regenerate the C ABI and the bindings from the C++ headers
codegen:
	./codegen

# Fail if a generated file is out of date
check:
	./codegen check

# Everything: generated files, core examples and every binding
test: check examples rust dart csharp

clean:
	rm -rf $(BUILD_DIR) bindings/rust/target bindings/dart/.dart_tool
	rm -rf examples/flutter_updater_example/build examples/flutter_updater_example/.dart_tool
	find bindings/csharp -type d \( -name bin -o -name obj \) -prune -exec rm -rf {} +
