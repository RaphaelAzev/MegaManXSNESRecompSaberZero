set(_saber_plugin_source "${CMAKE_CURRENT_SOURCE_DIR}/src/mods/mmx_saber_plugin.c")
set(_saber_assets_source "${CMAKE_CURRENT_SOURCE_DIR}/src/saber/mmx_saber_assets.c")
set(_saber_converter "${CMAKE_CURRENT_SOURCE_DIR}/tools/saber/convert_saber_zero.py")
set(_saber_manifest "${CMAKE_CURRENT_SOURCE_DIR}/tools/saber/saber_zero_manifest.json")
set(_ride_manifest "${CMAKE_CURRENT_SOURCE_DIR}/tools/saber/ride_zero_manifest.json")
set(_saber_source_dir "${CMAKE_CURRENT_SOURCE_DIR}/SaberSprites")
set(_saber_cache_dir "${CMAKE_CURRENT_BINARY_DIR}/cache/mmx-source")
set(_saber_cache "${_saber_cache_dir}/saber-v1.bin")
set(_ride_cache "${_saber_cache_dir}/ride-zero-v1.bin")

# The donor sheets are deliberately private and ignored. When they are
# present, build the same deterministic v1 caches as the old branch beside
# the executable. On a checkout without the private inputs the runtime/unit
# test remains buildable and reports SKIPPED for the missing cache instead.
file(GLOB _saber_donor_files CONFIGURE_DEPENDS
    "${_saber_source_dir}/*.png")
if(EXISTS "${_saber_source_dir}" AND _saber_donor_files AND
   EXISTS "${_saber_manifest}" AND EXISTS "${_ride_manifest}")
    add_custom_command(
        OUTPUT "${_saber_cache}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_saber_cache_dir}"
        COMMAND "${Python3_EXECUTABLE}" -I "${_saber_converter}"
            --manifest "${_saber_manifest}"
            --source-dir "${_saber_source_dir}"
            --out "${_saber_cache}"
        DEPENDS "${_saber_converter}" "${_saber_manifest}" ${_saber_donor_files}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
    add_custom_command(
        OUTPUT "${_ride_cache}"
        COMMAND "${CMAKE_COMMAND}" -E make_directory "${_saber_cache_dir}"
        COMMAND "${Python3_EXECUTABLE}" -I "${_saber_converter}"
            --manifest "${_ride_manifest}"
            --source-dir "${_saber_source_dir}"
            --out "${_ride_cache}"
        DEPENDS "${_saber_converter}" "${_ride_manifest}" ${_saber_donor_files}
        WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
        VERBATIM)
    add_custom_target(saber_asset_caches DEPENDS "${_saber_cache}" "${_ride_cache}")
endif()

if(TARGET MegaManXSNESRecomp)
    target_sources(MegaManXSNESRecomp PRIVATE
        "${_saber_plugin_source}" "${_saber_assets_source}")
    if(TARGET saber_asset_caches)
        add_dependencies(MegaManXSNESRecomp saber_asset_caches)
    endif()
endif()

if(MMX_STATE_TESTS AND TARGET mmx_state_tests)
    target_sources(mmx_state_tests PRIVATE
        "${_saber_plugin_source}" "${_saber_assets_source}")
    if(TARGET saber_asset_caches)
        add_dependencies(mmx_state_tests saber_asset_caches)
    endif()
    get_target_property(_saber_sources mmx_state_tests SOURCES)
    list(REMOVE_ITEM _saber_sources
        tests/mmx_adaptive_state_test.c
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/mmx_adaptive_state_test.c"
        src/mods/mmx_saber_plugin.c
        "${_saber_plugin_source}"
        src/saber/mmx_saber_assets.c
        "${_saber_assets_source}")

    add_executable(mmx_saber_rom_tests
        tests/saber/saber_rom_test.c ${_saber_sources}
        "${_saber_plugin_source}" "${_saber_assets_source}")
    foreach(_property INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS LINK_LIBRARIES LINK_OPTIONS)
        get_target_property(_value mmx_state_tests ${_property})
        if(_value)
            set_property(TARGET mmx_saber_rom_tests PROPERTY ${_property} "${_value}")
        endif()
    endforeach()
    add_dependencies(mmx_saber_rom_tests MegaManXSNESRecomp_widescreen_overrides)
    if(TARGET saber_asset_caches)
        add_dependencies(mmx_saber_rom_tests saber_asset_caches)
    endif()
endif()

if(BUILD_TESTING)
    add_executable(mmx_saber_check
        "${CMAKE_CURRENT_SOURCE_DIR}/tools/saber/mmx_saber_check.c"
        "${_saber_assets_source}"
        "${SNESRECOMP_ROOT}/runner/src/sha256.c")
    target_include_directories(mmx_saber_check PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/saber"
        "${SNESRECOMP_ROOT}/runner/src")

    add_executable(mmx_saber_assets_test
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/saber/mmx_saber_assets_test.c"
        "${_saber_assets_source}"
        "${SNESRECOMP_ROOT}/runner/src/sha256.c")
    target_include_directories(mmx_saber_assets_test PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/saber"
        "${SNESRECOMP_ROOT}/runner/src")
    target_compile_definitions(mmx_saber_assets_test PRIVATE
        MMX_SABER_CACHE_DIR="${_saber_cache_dir}")
    add_test(NAME mmx_saber_assets_test COMMAND mmx_saber_assets_test)
    set_tests_properties(mmx_saber_assets_test PROPERTIES SKIP_RETURN_CODE 77)
    if(TARGET saber_asset_caches)
        add_dependencies(mmx_saber_check saber_asset_caches)
        add_dependencies(mmx_saber_assets_test saber_asset_caches)
    endif()

endif()
