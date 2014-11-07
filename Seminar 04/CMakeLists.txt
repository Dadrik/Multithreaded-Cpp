cmake_minimum_required(VERSION 2.8.4)
project(shell)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES shell.cpp)
add_executable(shell ${SOURCE_FILES})