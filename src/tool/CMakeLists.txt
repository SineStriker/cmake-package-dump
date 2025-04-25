set(_src
    os.h
    os.cpp
    main.cpp
)

if(WIN32)
    list(APPEND _src os_win.cpp)
else()
    list(APPEND _src os_unix.cpp)
endif()

# Add target
add_executable(${PROJECT_NAME} ${_src})

# Add includes and links
target_include_directories(${PROJECT_NAME} PRIVATE .)
target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
set_target_properties(${PROJECT_NAME} PROPERTIES
    CXX_EXTENSIONS OFF
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
target_link_libraries(${PROJECT_NAME} PRIVATE
    stdcorelib::stdcorelib
    syscmdline::syscmdline
)

# Add information
set(RC_DESCRIPTION "${PROJECT_DESCRIPTION}")
set(RC_COPYRIGHT "Copyright (C) 2025 SineStriker")

if(WIN32)
    include("../3rdparty/stdcorelib/cmake/winrc.cmake")
endif()

target_compile_definitions(${PROJECT_NAME} PRIVATE
    TOOL_DESC="${RC_DESCRIPTION}, Version ${PROJECT_VERSION}"
    TOOL_COPYRIGHT="${RC_COPYRIGHT}, checkout ${PROJECT_HOMEPAGE_URL}"
    TOOL_VERSION="${PROJECT_VERSION}"
)

# Embed files
file(GLOB_RECURSE _resources_files "resources/*")

foreach(_file IN LISTS _resources_files)
    get_filename_component(_name ${_file} NAME)
    string(REPLACE "." "_" _name ${_name})
    set(_xdd_file "${CMAKE_CURRENT_BINARY_DIR}/resources/${_name}.cpp")
    add_custom_command(OUTPUT ${_xdd_file}
        COMMAND ${CMAKE_COMMAND}
            -DNAME=${_name}_data
            -DINPUT_FILE=${_file}
            -DOUTPUT_FILE=${_xdd_file} 
            -P ${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/xdd.cmake
        DEPENDS ${_file}
    )
    target_sources(${PROJECT_NAME} PRIVATE ${_xdd_file})
endforeach()

# Install
if(CMAKEDUMP_INSTALL)
    install(TARGETS ${PROJECT_NAME}
        EXPORT ${CMAKEDUMP_INSTALL_NAME}Targets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" OPTIONAL
    )
endif()