cmake_minimum_required(VERSION 3.16)
project(search_server)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Threads REQUIRED)

add_executable(search_server src/app/main.cpp)
target_link_libraries(search_server Threads::Threads)