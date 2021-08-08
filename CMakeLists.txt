cmake_minimum_required(VERSION 3.19)
project(Test)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -lcurl")

set(SOURCE_FILES main.cpp)
add_executable(Test main.cpp)
target_link_libraries(Test curl)