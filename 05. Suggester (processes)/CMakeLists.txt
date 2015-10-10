cmake_minimum_required(VERSION 2.8.4)
project(process_suggester)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

set(SOURCE_FILES process_suggester.cpp)
add_executable(suggester ${SOURCE_FILES})