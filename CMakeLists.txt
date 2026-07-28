cmake_minimum_required(VERSION 3.16)
project(openjvm)

include(cmake/EmbedResources.cmake)

include_directories(thirdparty)
include_directories(include)

add_subdirectory(thirdparty/ImGui)
add_subdirectory(thirdparty/MinHook)
add_subdirectory(thirdparty/HTTPRequest)
add_subdirectory(thirdparty/nlohmann)

add_subdirectory(scriptsdk)
add_subdirectory(commonsdk)
add_subdirectory(openjvmsdk)
add_subdirectory(launcher)

embed_resources(launcher "${CMAKE_SOURCE_DIR}/resources")
