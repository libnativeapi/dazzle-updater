cmake_minimum_required(VERSION 3.10)

project(updater_example)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_executable(updater_example main.cpp)

target_link_libraries(updater_example dazzle_updater)
