# check for pkg-config
find_package(PkgConfig REQUIRED)

# check for .ttl validation software
find_program(LV2V_SORD_VALIDATE NAMES sord_validate REQUIRED)
find_program(LV2V_SORDI NAMES sordi REQUIRED)

# check for LV2 schemas
if(NOT LV2_SCHEMA_LIST)
    pkg_check_modules(LV2 REQUIRED "lv2")

    pkg_get_variable(dir_lv2dir "lv2" "lv2dir")

    if(NOT dir_lv2dir)
        message(FATAL_ERROR "Unable to find LV2 directory from pkg-config!")
    endif()

    message(STATUS "found LV2 directory: ${dir_lv2dir}")

    set(lv2_schema_list)

    foreach(schema_dir IN ITEMS
        "${dir_lv2dir}/atom.lv2"
        "${dir_lv2dir}/buf-size.lv2"
        "${dir_lv2dir}/core.lv2"
        "${dir_lv2dir}/data-access.lv2"
        "${dir_lv2dir}/dynmanifest.lv2"
        "${dir_lv2dir}/event.lv2"
        "${dir_lv2dir}/instance-access.lv2"
        "${dir_lv2dir}/log.lv2"
        "${dir_lv2dir}/midi.lv2"
        "${dir_lv2dir}/morph.lv2"
        "${dir_lv2dir}/options.lv2"
        "${dir_lv2dir}/parameters.lv2"
        "${dir_lv2dir}/patch.lv2"
        "${dir_lv2dir}/port-groups.lv2"
        "${dir_lv2dir}/port-props.lv2"
        "${dir_lv2dir}/presets.lv2"
        "${dir_lv2dir}/resize-port.lv2"
        "${dir_lv2dir}/schemas.lv2"
        "${dir_lv2dir}/state.lv2"
        "${dir_lv2dir}/time.lv2"
        "${dir_lv2dir}/ui.lv2"
        "${dir_lv2dir}/units.lv2"
        "${dir_lv2dir}/uri-map.lv2"
        "${dir_lv2dir}/urid.lv2"
        "${dir_lv2dir}/worker.lv2"
    )
        file(GLOB next_group "${schema_dir}/*.ttl")
        list(APPEND lv2_schema_list ${next_group})
    endforeach()

    set(LV2_SCHEMA_LIST "${lv2_schema_list}" CACHE INTERNAL "list of found LV2 schemas")
endif()

if(${CMAKE_HOST_UNIX})
    set(path_devnull "/dev/null")
elseif(${CMAKE_HOST_WIN32})
    set(path_devnull "C:\\nul")
endif()

function(lv2v_add_ttl_sources)
    cmake_parse_arguments(PARSE_ARGV 0 lv2v

        # flag
        ""

        # one-arg
        "TARGET"

        # multi-arg
        "FILES"
    )

    if(NOT lv2v_TARGET)
        message(FATAL_ERROR "missing target")
    endif()

    # create folder if not exists
    get_target_property(lv2v_gendir "${lv2v_TARGET}" BINARY_DIR)
    set(lv2v_gendir "${lv2v_gendir}/${lv2v_TARGET}!lv2v.dir")
    file(MAKE_DIRECTORY "${lv2v_gendir}")

    foreach(fpath IN LISTS lv2v_FILES)
        # normalize file path relative to current directory
        cmake_path(IS_ABSOLUTE fpath fpath_is_abs)
        if (fpath_is_abs)
            cmake_path(RELATIVE_PATH fpath)
        endif()
        cmake_path(NORMAL_PATH fpath)

        # define lockfile for output tracking
        string(REGEX REPLACE "\\.\\.([\\/])" "__\\1" opath "${fpath}")
        set(opath "${opath}.lock")
        cmake_path(ABSOLUTE_PATH opath BASE_DIRECTORY "${lv2v_gendir}")

        # validation process:
        # validate prefixes using sord_validate
        # validate syntax using sordi
        # touch lockfile to confirm verification
        add_custom_command(
            OUTPUT "${opath}"
            DEPENDS "${fpath}"
            COMMENT "Validating ${fpath}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
            COMMAND "${LV2V_SORD_VALIDATE}" ARGS "${fpath}" ${LV2_SCHEMA_LIST}
            COMMAND "${LV2V_SORDI}" ARGS "${fpath}" > "${path_devnull}"
            COMMAND "${CMAKE_COMMAND}" ARGS -E touch "${opath}"
        )

        target_sources("${lv2v_TARGET}" PRIVATE "${opath}")
    endforeach()
endfunction()
