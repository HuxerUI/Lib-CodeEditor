include_guard(GLOBAL)

function(codeeditor_add_vendor source_dir binary_dir)
    if (NOT CMAKE_HOST_WIN32 OR MSVC)
        add_subdirectory("${source_dir}" "${binary_dir}" EXCLUDE_FROM_ALL)
        return()
    endif ()

    # The upstream archive merge expands absolute paths and shell globs that
    # cmd.exe cannot use. CMake already links the dependent archives.
    get_filename_component(vendor_name "${source_dir}" NAME)
    string(TOUPPER "${vendor_name}" vendor_prefix)
    set(vendor_project "${binary_dir}-source")
    set(vendor_cmake "${source_dir}/CMakeLists.txt")
    set(vendor_third_party "cmake/${vendor_name}ThirdParty.cmake")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
            "${vendor_cmake}" "${source_dir}/${vendor_third_party}"
    )

    file(READ "${vendor_cmake}" project_content)
    string(REPLACE "set(${vendor_prefix}_PROJECT_DIR \${PROJECT_SOURCE_DIR})"
            "set(${vendor_prefix}_PROJECT_DIR \"${source_dir}\")" project_content "${project_content}"
    )
    file(CONFIGURE OUTPUT "${vendor_project}/CMakeLists.txt" CONTENT "${project_content}" @ONLY)

    file(GLOB_RECURSE vendor_modules CONFIGURE_DEPENDS RELATIVE "${source_dir}" "${source_dir}/cmake/*.cmake")
    list(REMOVE_ITEM vendor_modules "cmake/MergeStaticLib.cmake" "${vendor_third_party}")
    foreach (vendor_module IN LISTS vendor_modules)
        configure_file("${source_dir}/${vendor_module}" "${vendor_project}/${vendor_module}" COPYONLY)
    endforeach ()

    file(READ "${source_dir}/${vendor_third_party}" third_party_content)
    string(REPLACE "\${CMAKE_CURRENT_LIST_DIR}/../3dparty/" "\${${vendor_prefix}_PROJECT_DIR}/3dparty/"
            third_party_content "${third_party_content}"
    )
    file(CONFIGURE OUTPUT "${vendor_project}/${vendor_third_party}" CONTENT "${third_party_content}" @ONLY)
    file(CONFIGURE OUTPUT "${vendor_project}/cmake/MergeStaticLib.cmake"
            CONTENT "function(merge_static_libs target)\nendfunction()\n" @ONLY
    )

    add_subdirectory("${vendor_project}" "${binary_dir}" EXCLUDE_FROM_ALL)
endfunction()
