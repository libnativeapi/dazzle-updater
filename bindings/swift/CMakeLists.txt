cmake_minimum_required(VERSION 3.10)

# Match Package.swift instead of silently requiring the build machine's macOS.
if(CMAKE_HOST_APPLE AND NOT CMAKE_OSX_DEPLOYMENT_TARGET
        AND NOT DEFINED ENV{MACOSX_DEPLOYMENT_TARGET})
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS version" FORCE)
endif()
project(dazzle_updater_swift LANGUAGES C CXX)

# CMake owns the platform sources and dependencies; Swift imports only the C ABI.
set(DAZZLE_BUILD_SHARED ON CACHE BOOL "Build the Swift binding's native library" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "Build core tests" FORCE)
add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../../core" core)

set_target_properties(dazzle_updater PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<0:>"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<0:>"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/$<0:>"
    MACOSX_RPATH ON
    BUILD_WITH_INSTALL_NAME_DIR ON
    INSTALL_NAME_DIR "@rpath"
)
