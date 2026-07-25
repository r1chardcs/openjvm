cmake_minimum_required(VERSION 3.16)

add_library(commonsdk SHARED
        src/common_memory.cpp
        src/common_exception.cpp
        )

target_include_directories(commonsdk PUBLIC include)

