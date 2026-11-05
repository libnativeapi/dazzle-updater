cmake_minimum_required(VERSION 3.10)
project(dazzle_updater_go LANGUAGES C CXX)

# Keep platform dependencies inside the shared library, including optional GTK 4
# and WinUI 3. cgo only links the C ABI.
set(DAZZLE_BUILD_SHARED ON CACHE BOOL "Build the Go binding's native library" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build core tests" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../core" core)

# One location for every generator, including Visual Studio and Xcode. The empty
# generator expression prevents multi-config generators adding a Release/ suffix.
set_target_properties(dazzle_updater PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<0:>"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<0:>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<0:>"
    MACOSX_RPATH ON
    BUILD_WITH_INSTALL_NAME_DIR ON
    INSTALL_NAME_DIR "@rpath"
)
