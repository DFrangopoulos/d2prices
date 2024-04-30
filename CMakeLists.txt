cmake_minimum_required(VERSION 3.25)
project(d2prices)

set(CMAKE_C_STANDARD 99)
add_executable(d2prices live_decoder.c)
target_link_libraries(d2prices sqlite3)