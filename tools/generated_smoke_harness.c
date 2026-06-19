#include "ngrecomp/neogeo_runtime.h"
#include "p_rom.h"

#include <stdio.h>
#include <string.h>

void ng_generated_call(uint32_t addr);

int ng_generated_smoke_run(const char *neo_path) {
    if (!neo_path || !*neo_path) {
        fprintf(stderr, "missing .neo path\n");
        return 2;
    }

    NgProgramRom rom;
    if (!ng_program_rom_load_neo(&rom, neo_path)) {
        fprintf(stderr, "failed to load neo image: %s\n", neo_path);
        return 1;
    }

    uint32_t cart_entry = 0;
    if (!ng_program_rom_cart_entry(&rom, &cart_entry)) {
        fprintf(stderr, "failed to locate cartridge entry in %s\n", neo_path);
        ng_program_rom_free(&rom);
        return 1;
    }

    ng_neogeo_reset_runtime();
    ng_neogeo_set_program_rom(rom.data, rom.size);
    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    g_ng_m68k.ssp = ng_program_rom_initial_ssp(&rom);
    g_ng_m68k.usp = 0;
    g_ng_m68k.a[7] = g_ng_m68k.ssp;
    g_ng_m68k.sr = 0x2700u;

    fprintf(stderr,
            "starting cart entry $%06X ssp=$%08X\n",
            cart_entry & 0x00FFFFFFu,
            g_ng_m68k.ssp);
    ng_generated_call(cart_entry);
    fprintf(stderr,
            "returned pc=$%06X sr=$%04X sp=$%08X\n",
            g_ng_m68k.pc & 0x00FFFFFFu,
            g_ng_m68k.sr,
            g_ng_m68k.a[7]);

    ng_neogeo_set_program_rom(NULL, 0);
    ng_program_rom_free(&rom);
    return 0;
}

#ifndef NG_GENERATED_SMOKE_NO_MAIN
int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <game.neo>\n", argv[0]);
        return 2;
    }
    return ng_generated_smoke_run(argv[1]);
}
#endif
