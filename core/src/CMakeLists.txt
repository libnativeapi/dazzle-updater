# The language bindings build this library with whatever CMake the user has
# installed. Do not raise the minimum version without a reason; doing so
# breaks those builds for users on older CMake.
cmake_minimum_required(VERSION 3.10)

project(dazzle VERSION 0.0.1 LANGUAGES CXX)

# Bindings that load the library at runtime (Dart FFI) need a shared library;
# bindings that link it (Rust) and the examples use the static one.
option(DAZZLE_BUILD_SHARED "Build dazzle as a shared library" OFF)

# UpdaterUI's native windows (../../specs/updater-ui.md) are optional. OFF leaves the
# window and preferences implementations of this platform out of the build, together
# with the Markdown renderer and the system UI frameworks they need; the entry points
# and the C ABI stay, and answer false, exactly as they do on a platform that has no
# built-in windows. Nothing in the sources is conditioned on this option: it decides
# which file implements the two seams (see below), which is also why an application or a
# binding built either way links and runs.
option(DAZZLE_WITH_UPDATER_UI "Build the built-in UpdaterUI native windows" ON)

# Enable Objective-C++
if(APPLE)
    enable_language(OBJCXX)
endif()

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Common source files, minus the fallback implementations of the platform seams: those
# are added below, and only when the platform directory has no file for that seam.
file(GLOB COMMON_SOURCES "*.cpp" "foundation/*.cpp")
list(REMOVE_ITEM COMMON_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/foundation/http_default.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/foundation/system_default.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/foundation/updater_ui_backend_default.cpp"
  "${CMAKE_CURRENT_SOURCE_DIR}/foundation/updater_ui_preferences_default.cpp")

