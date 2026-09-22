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

# Common source files
file(GLOB COMMON_SOURCES "*.cpp" "foundation/*.cpp")

# C API source files
file(GLOB CAPI_SOURCES "capi/*.cpp" "capi/*.c")

# Platform-specific source files. ANDROID and iOS must be checked before the
# generic APPLE branch, or they fall through to the desktop implementation.
if(ANDROID)
    file(GLOB PLATFORM_SOURCES "platform/android/*.cpp")
elseif(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    file(GLOB PLATFORM_SOURCES "platform/ios/*.mm")
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    file(GLOB PLATFORM_SOURCES "platform/linux/*.cpp")
elseif(APPLE)
    file(GLOB PLATFORM_SOURCES "platform/macos/*.mm")
elseif(CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    file(GLOB PLATFORM_SOURCES "platform/ohos/*.cpp")
elseif(WIN32)
    file(GLOB PLATFORM_SOURCES "platform/windows/*.cpp")
else()
    set(PLATFORM_SOURCES "")
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
)

# Propagate the language requirement to anything that links dazzle; the
# CMAKE_CXX_STANDARD variables above only govern this directory's targets.
target_compile_features(dazzle PUBLIC cxx_std_17)

# Set library include directories
target_include_directories(dazzle PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/../include>
  $<INSTALL_INTERFACE:include>
)

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
