include(${XMAKE_FIND_SCRIPT})
get_property(_targets DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY IMPORTED_TARGETS)

function(_get_imported_location _target _var)
    get_target_property(_path ${_target} IMPORTED_LOCATION)

    if(NOT _path)
        get_target_property(_path ${_target} IMPORTED_LOCATION_${XMAKE_CONFIG_UPPER})
    endif()

    if(NOT _path)
        get_target_property(_path ${_target} IMPORTED_LOCATION_RELEASE)
    endif()

    if(NOT _path)
        get_target_property(_path ${_target} IMPORTED_LOCATION_MINSIZEREL)
    endif()

    if(NOT _path)
        get_target_property(_path ${_target} IMPORTED_LOCATION_RELWITHDEBINFO)
    endif()

    if(NOT _path)
        get_target_property(_path ${_target} IMPORTED_LOCATION_DEBUG)
    endif()

    if(NOT _path)
        return()
    endif()

    set(${_var} ${_path} PARENT_SCOPE)
endfunction()

set(_lib_targets)
set(_all_lib_targets)

# Get library targets
foreach(_target IN LISTS _targets)
    set(_includes)
    set(_loc)
    _get_imported_location(${_target} _loc)

    if(NOT _loc)
        # Maybe a header-only library
        get_target_property(_includes ${_target} INTERFACE_INCLUDE_DIRECTORIES)

        if(NOT _includes)
            continue()
        endif()
    else()
        get_filename_component(_name ${_loc} NAME)
        string(TOLOWER ${_name} _name_lower)

        if(NOT _name_lower MATCHES "^.*(\\.lib|\\.a|\\.dll|\\.so|\\.dylib)")
            continue()
        endif()
    endif()

    list(APPEND _all_targets ${_target})

    if(XMAKE_DUMP_TARGETS AND NOT(${_target} IN_LIST XMAKE_DUMP_TARGETS))
        continue()
    endif()

    list(APPEND _lib_targets ${_target})
endforeach()

set(_exe_targets_paths)

# Get executable targets
foreach(_target IN LISTS _targets)
    if(${_target} IN_LIST _all_targets)
        continue()
    endif()

    set(_loc)
    _get_imported_location(${_target} _loc)

    if(NOT _loc)
        continue()
    endif()

    if(WIN32)
        get_filename_component(_name ${_loc} NAME)
        string(TOLOWER ${_name} _name_lower)

        if(NOT _name MATCHES "^.*(\\.exe|\\.bat)")
            continue()
        endif()
    else()
        execute_process(
            COMMAND sh -c "test -x '${_loc}'"
            RESULT_VARIABLE _result
            OUTPUT_QUIET
            ERROR_QUIET
        )

        if(NOT(_result EQUAL 0))
            continue()
        endif()
    endif()

    if(XMAKE_DUMP_TARGETS AND NOT(${_target} IN_LIST XMAKE_DUMP_TARGETS))
        continue()
    endif()

    list(APPEND _exe_targets_paths ${_target} ${_loc})
endforeach()

set(XMAKE_LIBRARY_TARGETS "${_lib_targets}" PARENT_SCOPE)
set(XMAKE_EXECUTABLE_TARGETS_PATHS "${_exe_targets_paths}" PARENT_SCOPE)