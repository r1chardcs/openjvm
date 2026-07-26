cmake_minimum_required(VERSION 3.16)

add_library(commonsdk SHARED
        src/common_memory.cpp
        src/common_exception.cpp
        include/common_system.h
        src/common_system.cpp
)

target_include_directories(commonsdk PUBLIC include)

