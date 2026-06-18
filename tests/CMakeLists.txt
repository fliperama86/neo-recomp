add_executable(test_p_rom
    test_p_rom.c
)
target_link_libraries(test_p_rom PRIVATE neo_recompiler_core)

add_executable(test_m68k_decode
    test_m68k_decode.c
)
target_link_libraries(test_m68k_decode PRIVATE neo_recompiler_core)

add_executable(test_function_discovery
    test_function_discovery.c
)
target_link_libraries(test_function_discovery PRIVATE neo_recompiler_core)

if(MSVC)
    target_compile_options(test_p_rom PRIVATE /W4)
    target_compile_options(test_m68k_decode PRIVATE /W4)
    target_compile_options(test_function_discovery PRIVATE /W4)
else()
    target_compile_options(test_p_rom PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_m68k_decode PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_function_discovery PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME test_p_rom COMMAND test_p_rom)
add_test(NAME test_m68k_decode COMMAND test_m68k_decode)
add_test(NAME test_function_discovery COMMAND test_function_discovery)
