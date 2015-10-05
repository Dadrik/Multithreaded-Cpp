cmake_minimum_required(VERSION 2.8.4)
project(chat)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -levent -gstabs")

set(SOURCE_FILES chat.cpp)
add_executable(chat ${SOURCE_FILES})
