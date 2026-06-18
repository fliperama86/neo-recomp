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

add_executable(test_c_emitter
    test_c_emitter.c
)
target_link_libraries(test_c_emitter PRIVATE neo_recompiler_core)

add_executable(generate_exec_fixture
    generate_exec_fixture.c
)
target_link_libraries(generate_exec_fixture PRIVATE neo_recompiler_core)

set(GENERATED_EXEC_C "${CMAKE_CURRENT_BINARY_DIR}/generated_exec_fixture.c")
add_custom_command(
    OUTPUT "${GENERATED_EXEC_C}"
    COMMAND "$<TARGET_FILE:generate_exec_fixture>" "${GENERATED_EXEC_C}"
    DEPENDS generate_exec_fixture
    VERBATIM
)

add_executable(test_generated_exec
    test_generated_exec.c
    "${GENERATED_EXEC_C}"
)
target_include_directories(test_generated_exec PRIVATE
    "${CMAKE_SOURCE_DIR}/include"
)

if(MSVC)
    target_compile_options(test_p_rom PRIVATE /W4)
    target_compile_options(test_m68k_decode PRIVATE /W4)
    target_compile_options(test_function_discovery PRIVATE /W4)
    target_compile_options(test_c_emitter PRIVATE /W4)
    target_compile_options(generate_exec_fixture PRIVATE /W4)
    target_compile_options(test_generated_exec PRIVATE /W4)
else()
    target_compile_options(test_p_rom PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_m68k_decode PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_function_discovery PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_c_emitter PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(generate_exec_fixture PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_generated_exec PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME test_p_rom COMMAND test_p_rom)
add_test(NAME test_m68k_decode COMMAND test_m68k_decode)
add_test(NAME test_function_discovery COMMAND test_function_discovery)
add_test(NAME test_c_emitter COMMAND test_c_emitter)
add_test(NAME test_generated_exec COMMAND test_generated_exec)
