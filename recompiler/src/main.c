#include "m68k_stub.h"
#include "p_rom.h"

#include <stdio.h>
#include <string.h>

static void print_usage(const char *argv0) {
    fprintf(stderr,
            "Usage: %s --game <game.toml> --p1 <program.rom> [--p2 <program.rom>]\n",
            argv0);
}

int main(int argc, char **argv) {
    const char *game_path = NULL;
    const char *p1_path = NULL;
    const char *p2_path = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--game") == 0 && i + 1 < argc) {
            game_path = argv[++i];
        } else if (strcmp(argv[i], "--p1") == 0 && i + 1 < argc) {
            p1_path = argv[++i];
        } else if (strcmp(argv[i], "--p2") == 0 && i + 1 < argc) {
            p2_path = argv[++i];
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }

    if (!game_path || !p1_path) {
        print_usage(argv[0]);
        return 2;
    }

    NgProgramRom rom = {0};
    if (!ng_program_rom_load(&rom, p1_path, p2_path)) {
        ng_program_rom_free(&rom);
        return 1;
    }

    printf("game config: %s\n", game_path);
    printf("program image: %u bytes\n", rom.size);
    if (rom.size >= 8) {
        uint32_t initial_ssp = ng_program_rom_initial_ssp(&rom);
        uint32_t initial_pc = ng_program_rom_initial_pc(&rom);
        printf("vector initial_ssp=$%08X initial_pc=$%08X\n",
               initial_ssp, initial_pc);
        if (!ng_program_rom_initial_pc_is_mapped(&rom)) {
            printf("warning: initial PC is outside the loaded program image\n");
        }
    }
    m68k_stub_print_scope();

    ng_program_rom_free(&rom);
    return 0;
}
