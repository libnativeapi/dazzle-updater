cmake_minimum_required(VERSION 3.10)

project(dazzle_library VERSION 0.0.1 LANGUAGES CXX C)

include(CTest)

# Add library subdirectory
add_subdirectory(src)

# Add example programs subdirectory
add_subdirectory(examples/updater_example)
add_subdirectory(examples/updater_c_example)

# The built-in windows are AppKit's, so their example is a macOS application bundle, and
# only exists when the module is built at all.
if(APPLE AND DAZZLE_WITH_UPDATER_UI)
    add_subdirectory(examples/updater_ui_example)
endif()

# Unit tests for the cross-platform logic (registered with CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
