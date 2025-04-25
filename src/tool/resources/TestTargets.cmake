include(${XMAKE_FIND_SCRIPT})
get_property(_targets DIRECTORY ${CMAKE_CURRENT_LIST_DIR} PROPERTY IMPORTED_TARGETS)

set(_lib_targets)

foreach(_target IN LISTS _targets)
    if(XMAKE_DUMP_TARGETS AND NOT(${_target} IN_LIST XMAKE_DUMP_TARGETS))
        continue()
    endif()

    set(_includes)
    get_target_property(_loc ${_target} IMPORTED_LOCATION)

    if(NOT _loc)
        get_target_property(_loc ${_target} IMPORTED_LOCATION_${XMAKE_CONFIG_UPPER})
    endif()

    if(NOT _loc)
        # Maybe a header-only library
        get_target_property(_includes ${_target} INTERFACE_INCLUDE_DIRECTORIES)

        if(NOT _includes)
            continue()
        endif()
    else()
        get_filename_component(_name ${_loc} NAME)

        if(NOT _name MATCHES "^.*(\\.lib|\\.a|\\.dll|\\.so|\\.dylib)")
            continue()
        endif()
    endif()

    list(APPEND _lib_targets ${_target})
endforeach()

set(XMAKE_FIND_TARGETS "${_lib_targets}" PARENT_SCOPE)