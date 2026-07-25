cmake_minimum_required(VERSION 3.16)
project(SimpleScript)

add_executable(simplescript main.cpp)

target_link_libraries(simplescript PRIVATE scriptsdk)

add_custom_command(
        TARGET simplescript POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:scriptsdk>"
        "$<TARGET_FILE_DIR:simplescript>"
        COMMENT "Copying scriptsdk.dll to output directory"
        VERBATIM
)

add_custom_command(
        TARGET simplescript POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
        "$<TARGET_FILE:commonsdk>"
        "$<TARGET_FILE_DIR:simplescript>"
        COMMENT "Copying commonsdk.dll to scriptsdk output directory"
        VERBATIM
)
