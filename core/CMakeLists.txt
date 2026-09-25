cmake_minimum_required(VERSION 3.10)

project(dazzle_library VERSION 0.0.1 LANGUAGES CXX C)

include(CTest)

# Add library subdirectory
add_subdirectory(src)

# Add example programs subdirectory
add_subdirectory(examples/updater_example)
add_subdirectory(examples/updater_c_example)

# Unit tests for the cross-platform logic (registered with CTest)
if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
