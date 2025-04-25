if(NOT DEFINED INPUT_FILE)
    message(FATAL_ERROR "INPUT_FILE is not defined")
endif()

if(NOT DEFINED OUTPUT_FILE)
    message(FATAL_ERROR "OUTPUT_FILE is not defined")
endif()

if(NOT DEFINED NAME)
    message(FATAL_ERROR "NAME is not defined")
endif()

get_filename_component(_output_dir ${OUTPUT_FILE} DIRECTORY)
if(NOT EXISTS ${_output_dir})
    file(MAKE_DIRECTORY ${_output_dir})
endif()

file(READ ${INPUT_FILE} _file_content HEX)
string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1, " _hex_content ${_file_content})
file(WRITE ${OUTPUT_FILE}
    "static unsigned char ${NAME}_data[] = {${_hex_content}};\n"
    "static unsigned int ${NAME}_size = sizeof(${NAME}_data);\n"
    "struct BinaryData {\n"
    "    const unsigned char *data;\n"
    "    const unsigned int size;\n"
    "};\n"
    "struct BinaryData ${NAME} = {${NAME}_data, ${NAME}_size};\n"
)