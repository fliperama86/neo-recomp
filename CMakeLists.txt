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

add_library(neo_recompiler_core
    recompiler/src/c_emitter.c
    recompiler/src/dispatch_audit.c
    recompiler/src/function_discovery.c
    recompiler/src/game_config.c
    recompiler/src/m68k_analyze.c
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
target_link_libraries(neo-recomp PRIVATE neo_recompiler_core neo_runtime)

if(MSVC)
    target_compile_options(neo_runtime PRIVATE /W4)
    target_compile_options(neo_recompiler_core PRIVATE /W4)
    target_compile_options(neo-recomp PRIVATE /W4)
else()
    target_compile_options(neo_runtime PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(neo_recompiler_core PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(neo-recomp PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_subdirectory(tests)
