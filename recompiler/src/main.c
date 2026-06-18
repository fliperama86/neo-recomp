#include "c_emitter.h"
#include "function_discovery.h"
#include "m68k_analyze.h"
#include "m68k_decode.h"
#include "m68k_stub.h"
#include "neogeo_map.h"
#include "p_rom.h"

#include <stdio.h>
#include <string.h>

#define CLI_MAX_FUNCTION_PREVIEWS 4u

static void print_usage(const char *argv0) {
    fprintf(stderr,
            "Usage: %s --game <game.toml> (--p1 <program.rom> [--p2 <program.rom>] | --neo <game.neo>) [--emit-c <out.c>]\n",
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

static void print_decode_preview(const NgProgramRom *rom,
                                 const char *label,
                                 uint32_t start_addr,
                                 int max_instructions,
                                 int show_dispatch_tables) {
    printf("%s:\n", label);
    uint32_t pc = start_addr;
    NgM68kInstr previous;
    int have_previous = 0;
    for (int i = 0; i < max_instructions; ++i) {
        NgM68kInstr instr;
        char text[128];
        if (!ng_m68k_decode(rom, pc, &instr)) {
            printf("  $%06X: <unmapped>\n", pc & 0xFFFFFFu);
            break;
        }
        ng_m68k_format(&instr, text, (unsigned)sizeof(text));
        printf("  $%06X: %-24s ; %s\n",
               pc & 0xFFFFFFu, text, ng_m68k_mnemonic_name(instr.mnemonic));
        if (show_dispatch_tables && have_previous) {
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

static void print_function_candidates(const NgProgramRom *rom,
                                      const NgFunctionDiscovery *discovery) {
    printf("function candidates: %u%s\n",
           discovery->count,
           discovery->truncated ? " (truncated)" : "");
    for (uint32_t i = 0; i < discovery->count; ++i) {
        uint32_t addr = discovery->addrs[i];
        printf("  [%02u] $%08X (%s)%s\n",
               i,
               addr,
               ng_address_region_name(ng_address_region(addr)),
               i == 0 ? " entry" : "");
    }

    uint32_t previews = 0;
    for (uint32_t i = 1; i < discovery->count && previews < CLI_MAX_FUNCTION_PREVIEWS; ++i) {
        char label[64];
        snprintf(label, sizeof(label), "function preview $%06X",
                 discovery->addrs[i] & 0xFFFFFFu);
        print_decode_preview(rom, label, discovery->addrs[i], 6, 0);
        ++previews;
    }
}

static int emit_c_file(const char *path, const NgFunctionDiscovery *discovery) {
    FILE *out = fopen(path, "w");
    if (!out) {
        fprintf(stderr, "cannot open %s for writing\n", path);
        return 0;
    }

    int ok = ng_emit_c_skeleton(out, discovery);
    if (fclose(out) != 0) {
        ok = 0;
    }
    if (!ok) {
        fprintf(stderr, "failed to emit %s\n", path);
        return 0;
    }

    printf("generated C skeleton: %s\n", path);
    return 1;
}

int main(int argc, char **argv) {
    const char *game_path = NULL;
    const char *p1_path = NULL;
    const char *p2_path = NULL;
    const char *neo_path = NULL;
    const char *emit_c_path = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--game") == 0 && i + 1 < argc) {
            game_path = argv[++i];
        } else if (strcmp(argv[i], "--p1") == 0 && i + 1 < argc) {
            p1_path = argv[++i];
        } else if (strcmp(argv[i], "--p2") == 0 && i + 1 < argc) {
            p2_path = argv[++i];
        } else if (strcmp(argv[i], "--neo") == 0 && i + 1 < argc) {
            neo_path = argv[++i];
        } else if (strcmp(argv[i], "--emit-c") == 0 && i + 1 < argc) {
            emit_c_path = argv[++i];
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
                print_decode_preview(&rom, "entry preview", cart_entry, 16, 1);

                NgFunctionDiscovery discovery;
                if (ng_function_discover_from_entry(&rom, cart_entry, &discovery)) {
                    print_function_candidates(&rom, &discovery);
                    if (emit_c_path && !emit_c_file(emit_c_path, &discovery)) {
                        ng_program_rom_free(&rom);
                        return 1;
                    }
                }
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
