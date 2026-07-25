cmake_minimum_required(VERSION 3.16)
project(ScriptSDK)

add_library(scriptsdk SHARED src/scriptsdk.cpp thirdparty/py/pocketpy.c)

target_compile_definitions(scriptsdk PRIVATE SELF_BUILD)
target_include_directories(scriptsdk PUBLIC include)
target_include_directories(scriptsdk PRIVATE thirdparty)
target_link_libraries(scriptsdk PUBLIC commonsdk ws2_32)

add_subdirectory(tests/simplescript)

