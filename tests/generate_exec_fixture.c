#include "c_emitter.h"
#include "exec_fixture.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static NgProgramRom make_rom(uint32_t size) {
    NgProgramRom rom;
    memset(&rom, 0, sizeof(rom));
    rom.size = size;
    rom.data = (uint8_t *)calloc(size ? size : 1u, 1);
    return rom;
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <out.c>\n", argv[0]);
        return 2;
    }

    NgProgramRom rom = make_rom(NG_EXEC_FIXTURE_SIZE);
    if (!rom.data) {
        return 1;
    }
    ng_exec_fixture_fill(rom.data, rom.size);

    NgFunctionDiscovery discovery;
    ng_function_discovery_init(&discovery);
    for (uint32_t i = 0; i < NG_EXEC_FIXTURE_ADDR_COUNT; ++i) {
        if (!ng_function_discovery_add(&discovery, &rom, ng_exec_fixture_addr(i))) {
            ng_program_rom_free(&rom);
            return 1;
        }
    }

    FILE *out = fopen(argv[1], "w");
    if (!out) {
        ng_program_rom_free(&rom);
        return 1;
    }

    int ok = ng_emit_c(out, &rom, &discovery);
    if (fclose(out) != 0) {
        ok = 0;
    }
    ng_program_rom_free(&rom);
    return ok ? 0 : 1;
}
