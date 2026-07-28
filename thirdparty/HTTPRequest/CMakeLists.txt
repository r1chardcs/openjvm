cmake_minimum_required(VERSION 3.16)

add_library(HTTPRequest INTERFACE)

target_include_directories(HTTPRequest INTERFACE ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(HTTPRequest INTERFACE ws2_32)