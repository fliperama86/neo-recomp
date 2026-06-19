add_executable(test_p_rom
    test_p_rom.c
)
target_link_libraries(test_p_rom PRIVATE neo_recompiler_core)

add_executable(test_m68k_decode
    test_m68k_decode.c
)
target_link_libraries(test_m68k_decode PRIVATE neo_recompiler_core)

add_executable(test_m68k_validate
    test_m68k_validate.c
)
target_link_libraries(test_m68k_validate PRIVATE neo_recompiler_core)

add_executable(test_function_discovery
    test_function_discovery.c
)
target_link_libraries(test_function_discovery PRIVATE neo_recompiler_core)

add_executable(test_dispatch_audit
    test_dispatch_audit.c
)
target_link_libraries(test_dispatch_audit PRIVATE neo_recompiler_core)

add_executable(test_game_config
    test_game_config.c
)
target_link_libraries(test_game_config PRIVATE neo_recompiler_core)

add_executable(test_c_emitter
    test_c_emitter.c
)
target_link_libraries(test_c_emitter PRIVATE neo_recompiler_core)

add_executable(test_runtime_interrupts
    test_runtime_interrupts.c
)
target_link_libraries(test_runtime_interrupts PRIVATE neo_runtime)

add_executable(test_generated_smoke_harness
    test_generated_smoke_harness.c
    ../tools/generated_smoke_harness.c
)
target_compile_definitions(test_generated_smoke_harness PRIVATE
    NG_GENERATED_SMOKE_NO_MAIN
)
target_link_libraries(test_generated_smoke_harness PRIVATE
    neo_recompiler_core
    neo_runtime
)

add_executable(test_generated_smoke_trampoline
    test_generated_smoke_trampoline.c
    ../tools/generated_smoke_harness.c
)
target_compile_definitions(test_generated_smoke_trampoline PRIVATE
    NG_GENERATED_SMOKE_NO_MAIN
    NG_GENERATED_SMOKE_HAS_BIOS
    NG_GENERATED_SMOKE_COMBINED_DISPATCH
)
target_link_libraries(test_generated_smoke_trampoline PRIVATE
    neo_recompiler_core
    neo_runtime
)

add_executable(test_opcode_sweep
    test_opcode_sweep.c
)
target_link_libraries(test_opcode_sweep PRIVATE neo_recompiler_core)

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
    target_compile_options(test_m68k_validate PRIVATE /W4)
    target_compile_options(test_function_discovery PRIVATE /W4)
    target_compile_options(test_dispatch_audit PRIVATE /W4)
    target_compile_options(test_game_config PRIVATE /W4)
    target_compile_options(test_c_emitter PRIVATE /W4)
    target_compile_options(test_runtime_interrupts PRIVATE /W4)
    target_compile_options(test_generated_smoke_harness PRIVATE /W4)
    target_compile_options(test_generated_smoke_trampoline PRIVATE /W4)
    target_compile_options(test_opcode_sweep PRIVATE /W4)
    target_compile_options(generate_exec_fixture PRIVATE /W4)
    target_compile_options(test_generated_exec PRIVATE /W4)
else()
    target_compile_options(test_p_rom PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_m68k_decode PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_m68k_validate PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_function_discovery PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_dispatch_audit PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_game_config PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_c_emitter PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_runtime_interrupts PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_generated_smoke_harness PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_generated_smoke_trampoline PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_opcode_sweep PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(generate_exec_fixture PRIVATE -Wall -Wextra -Wpedantic)
    target_compile_options(test_generated_exec PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME test_p_rom COMMAND test_p_rom)
add_test(NAME test_m68k_decode COMMAND test_m68k_decode)
add_test(NAME test_m68k_validate COMMAND test_m68k_validate)
add_test(NAME test_function_discovery COMMAND test_function_discovery)
add_test(NAME test_dispatch_audit COMMAND test_dispatch_audit)
add_test(NAME test_game_config COMMAND test_game_config)
add_test(NAME test_c_emitter COMMAND test_c_emitter)
add_test(NAME test_runtime_interrupts COMMAND test_runtime_interrupts)
add_test(NAME test_generated_smoke_harness COMMAND test_generated_smoke_harness)
add_test(NAME test_generated_smoke_trampoline COMMAND test_generated_smoke_trampoline)
add_test(NAME test_opcode_sweep COMMAND test_opcode_sweep)
add_test(NAME test_generated_exec COMMAND test_generated_exec)

find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
    add_test(
        NAME test_snapshot_debug
        COMMAND "${Python3_EXECUTABLE}"
                "${CMAKE_CURRENT_SOURCE_DIR}/test_snapshot_debug.py"
                "${CMAKE_SOURCE_DIR}"
                "${CMAKE_CURRENT_BINARY_DIR}"
    )
endif()
