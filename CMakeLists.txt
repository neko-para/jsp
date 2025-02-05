cmake_minimum_required(VERSION 3.20)

project(jsp)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED YES)
set(CMAKE_EXPORT_COMPILE_COMMANDS YES)

add_library(jsp INTERFACE)
target_include_directories(jsp INTERFACE include)

add_executable(test test/test.cpp)
target_link_libraries(test PRIVATE jsp)
