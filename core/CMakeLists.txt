cmake_minimum_required(VERSION 3.10)

project(dazzle_updater VERSION 0.0.1 LANGUAGES CXX C)

# Every source in this tree is UTF-8, and the localized strings in updater_ui_strings.cpp
# and the tests are not ASCII. MSVC reads a file in the system codepage unless told
# otherwise, which turns them into "newline in constant" errors on a Chinese Windows. This
# belongs to the whole project: the tests and the examples hold those strings too.
if(MSVC)
    add_compile_options("/utf-8")
endif()

include(CTest)

# Add library subdirectory
add_subdirectory(src)

# Add example programs subdirectory
add_subdirectory(examples/updater_example)
add_subdirectory(examples/updater_c_example)

# The built-in windows are a platform's own: an AppKit application bundle on macOS, a
# GTK 4 desktop application on Linux. Each only exists when the module is built at all,
# and Linux needs GTK 4's development files for the example to build.
if(APPLE AND DAZZLE_WITH_UPDATER_UI)
    add_subdirectory(examples/updater_ui_example)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux" AND DAZZLE_WITH_UPDATER_UI AND DAZZLE_LINUX_GTK4_AVAILABLE)
    add_subdirectory(examples/updater_ui_example)
endif()

# Unit tests for the cross-platform logic (registered with CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
