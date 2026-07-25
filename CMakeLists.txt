cmake_minimum_required(VERSION 3.16)
project(openjvm)

include_directories(thirdparty)

add_subdirectory(thirdparty/ImGui)
add_subdirectory(thirdparty/MinHook)

add_subdirectory(scriptsdk)
add_subdirectory(commonsdk)
add_subdirectory(openjvmsdk)