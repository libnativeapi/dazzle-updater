# The language bindings build this library with whatever CMake the user has
# installed. Do not raise the minimum version without a reason; doing so
# breaks those builds for users on older CMake.
cmake_minimum_required(VERSION 3.10)

project(dazzle VERSION 0.0.1 LANGUAGES CXX)

# Bindings that load the library at runtime (Dart FFI) need a shared library;
# bindings that link it (Rust) and the examples use the static one.
option(DAZZLE_BUILD_SHARED "Build dazzle as a shared library" OFF)

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
  "${CMAKE_CURRENT_SOURCE_DIR}/foundation/system_default.cpp")

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
    target_compile_options(dazzle PRIVATE "-x" "objective-c++")
elseif(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    target_link_libraries(dazzle PUBLIC hilog_ndk.z)
elseif(WIN32)
    target_link_libraries(dazzle PUBLIC kernel32)
endif()
