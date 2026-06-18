#include "m68k_analyze.h"
#include "m68k_decode.h"
#include "m68k_stub.h"
#include "neogeo_map.h"
#include "p_rom.h"

#include <stdio.h>
#include <string.h>

static void print_usage(const char *argv0) {
    fprintf(stderr,
            "Usage: %s --game <game.toml> (--p1 <program.rom> [--p2 <program.rom>] | --neo <game.neo>)\n",
            argv0);
}

static void print_vector(const NgProgramRom *rom, int index) {
    uint32_t value = ng_program_rom_read32(rom, (uint32_t)index * 4u);
    NgAddressRegion region = ng_address_region(value);
    printf("vector[%02d]=$%08X (%s)\n",
           index, value, ng_address_region_name(region));
}

static void print_dispatch_table(const NgProgramRom *rom,
                                 const NgM68kJumpTablePattern *pattern) {
    printf("  dispatch table: base=$%06X index=D%u.W target=A%u entry_size=%u\n",
           pattern->table_addr & 0xFFFFFFu,
           pattern->index_reg,
           pattern->target_reg,
           pattern->entry_size);

    for (uint8_t i = 0; i < 4; ++i) {
        uint32_t entry_addr = pattern->table_addr + (uint32_t)i * pattern->entry_size;
        if (!ng_program_rom_addr_is_mapped(rom, entry_addr) ||
            !ng_program_rom_addr_is_mapped(rom, entry_addr + 3u)) {
            printf("    [%u] <unmapped>\n", i);
            break;
        }

        uint32_t target = ng_program_rom_read32(rom, entry_addr);
        printf("    [%u] $%08X (%s)\n",
               i, target, ng_address_region_name(ng_address_region(target)));
    }
}

static void print_decode_preview(const NgProgramRom *rom, uint32_t start_addr) {
    printf("entry preview:\n");
    uint32_t pc = start_addr;
    NgM68kInstr previous;
    int have_previous = 0;
    for (int i = 0; i < 16; ++i) {
        NgM68kInstr instr;
        char text[128];
        if (!ng_m68k_decode(rom, pc, &instr)) {
            printf("  $%06X: <unmapped>\n", pc & 0xFFFFFFu);
            break;
        }
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        printf("  $%06X: %-24s ; %s\n",
               pc & 0xFFFFFFu, text, ng_m68k_mnemonic_name(instr.mnemonic));
        if (have_previous) {
            NgM68kJumpTablePattern pattern;
            if (ng_m68k_match_pc_index_jump_table(&previous, &instr, &pattern)) {
                print_dispatch_table(rom, &pattern);
            }
        }
        if (instr.byte_length == 0) {
            break;
        }
        pc += instr.byte_length;
        if (instr.mnemonic == NG_M68K_JMP || instr.mnemonic == NG_M68K_RTS) {
            break;
        }
        previous = instr;
        have_previous = 1;
    }
}

int main(int argc, char **argv) {
    const char *game_path = NULL;
    const char *p1_path = NULL;
    const char *p2_path = NULL;
    const char *neo_path = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--game") == 0 && i + 1 < argc) {
            game_path = argv[++i];
        } else if (strcmp(argv[i], "--p1") == 0 && i + 1 < argc) {
            p1_path = argv[++i];
        } else if (strcmp(argv[i], "--p2") == 0 && i + 1 < argc) {
            p2_path = argv[++i];
        } else if (strcmp(argv[i], "--neo") == 0 && i + 1 < argc) {
            neo_path = argv[++i];
        } else {
            print_usage(argv[0]);
            return 2;
        }
    }

    if (!game_path || (!p1_path && !neo_path) || (p1_path && neo_path)) {
        print_usage(argv[0]);
        return 2;
    }

    NgProgramRom rom = {0};
    int loaded = neo_path ? ng_program_rom_load_neo(&rom, neo_path)
                          : ng_program_rom_load(&rom, p1_path, p2_path);
    if (!loaded) {
        ng_program_rom_free(&rom);
        return 1;
    }

    printf("game config: %s\n", game_path);
    printf("program image: %u bytes\n", rom.size);
    if (rom.size >= 8) {
        uint32_t initial_ssp = ng_program_rom_initial_ssp(&rom);
        uint32_t initial_pc = ng_program_rom_initial_pc(&rom);
        printf("vector initial_ssp=$%08X (%s) initial_pc=$%08X (%s)\n",
               initial_ssp, ng_address_region_name(ng_address_region(initial_ssp)),
               initial_pc, ng_address_region_name(ng_address_region(initial_pc)));
        for (int i = 0; i < 16; ++i) {
            print_vector(&rom, i);
        }

        uint32_t cart_entry = 0;
        if (ng_program_rom_cart_entry(&rom, &cart_entry)) {
            printf("cartridge header: NEO-GEO, entry=$%08X (%s)\n",
                   cart_entry, ng_address_region_name(ng_address_region(cart_entry)));
            if (ng_program_rom_addr_is_mapped(&rom, cart_entry)) {
                print_decode_preview(&rom, cart_entry);
            } else {
                printf("entry preview: entry is outside loaded P-ROM image\n");
            }
        } else {
            printf("cartridge header: not found or unsupported\n");
        }
    }
    m68k_stub_print_scope();

    ng_program_rom_free(&rom);
    return 0;
}
