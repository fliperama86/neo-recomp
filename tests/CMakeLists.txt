add_executable(test_p_rom
    test_p_rom.c
)
target_link_libraries(test_p_rom PRIVATE neo_recompiler_core)

if(MSVC)
    target_compile_options(test_p_rom PRIVATE /W4)
else()
    target_compile_options(test_p_rom PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME test_p_rom COMMAND test_p_rom)
