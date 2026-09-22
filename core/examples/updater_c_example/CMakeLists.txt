cmake_minimum_required(VERSION 3.10)

project(updater_c_example)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_executable(updater_c_example main.c)

target_link_libraries(updater_c_example dazzle)
