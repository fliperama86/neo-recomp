cmake_minimum_required(VERSION 3.20)

project(neo_recomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

enable_testing()

add_library(neo_runtime
    runtime/src/neogeo_runtime.c
)
target_include_directories(neo_runtime PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_library(neo_video
    runtime/src/neogeo_video.c
)
target_include_directories(neo_video PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_library(neo_recompiler_core
    recompiler/src/c_emitter.c
    recompiler/src/dispatch_audit.c
    recompiler/src/function_discovery.c
    recompiler/src/game_config.c
    recompiler/src/m68k_analyze.c
    recompiler/src/m68k_cycles.c
    recompiler/src/m68k_decode.c
    recompiler/src/m68k_validate.c
    recompiler/src/m68k_stub.c
    recompiler/src/neogeo_map.c
    recompiler/src/p_rom.c
)
target_include_directories(neo_recompiler_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/recompiler/src"
)

add_executable(neo-recomp
    recompiler/src/main.c
)
target_include_directories(neo-recomp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/recompiler/src"
)
target_link_libraries(neo-recomp PRIVATE neo_recompiler_core)

add_executable(generate-bios-recomp
    tools/generate_bios_recomp.c
)
target_include_directories(generate-bios-recomp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/recompiler/src"
)
target_link_libraries(generate-bios-recomp PRIVATE neo_recompiler_core)

add_executable(neo-render-snapshot
    tools/render_snapshot_frame.c
)
target_include_directories(neo-render-snapshot PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/recompiler/src"
)
target_link_libraries(neo-render-snapshot PRIVATE
    neo_video
    neo_recompiler_core
)

find_package(PkgConfig QUIET)
if(PkgConfig_FOUND)
    pkg_check_modules(SDL2 QUIET IMPORTED_TARGET sdl2)
endif()
if(SDL2_FOUND)
    add_executable(neo-snapshot-viewer
        tools/sdl_snapshot_viewer.c
    )
    target_include_directories(neo-snapshot-viewer PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/recompiler/src"
    )
    target_link_libraries(neo-snapshot-viewer PRIVATE
        PkgConfig::SDL2
        neo_video
        neo_recompiler_core
    )
else()
    message(STATUS "SDL2 not found via pkg-config; skipping optional neo-snapshot-viewer target")
endif()

if(MSVC)
    target_compile_options(neo_runtime PRIVATE /W4)
    target_compile_options(neo_video PRIVATE /W4)
    target_compile_options(neo_recompiler_core PRIVATE /W4)
    target_compile_options(neo-recomp PRIVATE /W4)
    target_compile_options(generate-bios-recomp PRIVATE /W4)
    target_compile_options(neo-render-snapshot PRIVATE /W4)
    if(TARGET neo-snapshot-viewer)
        target_compile_options(neo-snapshot-viewer PRIVATE /W4)
    endif()
else()
    target_compile_options(neo_runtime PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(neo_video PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(neo_recompiler_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(neo-recomp PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(generate-bios-recomp PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(neo-render-snapshot PRIVATE -Wall -Wextra -Wpedantic)
    if(TARGET neo-snapshot-viewer)
        target_compile_options(neo-snapshot-viewer PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endif()

add_subdirectory(tests)