# Vendored third-party code (core/third_party/, never edited locally): Monocypher
# provides Ed25519, nlohmann/json is a single header included by the release index
# parser. Both are compiled into the library and hidden from it (PRIVATE include).
set(THIRD_PARTY_SOURCES
  "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/monocypher/monocypher.c"
  "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/monocypher/monocypher-ed25519.c"
)
set(THIRD_PARTY_INCLUDE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/../third_party")

# C API source files
file(GLOB CAPI_SOURCES "capi/*.cpp" "capi/*.c")

# Platform-specific source files. ANDROID and iOS must be checked before the
# generic APPLE branch, or they fall through to the desktop implementation.
if(ANDROID)
    set(PLATFORM_DIR "platform/android")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    set(PLATFORM_DIR "platform/ios")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(PLATFORM_DIR "platform/linux")
elseif(APPLE)
    set(PLATFORM_DIR "platform/macos")
elseif(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    set(PLATFORM_DIR "platform/ohos")
elseif(WIN32)
    set(PLATFORM_DIR "platform/windows")
else()
    set(PLATFORM_DIR "")
endif()

set(PLATFORM_SOURCES "")
if(PLATFORM_DIR)
    file(GLOB PLATFORM_SOURCES "${PLATFORM_DIR}/*.cpp" "${PLATFORM_DIR}/*.mm")
endif()

# A build without the built-in UI drops this platform's windows and its preference store,
# so that the same fallbacks below answer for it. The globs above pick every file up by
# extension, so this is the only place the option has to be considered.
if(NOT DAZZLE_WITH_UPDATER_UI)
    list(FILTER PLATFORM_SOURCES EXCLUDE REGEX "updater_ui_")
else()
    # Markdown for the notes box: the parser is vendored (third_party/md4c), the writers
    # are dazzle's own. Only the windows render Markdown, so only they pull it in.
    file(GLOB MARKDOWN_SOURCES "markdown/*.cpp")
    list(APPEND COMMON_SOURCES ${MARKDOWN_SOURCES})
    list(APPEND THIRD_PARTY_SOURCES
      "${CMAKE_CURRENT_SOURCE_DIR}/../third_party/md4c/md4c.c")
endif()

# Every platform seam has a cross-platform default, and a platform overrides it by
# providing <seam>_<os> in its directory. So a platform needs source files only for the
# behavior it really implements: Android, iOS and OHOS carry none today, and Windows and
# Linux keep only what they already support.
set(SEAM_FALLBACK_SOURCES "")
if(NOT "${PLATFORM_SOURCES}" MATCHES "http_")
    list(APPEND SEAM_FALLBACK_SOURCES "foundation/http_default.cpp")
endif()
if(NOT "${PLATFORM_SOURCES}" MATCHES "system_")
    list(APPEND SEAM_FALLBACK_SOURCES "foundation/system_default.cpp")
endif()
# UpdaterUI's two seams: the windows, and what the user answered. A platform without
# either gets the cross-platform default, which is also what DAZZLE_WITH_UPDATER_UI=OFF
# ends up compiling for every platform.
if(NOT "${PLATFORM_SOURCES}" MATCHES "updater_ui_backend_")
    list(APPEND SEAM_FALLBACK_SOURCES "foundation/updater_ui_backend_default.cpp")
endif()
if(NOT "${PLATFORM_SOURCES}" MATCHES "updater_ui_preferences_")
    list(APPEND SEAM_FALLBACK_SOURCES "foundation/updater_ui_preferences_default.cpp")
endif()

# The macOS windows and their preference store are written for ARC; the older
# Objective-C++ files here manage their objects by hand. Set per source file, so the flag
# reaches neither the vendored C nor anything else.
if(APPLE AND DAZZLE_WITH_UPDATER_UI)
    set_source_files_properties(
      "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/updater_ui_backend_macos.mm"
      "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/updater_ui_controls_macos.mm"
      "${CMAKE_CURRENT_SOURCE_DIR}/platform/macos/updater_ui_preferences_macos.mm"
      PROPERTIES COMPILE_OPTIONS "-fobjc-arc")
endif()

# Add library target
if(DAZZLE_BUILD_SHARED)
  set(DAZZLE_LIBRARY_TYPE SHARED)
else()
  set(DAZZLE_LIBRARY_TYPE STATIC)
endif()

add_library(dazzle ${DAZZLE_LIBRARY_TYPE}
  ${COMMON_SOURCES}
  ${PLATFORM_SOURCES}
  ${CAPI_SOURCES}
  ${THIRD_PARTY_SOURCES}
  ${SEAM_FALLBACK_SOURCES}
)

# Propagate the language requirement to anything that links dazzle; the
# CMAKE_CXX_STANDARD variables above only govern this directory's targets.
target_compile_features(dazzle PUBLIC cxx_std_17)

# Set library include directories
target_include_directories(dazzle PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:include>
)

# The vendored headers stay an implementation detail of the library.
target_include_directories(dazzle PRIVATE "${THIRD_PARTY_INCLUDE_DIR}")

# Link required frameworks and libraries
if(ANDROID)
    target_link_libraries(dazzle PUBLIC log android)
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    target_link_libraries(dazzle PUBLIC "-framework Foundation")
    target_compile_options(dazzle PRIVATE "-x" "objective-c++")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    target_link_libraries(dazzle PUBLIC pthread)
elseif(APPLE)
    target_link_libraries(dazzle PUBLIC "-framework Foundation")
    # The Objective-C++ files are the platform ones, and the `.mm` extension is already
    # what makes CMake compile them as OBJCXX (enable_language above). The vendored C
    # files are therefore left as C: md4c does not compile as Objective-C++.
    # The windows are AppKit; a build without them does not link it at all.
    if(DAZZLE_WITH_UPDATER_UI)
        target_link_libraries(dazzle PUBLIC "-framework AppKit")
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    target_link_libraries(dazzle PUBLIC hilog_ndk.z)
elseif(WIN32)
    target_link_libraries(dazzle PUBLIC kernel32)
    # The windows draw their own Fluent controls (Direct2D + DirectWrite), follow the
    # system theme through DWM and expose themselves to UI Automation.
    if(DAZZLE_WITH_UPDATER_UI)
        target_link_libraries(dazzle PUBLIC user32 gdi32 shell32 d2d1 dwrite dwmapi uiautomationcore)
    endif()
endif()
