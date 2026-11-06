# PC port build. The GBA ROM is still built with make; see port/README.md.
cmake_minimum_required(VERSION 3.20)
project(warioland4_pc C ASM)

option(WL4_CAST_WARNINGS "Show the pointer/integer cast warnings (the GbaPtr worklist)" OFF)
option(WL4_ASAN "Build with AddressSanitizer" OFF)

if(APPLE)
    if(NOT CMAKE_SYSTEM_PROCESSOR STREQUAL "arm64")
        message(FATAL_ERROR "The experimental Darwin backend currently requires arm64")
    endif()
    include(port/macos/CMakeLists.txt)
endif()

if(WL4_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()

# Game code: everything in src/ except the ROM data in src/data/, which the port reads from
# the user's ROM. The function pointer tables there hold native addresses, so they are built.
file(GLOB_RECURSE WL4_GAME_SOURCES CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/src/*.c)
list(FILTER WL4_GAME_SOURCES EXCLUDE REGEX "/src/data/")
# macOS creates AppleDouble metadata files on exFAT volumes.
list(FILTER WL4_GAME_SOURCES EXCLUDE REGEX "/\\._")
list(APPEND WL4_GAME_SOURCES
    ${PROJECT_SOURCE_DIR}/src/data/color_fading_pointers.c
    ${PROJECT_SOURCE_DIR}/src/data/stage_end_pointers.c
    ${PROJECT_SOURCE_DIR}/src/data/tile_fn_pointers.c
)

file(GLOB WL4_LAYOUT_ASSERTS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/port/generated/layout_asserts_*.c)
list(FILTER WL4_LAYOUT_ASSERTS EXCLUDE REGEX "/\\._")

# How game code is compiled, shared by the game library and the layout checks.
add_library(wl4_game_flags INTERFACE)
target_include_directories(wl4_game_flags INTERFACE include port/include)
target_compile_definitions(wl4_game_flags INTERFACE PORTABLE VERSION_US)
target_compile_options(wl4_game_flags INTERFACE
    -std=gnu89
    # agbcc's semantics: unsigned char, no type-based aliasing, wrapping signed overflow,
    # and null pointers that may be dereferenced (GBA address 0 is the BIOS).
    -funsigned-char -fno-strict-aliasing -fwrapv -fno-delete-null-pointer-checks
    # GCC bitfield layout, as agbcc uses, instead of the Microsoft layout mingw defaults to.
    $<$<STREQUAL:${CMAKE_SYSTEM_PROCESSOR},x86_64>:-mno-ms-bitfields>
    # The game's data sits at its GBA addresses, which are aligned to 4 bytes at most. Clang
    # takes every global array of 16 bytes or more to be 16-byte aligned, and copies them
    # with aligned SSE loads, which fault. No SSE in game code, then (it has no float maths
    # beyond a few casts, which x87 does).
    $<$<STREQUAL:${CMAKE_SYSTEM_PROCESSOR},x86_64>:-mno-sse>
    # Game code starts DMA by writing the registers, often to copy a local variable it has just
    # set: `fill = 0; REG_DMA3SAD = (u32)&fill; ... REG_DMA3CNT = ...;`. Clang would drop or
    # delay the store to the local, which only the transfer reads. Acquire/release semantics
    # for volatile accesses keep every store before the register write that starts a DMA.
    -fms-volatile
    -Werror=return-type -Werror=implicit-function-declaration
    # Pointer type mismatches and int/pointer conversions are fixed by the GbaPtr conversion.
    -Wno-error=incompatible-pointer-types -Wno-error=incompatible-function-pointer-types -Wno-error=int-conversion
)
if(NOT WL4_CAST_WARNINGS)
    target_compile_options(wl4_game_flags INTERFACE
        -Wno-int-to-pointer-cast -Wno-pointer-to-int-cast -Wno-void-pointer-to-int-cast -Wno-int-to-void-pointer-cast)
endif()

if(APPLE)
    list(REMOVE_ITEM WL4_GAME_SOURCES ${PROJECT_SOURCE_DIR}/src/color_fading.c)
    list(APPEND WL4_GAME_SOURCES ${WL4_DARWIN_GENERATED}/color_fading.c)
    list(REMOVE_ITEM WL4_GAME_SOURCES ${PROJECT_SOURCE_DIR}/src/minigames/roulette/update_roulette_intro.c)
    list(APPEND WL4_GAME_SOURCES ${WL4_DARWIN_GENERATED}/update_roulette_intro.c)
    foreach(_name hoggus aerodent cuckoo_condor)
        list(REMOVE_ITEM WL4_GAME_SOURCES ${PROJECT_SOURCE_DIR}/src/sprite_ai/${_name}.c)
        list(APPEND WL4_GAME_SOURCES ${WL4_DARWIN_GENERATED}/${_name}.c)
    endforeach()
endif()

add_library(wl4_game STATIC ${WL4_GAME_SOURCES} ${WL4_LAYOUT_ASSERTS})
target_link_libraries(wl4_game PRIVATE wl4_game_flags)

# Layout checks for the structs that game sources define themselves. Each one includes its
# source file, so these objects are compiled but never linked.
file(GLOB WL4_LAYOUT_CHECKS CONFIGURE_DEPENDS ${PROJECT_SOURCE_DIR}/port/generated/layout_checks/*.c)
list(FILTER WL4_LAYOUT_CHECKS EXCLUDE REGEX "/\\._")
if(APPLE)
    list(REMOVE_ITEM WL4_LAYOUT_CHECKS ${PROJECT_SOURCE_DIR}/port/generated/layout_checks/color_fading.c)
    list(APPEND WL4_LAYOUT_CHECKS ${WL4_DARWIN_GENERATED}/layout_color_fading.c)
endif()
add_library(wl4_layout_checks OBJECT ${WL4_LAYOUT_CHECKS})
target_link_libraries(wl4_layout_checks PRIVATE wl4_game_flags)
# The game library already reports these sources' warnings.
target_compile_options(wl4_layout_checks PRIVATE -w)

add_library(wl4_platform_stub STATIC port/platform/stub/platform_stub.c)

# Native replacements for game asm that need the game's headers. An object library, so that
# its definitions are always linked and replace the weak stubs in port/generated/gba_stubs.c.
add_library(wl4_game_native OBJECT port/src/m4a_native.c port/src/rumble_snapshot.c port/src/tile_wrap_test.c)
target_link_libraries(wl4_game_native PRIVATE wl4_game_flags)

add_executable(wl4-rumble-event-test port/src/rumble_event_test.c port/src/rumble_snapshot.c)
target_link_libraries(wl4-rumble-event-test PRIVATE wl4_game_flags)

# The port runtime and the executable. port/generated/gba_*.{c,s} come from
# port/tools/gen_symbols.py: GBA addresses for the game's data, the function dispatch table,
# and stubs for the functions that are still asm.
add_executable(wl4
    port/src/main.c
    port/src/audio.c
    port/src/game_thread.c
    port/src/bios.c
    port/src/bios_test.c
    port/src/frame.c
    port/src/gba_functions.c
    port/src/gba_io.c
    port/src/gba_memory.c
    port/src/gba_runtime.c
    port/src/input_script.c
    port/src/io_test.c
    port/src/md5.c
    port/src/os_win32.c
    port/src/ppu.c
    port/src/rom_validate.c
    port/src/rom.c
    port/src/save.c
    port/src/save_test.c
    port/src/x86_access.c
    port/generated/gba_symbols.s
    port/generated/gba_functions.c
    port/generated/gba_stubs.c
    port/generated/lz77_sources.c
    $<TARGET_OBJECTS:wl4_game_native>
)
target_include_directories(wl4 PRIVATE port/include port/third_party/minicoro)

# The SDL3 frontend (a window, input and pacing). Without SDL3, wl4 only runs headless. Point
# SDL3_DIR at the SDL3 development package's lib/cmake/SDL3 directory, e.g. for the mingw
# release: SDL3-<version>/x86_64-w64-mingw32/lib/cmake/SDL3.
find_package(SDL3 CONFIG)
if(SDL3_FOUND)
    add_library(wl4_settings STATIC port/src/settings.c port/src/controls.c)
    add_library(wl4_crt STATIC port/platform/sdl/crt.c)
    target_include_directories(wl4_crt PUBLIC port/include)
    target_link_libraries(wl4_crt PUBLIC SDL3::SDL3)
    target_compile_options(wl4_crt PRIVATE -Wall -Wextra)
    target_include_directories(wl4_settings PUBLIC port/include)
    target_link_libraries(wl4_settings PUBLIC SDL3::SDL3)
    target_compile_options(wl4_settings PRIVATE -Wall -Wextra)
    target_sources(wl4 PRIVATE port/platform/sdl/sdl_frontend.c)
    target_link_libraries(wl4 PRIVATE wl4_settings wl4_crt)
    add_executable(wl4-crt-test port/src/crt_test.c)
    target_link_libraries(wl4-crt-test PRIVATE wl4_crt)
    add_executable(wl4-input-test port/src/settings_test.c)
    target_link_libraries(wl4-input-test PRIVATE wl4_settings)
    target_compile_definitions(wl4 PRIVATE $<$<COMPILE_LANGUAGE:C>:WL4_HAVE_SDL>)
    target_link_libraries(wl4 PRIVATE SDL3::SDL3)
    if(WIN32)
        add_custom_command(TARGET wl4 POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different $<TARGET_FILE:SDL3::SDL3> $<TARGET_FILE_DIR:wl4>)
    endif()
else()
    message(STATUS "SDL3 not found: wl4 is built without its window (set SDL3_DIR)")
endif()
# gba_functions.c declares every game function as void(void) to take its address.
set_source_files_properties(port/generated/gba_functions.c PROPERTIES COMPILE_OPTIONS -Wno-incompatible-library-redeclaration)
target_link_libraries(wl4 PRIVATE wl4_game wl4_platform_stub)

# A separate Windows GUI; no ROM assets or game library are linked into it.
if(WIN32 AND SDL3_FOUND)
    enable_language(RC)
    add_executable(wl4-launcher WIN32 port/src/launcher_win32.c port/src/rom_validate.c port/src/md5.c port/platform/sdl/settings_window.c port/launcher.rc)
    target_include_directories(wl4-launcher PRIVATE port/include)
    target_compile_definitions(wl4-launcher PRIVATE UNICODE _UNICODE)
    target_compile_options(wl4-launcher PRIVATE -Wall -Wextra)
    target_link_options(wl4-launcher PRIVATE -municode)
    target_link_libraries(wl4-launcher PRIVATE wl4_settings wl4_crt comdlg32 advapi32 gdi32 dwmapi)
    add_dependencies(wl4-launcher wl4)
endif()

if(APPLE)
    target_compile_options(wl4 PRIVATE -fno-delete-null-pointer-checks)
    # Replace platform sources with build-only derivatives and Darwin implementations.
    get_target_property(_wl4_sources wl4 SOURCES)
    foreach(_name bios_test rom game_thread main gba_memory gba_io ppu frame bios audio save gba_functions)
        list(REMOVE_ITEM _wl4_sources port/src/${_name}.c)
        list(APPEND _wl4_sources ${WL4_DARWIN_GENERATED}/${_name}.c)
    endforeach()
    list(REMOVE_ITEM _wl4_sources port/platform/sdl/sdl_frontend.c port/src/os_win32.c port/src/save_test.c port/src/io_test.c port/src/x86_access.c port/generated/gba_symbols.s)
    list(APPEND _wl4_sources port/macos/audio_output.c ${WL4_DARWIN_GENERATED}/sdl_frontend.c port/macos/pad_route.c port/macos/os_darwin.c port/macos/os_file_posix.c port/macos/memory_libc.c port/macos/arm64_access.c port/macos/save_test_posix.c port/macos/io_test.c ${WL4_DARWIN_GENERATED}/gba_symbols.s)
    set_property(TARGET wl4 PROPERTY SOURCES ${_wl4_sources})
    # Game-native functions use the same pointer-access overlay.
    set_property(TARGET wl4_game_native PROPERTY SOURCES ${WL4_DARWIN_GENERATED}/m4a_native.c port/src/rumble_snapshot.c port/src/tile_wrap_test.c port/macos/trace.c)
    if(SDL3_FOUND)
        add_executable(wl4-audio-output-test port/macos/audio_output_test.c port/macos/audio_output.c port/macos/memory_libc.c)
        target_include_directories(wl4-audio-output-test PRIVATE port/macos)
        target_link_libraries(wl4-audio-output-test PRIVATE SDL3::SDL3)
        add_executable(wl4-controller-recovery-test port/macos/controller_recovery_test.c)
        target_link_libraries(wl4-controller-recovery-test PRIVATE wl4_settings)
        target_link_libraries(wl4_game_native PRIVATE SDL3::SDL3)
        set_property(TARGET wl4_settings PROPERTY SOURCES port/macos/settings.c port/macos/controls.c)
        target_sources(wl4_settings PRIVATE port/macos/os_file_posix.c port/macos/memory_libc.c)
        target_sources(wl4_crt PRIVATE port/macos/memory_libc.c)
    endif()
    target_sources(wl4-rumble-event-test PRIVATE port/macos/memory_libc.c)
endif()
