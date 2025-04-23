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

# Add platform specific
if(WIN32)
    set(RC_DESCRIPTION "CMake Package Dump")
    set(RC_COPYRIGHT "Copyright (C) 2025 SineStriker")
    include("../3rdparty/stdcorelib/cmake/winrc.cmake")
endif()

if(CMAKEDUMP_INSTALL)
    install(TARGETS ${PROJECT_NAME}
        EXPORT ${CMAKEDUMP_INSTALL_NAME}Targets
        RUNTIME DESTINATION "${CMAKE_INSTALL_BINDIR}" OPTIONAL
    )
endif()