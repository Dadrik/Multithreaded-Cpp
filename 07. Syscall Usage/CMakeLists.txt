cmake_minimum_required(VERSION 2.8.4)
project(syscall_usage)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -fopenmp")

set(SOURCE_FILES syscall_usage.cpp)
add_executable(syscall_usage ${SOURCE_FILES})