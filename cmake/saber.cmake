if(MMX_STATE_TESTS AND TARGET mmx_state_tests)
    get_target_property(_saber_sources mmx_state_tests SOURCES)
    list(REMOVE_ITEM _saber_sources
        tests/mmx_adaptive_state_test.c
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/mmx_adaptive_state_test.c")

    add_executable(mmx_saber_rom_tests
        tests/saber/saber_rom_test.c ${_saber_sources})
    foreach(_property INCLUDE_DIRECTORIES COMPILE_DEFINITIONS COMPILE_OPTIONS LINK_LIBRARIES LINK_OPTIONS)
        get_target_property(_value mmx_state_tests ${_property})
        if(_value)
            set_property(TARGET mmx_saber_rom_tests PROPERTY ${_property} "${_value}")
        endif()
    endforeach()
    add_dependencies(mmx_saber_rom_tests MegaManXSNESRecomp_widescreen_overrides)
endif()
