cmake_minimum_required(VERSION 3.16)

file(GLOB_RECURSE NLOHMANN_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.c"
        "${CMAKE_CURRENT_SOURCE_DIR}/*.hpp"
)

add_library(nlohmann STATIC file.cpp ${NLOHMANN_SOURCES})

target_include_directories(nlohmann PUBLIC
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${CMAKE_CURRENT_SOURCE_DIR}/src
)

target_compile_definitions(nlohmann PRIVATE NLOHMANN_JSON_BUILD)