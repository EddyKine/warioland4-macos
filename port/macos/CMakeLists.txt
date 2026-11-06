# Experimental Darwin backend, isolated from the GBA and Windows paths.
set(WL4_DARWIN_GENERATED ${CMAKE_BINARY_DIR}/darwin-generated)
option(WL4_DARWIN_AUDIO_EXPERIMENT "Enable unvalidated audio continuity experiment" OFF)
set(_audio_experiment_argument)
if(WL4_DARWIN_AUDIO_EXPERIMENT)
    set(_audio_experiment_argument --experimental-audio)
endif()
execute_process(COMMAND python3 ${CMAKE_SOURCE_DIR}/port/macos/prepare.py ${CMAKE_SOURCE_DIR} ${WL4_DARWIN_GENERATED} ${_audio_experiment_argument} COMMAND_ERROR_IS_FATAL ANY)
include_directories(BEFORE ${WL4_DARWIN_GENERATED}/include ${CMAKE_SOURCE_DIR}/port/macos)
add_compile_options($<$<COMPILE_LANGUAGE:C>:-include${CMAKE_SOURCE_DIR}/port/macos/compat.h>)

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${CMAKE_SOURCE_DIR}/port/macos/prepare.py
    ${CMAKE_SOURCE_DIR}/port/macos/address.h
    ${CMAKE_SOURCE_DIR}/include/gba/gba_ptr.h
    ${CMAKE_SOURCE_DIR}/port/generated/gba_symbols.s
    ${CMAKE_SOURCE_DIR}/port/generated/layout_checks/color_fading.c
    ${CMAKE_SOURCE_DIR}/src/color_fading.c
    ${CMAKE_SOURCE_DIR}/src/minigames/roulette/update_roulette_intro.c)
foreach(_name hoggus aerodent cuckoo_condor)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/src/sprite_ai/${_name}.c)
endforeach()
foreach(_name bios_test rom game_thread main gba_memory gba_io ppu frame bios audio m4a_native save gba_functions)
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/port/src/${_name}.c)
endforeach()

set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS ${CMAKE_SOURCE_DIR}/port/platform/sdl/sdl_frontend.c)
