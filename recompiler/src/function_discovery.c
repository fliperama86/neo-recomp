#include "function_discovery.h"

#include "m68k_analyze.h"
#include "m68k_decode.h"
#include "m68k_validate.h"

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

static void add_config_jump_table_targets(const NgProgramRom *rom,
                                          const NgGameConfigJumpTable *table,
                                          NgFunctionDiscovery *out) {
    if (!rom || !table || !out ||
        table->stride == 0u ||
        table->end <= table->start) {
        return;
    }

    for (uint32_t addr = table->start; addr < table->end;) {
        uint32_t target = 0;
        int have_target = 0;

        switch (table->format) {
        case NG_GAME_CONFIG_JUMP_TABLE_ABS32:
            if (ng_program_rom_addr_is_mapped(rom, addr) &&
                ng_program_rom_addr_is_mapped(rom, addr + 3u)) {
                target = ng_program_rom_read32(rom, addr);
                have_target = 1;
            }
            break;
        case NG_GAME_CONFIG_JUMP_TABLE_PCREL16:
            if (ng_program_rom_addr_is_mapped(rom, addr) &&
                ng_program_rom_addr_is_mapped(rom, addr + 1u)) {
                int16_t disp = (int16_t)ng_program_rom_read16(rom, addr);
                target = (uint32_t)((int32_t)(table->start & 0x00FFFFFFu) +
                                    (int32_t)disp);
                have_target = 1;
            }
            break;
        case NG_GAME_CONFIG_JUMP_TABLE_BRA16:
        case NG_GAME_CONFIG_JUMP_TABLE_BRA8:
            target = addr;
            have_target = 1;
            break;
        }

        if (have_target) {
            ng_function_discovery_add(out, rom, target);
        }

        if (UINT32_MAX - addr < table->stride) {
            break;
        }
        addr += table->stride;
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
           instr->src.mode == NG_M68K_EA_PC_DISP;
}

static int is_branch_target_candidate(const NgM68kInstr *instr) {
    if (instr->mnemonic == NG_M68K_BRA ||
        instr->mnemonic == NG_M68K_BCC) {
        return 1;
    }
    if (instr->mnemonic == NG_M68K_DBCC) {
        return instr->condition != 0u; /* DBT never decrements or branches. */
    }
    return 0;
}

static int has_static_code_target(const NgM68kInstr *instr) {
    if (!instr) {
        return 0;
    }
    switch (instr->src.mode) {
    case NG_M68K_EA_ABS_W:
    case NG_M68K_EA_ABS_L:
    case NG_M68K_EA_PC_DISP:
    case NG_M68K_EA_PC_INDEX:
        return 1;
    default:
        return 0;
    }
}

static int is_task_state_store(const NgM68kInstr *load,
                               const NgM68kInstr *store,
                               uint32_t *target) {
    if (!load || !store || !target) {
        return 0;
    }
    if (load->mnemonic != NG_M68K_LEA ||
        load->dst.mode != NG_M68K_EA_AREG ||
        !has_static_code_target(load)) {
        return 0;
    }
    if (store->mnemonic != NG_M68K_MOVE ||
        store->size != 4u ||
        store->src.mode != NG_M68K_EA_AREG ||
        store->src.reg != load->dst.reg ||
        store->dst.mode != NG_M68K_EA_AIND ||
        store->dst.reg != 6u) {
        return 0;
    }

    *target = load->target;
    return 1;
}

static int is_task_spawn_call(const NgM68kInstr *load,
                              const NgM68kInstr *call,
                              uint32_t *target) {
    if (!load || !call || !target) {
        return 0;
    }
    if (load->mnemonic != NG_M68K_LEA ||
        load->dst.mode != NG_M68K_EA_AREG ||
        load->dst.reg != 1u ||
        !has_static_code_target(load)) {
        return 0;
    }
    if (call->mnemonic != NG_M68K_JSR ||
        call->src.mode != NG_M68K_EA_ABS_L) {
        return 0;
    }

    uint32_t helper = call->target & 0x00FFFFFFu;
    if (helper != 0x0004AEu && helper != 0x0006FEu) {
        return 0;
    }

    *target = load->target;
    return 1;
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

        if (instr.byte_length == 0 ||
            !ng_m68k_validate(&instr) ||
            instr.mnemonic == NG_M68K_UNKNOWN ||
            instr.mnemonic == NG_M68K_INVALID) {
            return;
        }

        if (pc != start_addr) {
            ng_function_discovery_add(out, rom, pc);
        }

        if (have_previous) {
            NgM68kJumpTablePattern pattern;
            if (ng_m68k_match_pc_index_jump_table(&previous, &instr, &pattern)) {
                add_jump_table_targets(rom, &pattern, out);
            } else {
                uint32_t target = 0;
                if (is_task_state_store(&previous, &instr, &target)) {
                    ng_function_discovery_add(out, rom, target);
                } else if (is_task_spawn_call(&previous, &instr, &target)) {
                    ng_function_discovery_add(out, rom, target);
                }
            }
        }

        if (is_direct_function_target(&instr)) {
            ng_function_discovery_add(out, rom, instr.target);
        }
        if (is_branch_target_candidate(&instr)) {
            ng_function_discovery_add(out, rom, instr.target);
        }
        if ((instr.mnemonic == NG_M68K_JSR || instr.mnemonic == NG_M68K_BSR) &&
            instr.byte_length != 0) {
            ng_function_discovery_add(out, rom, pc + instr.byte_length);
        }
        if (instr.mnemonic == NG_M68K_STOP && instr.byte_length != 0) {
            ng_function_discovery_add(out, rom, pc + instr.byte_length);
        }

        if (instr.mnemonic == NG_M68K_BRA ||
            instr.mnemonic == NG_M68K_JMP ||
            instr.mnemonic == NG_M68K_RTS ||
            instr.mnemonic == NG_M68K_RTE ||
            instr.mnemonic == NG_M68K_RTR) {
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
    return ng_function_discover_from_seeds(rom, &entry, 1u, out);
}

int ng_function_discover_from_seeds(const NgProgramRom *rom,
                                    const uint32_t *seeds,
                                    uint32_t seed_count,
                                    NgFunctionDiscovery *out) {
    return ng_function_discover_from_game_config(rom, seeds, seed_count, NULL, out);
}

int ng_function_discover_from_game_config(const NgProgramRom *rom,
                                          const uint32_t *seeds,
                                          uint32_t seed_count,
                                          const NgGameConfig *config,
                                          NgFunctionDiscovery *out) {
    if (!rom || !out) {
        return 0;
    }

    ng_function_discovery_init(out);

    if (seeds) {
        for (uint32_t i = 0; i < seed_count; ++i) {
            ng_function_discovery_add(out, rom, seeds[i]);
        }
    }
    if (config) {
        for (uint32_t i = 0; i < config->entry_count; ++i) {
            ng_function_discovery_add(out, rom, config->entry[i]);
        }
        for (uint32_t i = 0; i < config->extra_count; ++i) {
            ng_function_discovery_add(out, rom, config->extra[i]);
        }
        for (uint32_t i = 0; i < config->jump_table_count; ++i) {
            add_config_jump_table_targets(rom, &config->jump_tables[i], out);
        }
    }
    if (out->count == 0u) {
        return 0;
    }

    for (uint32_t i = 0; i < out->count; ++i) {
        scan_function_candidate(rom, out->addrs[i], out);
    }
    return 1;
}
