cmake_minimum_required(VERSION 3.16)

add_executable(launcher main.cpp
        api.h
        api.cpp)

target_link_libraries(launcher PRIVATE ImGui openjvmsdk scriptsdk commonsdk)

add_custom_command(
        TARGET launcher POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_CURRENT_BINARY_DIR}/sdk"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:openjvmsdk>"
        "${CMAKE_CURRENT_BINARY_DIR}/sdk/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:scriptsdk>"
        "${CMAKE_CURRENT_BINARY_DIR}/sdk/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:commonsdk>"
        "${CMAKE_CURRENT_BINARY_DIR}/sdk/"
        COMMENT "Copying SDK DLLs to sdk directory"
        VERBATIM
)

target_link_options(launcher PRIVATE
        -static
)