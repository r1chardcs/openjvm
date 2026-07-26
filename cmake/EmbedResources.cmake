function(_sanitize_var_name IN_NAME OUT_VAR)
    string(MAKE_C_IDENTIFIER "${IN_NAME}" SANITIZED)
    set(${OUT_VAR} "${SANITIZED}" PARENT_SCOPE)
endfunction()

function(embed_resources TARGET_NAME RESOURCES_DIR)
    get_filename_component(RESOURCES_DIR_ABS "${RESOURCES_DIR}" ABSOLUTE)

    if(NOT EXISTS "${RESOURCES_DIR_ABS}")
        message(STATUS "embed_resources: '${RESOURCES_DIR_ABS}' not found, skipping.")
        return()
    endif()

    file(GLOB_RECURSE RESOURCE_FILES RELATIVE "${RESOURCES_DIR_ABS}" "${RESOURCES_DIR_ABS}/*")

    set(GENERATED_DIR "${CMAKE_SOURCE_DIR}/include/resources")
    set(GENERATED_HEADERS "")
    set(AGGREGATE_INCLUDES "")

    foreach(REL_FILE ${RESOURCE_FILES})
        get_filename_component(BASE_NAME "${REL_FILE}" NAME_WE)
        get_filename_component(SUB_DIR "${REL_FILE}" DIRECTORY)
        _sanitize_var_name("${BASE_NAME}" VAR_NAME)

        if(SUB_DIR)
            set(OUTPUT_HEADER "${GENERATED_DIR}/${SUB_DIR}/${VAR_NAME}.h")
        else()
            set(OUTPUT_HEADER "${GENERATED_DIR}/${VAR_NAME}.h")
        endif()

        set(INPUT_ABS "${RESOURCES_DIR_ABS}/${REL_FILE}")

        string(REPLACE "\"" "" INPUT_ABS_CLEAN "${INPUT_ABS}")
        string(REPLACE "\"" "" OUTPUT_HEADER_CLEAN "${OUTPUT_HEADER}")

        add_custom_command(
                OUTPUT "${OUTPUT_HEADER}"
                COMMAND "${CMAKE_COMMAND}"
                -DINPUT_FILE="${INPUT_ABS_CLEAN}"
                -DOUTPUT_FILE="${OUTPUT_HEADER_CLEAN}"
                -DVAR_NAME="${VAR_NAME}"
                -P "${CMAKE_SOURCE_DIR}/cmake/bin2h.cmake"
                DEPENDS "${INPUT_ABS}" "${CMAKE_SOURCE_DIR}/cmake/bin2h.cmake"
                COMMENT "Embedding resource: ${REL_FILE} -> ${VAR_NAME}"
                VERBATIM
        )

        list(APPEND GENERATED_HEADERS "${OUTPUT_HEADER}")
        if(SUB_DIR)
            list(APPEND AGGREGATE_INCLUDES "#include \"resources/${SUB_DIR}/${VAR_NAME}.h\"")
        else()
            list(APPEND AGGREGATE_INCLUDES "#include \"resources/${VAR_NAME}.h\"")
        endif()
    endforeach()

    set(AGGREGATE_HEADER "${GENERATED_DIR}/resources.h")
    set(AGGREGATE_CONTENT "// Auto-generated aggregate header. Do not edit.\n#pragma once\n\n")
    foreach(INC ${AGGREGATE_INCLUDES})
        string(APPEND AGGREGATE_CONTENT "${INC}\n")
    endforeach()
    file(WRITE "${AGGREGATE_HEADER}" "${AGGREGATE_CONTENT}")

    set(STAMP_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}_resources.stamp")
    add_custom_command(
            OUTPUT "${STAMP_FILE}"
            COMMAND "${CMAKE_COMMAND}" -E touch "${STAMP_FILE}"
            DEPENDS ${GENERATED_HEADERS}
            VERBATIM
    )

    add_custom_target(${TARGET_NAME}_embed_resources DEPENDS "${STAMP_FILE}")
    add_dependencies(${TARGET_NAME} ${TARGET_NAME}_embed_resources)

    target_include_directories(${TARGET_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/include")
endfunction()