cmake_minimum_required(VERSION 3.31)
project(JerryParser)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(ZLIB REQUIRED)

add_executable(JerryParser main.cpp)

target_link_libraries(JerryParser PRIVATE ZLIB::ZLIB)
