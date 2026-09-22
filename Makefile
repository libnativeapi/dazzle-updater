.PHONY: build examples rust dart csharp test clean

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

# Everything: core examples and every binding
test: examples rust dart csharp

clean:
	rm -rf $(BUILD_DIR) bindings/rust/target bindings/dart/.dart_tool
	find bindings/csharp -type d \( -name bin -o -name obj \) -prune -exec rm -rf {} +
