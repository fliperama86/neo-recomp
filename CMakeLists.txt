cmake_minimum_required(VERSION 3.20)

project(neo_recomp C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

add_library(neo_runtime
    runtime/src/neogeo_runtime.c
)
target_include_directories(neo_runtime PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

add_executable(neo-recomp
    recompiler/src/main.c
    recompiler/src/m68k_stub.c
    recompiler/src/p_rom.c
)
target_include_directories(neo-recomp PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/recompiler/src"
)
target_link_libraries(neo-recomp PRIVATE neo_runtime)

if(MSVC)
    target_compile_options(neo_runtime PRIVATE /W4)
    target_compile_options(neo-recomp PRIVATE /W4)
else()
    target_compile_options(neo_runtime PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(neo-recomp PRIVATE -Wall -Wextra -Wpedantic)
endif()

