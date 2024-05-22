cmake_minimum_required(VERSION 3.25)
project(d2prices)

set(CMAKE_C_STANDARD 99)
add_executable(d2prices live_decoder.c
        variable_fetchers.h
        db_operations.h
        config.h)
target_link_libraries(d2prices sqlite3)