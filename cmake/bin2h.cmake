string(REPLACE "\"" "" INPUT_FILE "${INPUT_FILE}")
string(REPLACE "\"" "" OUTPUT_FILE "${OUTPUT_FILE}")
string(REPLACE "\"" "" VAR_NAME "${VAR_NAME}")

file(READ "${INPUT_FILE}" HEX_CONTENT HEX)
string(LENGTH "${HEX_CONTENT}" HEX_LENGTH)

math(EXPR BYTE_COUNT "${HEX_LENGTH} / 2")
string(REGEX REPLACE "(..)" "0x\\1," HEX_ARRAY "${HEX_CONTENT}")
string(REGEX REPLACE ",$" "" HEX_ARRAY "${HEX_ARRAY}")


file(WRITE "${OUTPUT_FILE}"
        "// Auto-generated from ${INPUT_FILE}. Do not edit.\n"
        "#pragma once\n"
        "#include <cstddef>\n\n"
        "namespace Resources {\n"
        "    inline constexpr unsigned char ${VAR_NAME}[] = { ${HEX_ARRAY} };\n"
        "    inline constexpr std::size_t ${VAR_NAME}_size = ${BYTE_COUNT};\n"
        "}\n"
)

message(STATUS "Generated: ${OUTPUT_FILE} (${BYTE_COUNT} bytes)")