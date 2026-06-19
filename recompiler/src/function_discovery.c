#include "function_discovery.h"

#include "m68k_analyze.h"
#include "m68k_decode.h"

#include <string.h>

void ng_function_discovery_init(NgFunctionDiscovery *discovery) {
    if (discovery) {
        memset(discovery, 0, sizeof(*discovery));
    }
}

int ng_function_discovery_contains(const NgFunctionDiscovery *discovery,
                                   uint32_t addr) {
    if (!discovery) {
        return 0;
    }
    for (uint32_t i = 0; i < discovery->count; ++i) {
        if (discovery->addrs[i] == addr) {
            return 1;
        }
    }
    return 0;
}

int ng_function_discovery_add(NgFunctionDiscovery *discovery,
                              const NgProgramRom *rom,
                              uint32_t addr) {
    if (!discovery || !rom || !ng_program_rom_addr_is_mapped(rom, addr)) {
        return 0;
    }
    if (ng_function_discovery_contains(discovery, addr)) {
        return 1;
    }
    if (discovery->count >= NG_FUNCTION_DISCOVERY_MAX_CANDIDATES) {
        discovery->truncated = 1;
        return 0;
    }

    discovery->addrs[discovery->count++] = addr;
    return 1;
}

static void add_jump_table_targets(const NgProgramRom *rom,
                                   const NgM68kJumpTablePattern *pattern,
                                   NgFunctionDiscovery *out) {
    for (uint32_t i = 0; i < NG_FUNCTION_DISCOVERY_TABLE_ENTRIES; ++i) {
        uint32_t entry_addr = pattern->table_addr + i * pattern->entry_size;
        if (!ng_program_rom_addr_is_mapped(rom, entry_addr) ||
            !ng_program_rom_addr_is_mapped(rom, entry_addr + 3u)) {
            break;
        }

        ng_function_discovery_add(out, rom, ng_program_rom_read32(rom, entry_addr));
    }
}

static int is_direct_function_target(const NgM68kInstr *instr) {
    if (instr->mnemonic == NG_M68K_BSR) {
        return 1;
    }
    if (instr->mnemonic != NG_M68K_JSR && instr->mnemonic != NG_M68K_JMP) {
        return 0;
    }
    return instr->src.mode == NG_M68K_EA_ABS_W ||
           instr->src.mode == NG_M68K_EA_ABS_L ||
           instr->src.mode == NG_M68K_EA_PC_DISP ||
           instr->src.mode == NG_M68K_EA_PC_INDEX;
}

static void scan_function_candidate(const NgProgramRom *rom,
                                    uint32_t start_addr,
                                    NgFunctionDiscovery *out) {
    uint32_t pc = start_addr;
    NgM68kInstr previous;
    int have_previous = 0;

    for (uint32_t i = 0; i < NG_FUNCTION_DISCOVERY_MAX_INSTRUCTIONS; ++i) {
        NgM68kInstr instr;
        if (!ng_m68k_decode(rom, pc, &instr)) {
            return;
        }

        if (have_previous) {
            NgM68kJumpTablePattern pattern;
            if (ng_m68k_match_pc_index_jump_table(&previous, &instr, &pattern)) {
                add_jump_table_targets(rom, &pattern, out);
            }
        }

        if (is_direct_function_target(&instr)) {
            ng_function_discovery_add(out, rom, instr.target);
        }
        if ((instr.mnemonic == NG_M68K_JSR || instr.mnemonic == NG_M68K_BSR) &&
            instr.byte_length != 0) {
            ng_function_discovery_add(out, rom, pc + instr.byte_length);
        }

        if (instr.byte_length == 0 ||
            instr.mnemonic == NG_M68K_JMP ||
            instr.mnemonic == NG_M68K_RTS) {
            return;
        }

        pc += instr.byte_length;
        previous = instr;
        have_previous = 1;
    }
}

int ng_function_discover_from_entry(const NgProgramRom *rom,
                                    uint32_t entry,
                                    NgFunctionDiscovery *out) {
    if (!rom || !out) {
        return 0;
    }

    ng_function_discovery_init(out);
    if (!ng_function_discovery_add(out, rom, entry)) {
        return 0;
    }

    for (uint32_t i = 0; i < out->count; ++i) {
        scan_function_candidate(rom, out->addrs[i], out);
    }
    return 1;
}
