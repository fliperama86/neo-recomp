#include "function_discovery.h"

#include "m68k_analyze.h"
#include "m68k_decode.h"
#include "m68k_validate.h"

#include <stdlib.h>
#include <string.h>

#define NG_FUNCTION_DISCOVERY_BRANCH_TABLE_MAX_ENTRIES 32u
#define NG_FUNCTION_DISCOVERY_BRANCH_PROBE_MAX_INSTRUCTIONS 12u
#define NG_FUNCTION_DISCOVERY_HEURISTIC_TARGET_MAX_INSTRUCTIONS 32u

static int is_probable_function_target(const NgProgramRom *rom,
                                       uint32_t addr);
static int is_probable_heuristic_code_target(const NgProgramRom *rom,
                                             uint32_t addr);
static int table_call_target_allowed(const NgProgramRom *rom,
                                     const NgGameConfigTableCall *call,
                                     uint32_t target);
static int is_direct_function_target(const NgM68kInstr *instr);

void ng_function_discovery_init(NgFunctionDiscovery *discovery) {
    if (discovery) {
        memset(discovery, 0, sizeof(*discovery));
        discovery->max_candidates = NG_FUNCTION_DISCOVERY_MAX_CANDIDATES;
        for (uint32_t i = 0; i < NG_FUNCTION_DISCOVERY_MAX_CANDIDATES; ++i) {
            discovery->banks[i] = NG_FUNCTION_DISCOVERY_BANK_NONE;
        }
    }
}

void ng_function_discovery_set_max_candidates(NgFunctionDiscovery *discovery,
                                              uint32_t max_candidates) {
    if (!discovery) {
        return;
    }
    if (max_candidates == 0u ||
        max_candidates > NG_FUNCTION_DISCOVERY_MAX_CANDIDATES) {
        max_candidates = NG_FUNCTION_DISCOVERY_MAX_CANDIDATES;
    }
    discovery->max_candidates = max_candidates;
}

static uint32_t discovery_bank_for_addr(const NgProgramRom *rom,
                                        uint32_t addr) {
    if (rom &&
        ng_program_rom_bank_count(rom) > 1u &&
        ng_program_rom_addr_is_banked(rom, addr)) {
        return rom->active_bank;
    }
    return NG_FUNCTION_DISCOVERY_BANK_NONE;
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

int ng_function_discovery_contains_bank(const NgFunctionDiscovery *discovery,
                                        uint32_t addr,
                                        uint32_t bank) {
    if (!discovery) {
        return 0;
    }
    for (uint32_t i = 0; i < discovery->count; ++i) {
        uint32_t entry_bank = discovery->banked[i] ?
            discovery->banks[i] : NG_FUNCTION_DISCOVERY_BANK_NONE;
        if (discovery->addrs[i] == addr && entry_bank == bank) {
            return 1;
        }
    }
    return 0;
}

int ng_function_discovery_contains_for_rom(const NgFunctionDiscovery *discovery,
                                           const NgProgramRom *rom,
                                           uint32_t addr) {
    return ng_function_discovery_contains_bank(discovery,
                                               addr,
                                               discovery_bank_for_addr(rom,
                                                                       addr));
}

uint32_t ng_function_discovery_bank_at(const NgFunctionDiscovery *discovery,
                                       uint32_t index) {
    if (!discovery || index >= discovery->count) {
        return NG_FUNCTION_DISCOVERY_BANK_NONE;
    }
    return discovery->banked[index] ?
        discovery->banks[index] : NG_FUNCTION_DISCOVERY_BANK_NONE;
}

int ng_function_discovery_is_entry_at(const NgFunctionDiscovery *discovery,
                                      uint32_t index) {
    return discovery && index < discovery->count && discovery->entries[index];
}

int ng_function_discovery_entry_contains_bank(
    const NgFunctionDiscovery *discovery,
    uint32_t addr,
    uint32_t bank) {
    if (!discovery) {
        return 0;
    }
    for (uint32_t i = 0; i < discovery->count; ++i) {
        uint32_t entry_bank = discovery->banked[i] ?
            discovery->banks[i] : NG_FUNCTION_DISCOVERY_BANK_NONE;
        if (discovery->addrs[i] == addr &&
            entry_bank == bank &&
            discovery->entries[i]) {
            return 1;
        }
    }
    return 0;
}

static int ng_function_discovery_add_with_entry(
    NgFunctionDiscovery *discovery,
    const NgProgramRom *rom,
    uint32_t addr,
    int is_entry) {
    if (!discovery || !rom || !is_probable_function_target(rom, addr)) {
        return 0;
    }
    uint32_t bank = discovery_bank_for_addr(rom, addr);
    for (uint32_t i = 0; i < discovery->count; ++i) {
        uint32_t entry_bank = discovery->banked[i] ?
            discovery->banks[i] : NG_FUNCTION_DISCOVERY_BANK_NONE;
        if (discovery->addrs[i] == addr && entry_bank == bank) {
            if (is_entry) {
                discovery->entries[i] = 1u;
            }
            return 1;
        }
    }
    uint32_t max_candidates = discovery->max_candidates != 0u ?
        discovery->max_candidates : NG_FUNCTION_DISCOVERY_MAX_CANDIDATES;
    if (max_candidates > NG_FUNCTION_DISCOVERY_MAX_CANDIDATES) {
        max_candidates = NG_FUNCTION_DISCOVERY_MAX_CANDIDATES;
    }
    if (discovery->count >= max_candidates) {
        discovery->truncated = 1;
        return 0;
    }

    discovery->addrs[discovery->count] = addr;
    discovery->entries[discovery->count] = is_entry ? 1u : 0u;
    if (bank != NG_FUNCTION_DISCOVERY_BANK_NONE) {
        discovery->banks[discovery->count] = bank;
        discovery->banked[discovery->count] = 1u;
    } else {
        discovery->banks[discovery->count] = NG_FUNCTION_DISCOVERY_BANK_NONE;
        discovery->banked[discovery->count] = 0u;
    }
    ++discovery->count;
    return 1;
}

static int ng_function_discovery_add_label(NgFunctionDiscovery *discovery,
                                           const NgProgramRom *rom,
                                           uint32_t addr) {
    return ng_function_discovery_add_with_entry(discovery, rom, addr, 0);
}

int ng_function_discovery_add(NgFunctionDiscovery *discovery,
                              const NgProgramRom *rom,
                              uint32_t addr) {
    return ng_function_discovery_add_with_entry(discovery, rom, addr, 1);
}

static void add_jump_table_targets(const NgProgramRom *rom,
                                   const NgM68kJumpTablePattern *pattern,
                                   NgFunctionDiscovery *out) {
    if (!rom || !pattern || !out) {
        return;
    }

    if (pattern->entry_kind == NG_M68K_JUMP_TABLE_ENTRY_ADDRESS ||
        pattern->entry_kind == NG_M68K_JUMP_TABLE_ENTRY_INLINE_CODE ||
        pattern->entry_kind == NG_M68K_JUMP_TABLE_ENTRY_DIRECT_DISPATCH) {
        uint32_t entry_addr = pattern->table_addr;
        uint32_t stride = pattern->entry_size;
        for (uint32_t i = 0;
             i < NG_FUNCTION_DISCOVERY_BRANCH_TABLE_MAX_ENTRIES;
             ++i) {
            if (pattern->entry_kind == NG_M68K_JUMP_TABLE_ENTRY_ADDRESS) {
                NgM68kInstr entry;
                if (!ng_m68k_decode(rom, entry_addr, &entry) ||
                    !ng_m68k_validate(&entry) ||
                    entry.mnemonic != NG_M68K_BRA ||
                    entry.byte_length == 0u) {
                    if (pattern->include_terminal_entry &&
                        is_probable_function_target(rom, entry_addr)) {
                        ng_function_discovery_add(out, rom, entry_addr);
                    }
                    break;
                }
                if (stride == 0u) {
                    stride = entry.byte_length;
                } else if (entry.byte_length != stride) {
                    break;
                }
            } else {
                if (pattern->entry_kind ==
                    NG_M68K_JUMP_TABLE_ENTRY_INLINE_CODE) {
                    if (stride == 0u ||
                        !ng_program_rom_addr_is_mapped(rom, entry_addr) ||
                        !ng_program_rom_addr_is_mapped(rom, entry_addr + 3u) ||
                        ng_program_rom_read32(rom, entry_addr) !=
                            pattern->entry_signature) {
                        break;
                    }
                } else {
                    NgM68kInstr entry;
                    if (stride == 0u ||
                        !ng_m68k_decode(rom, entry_addr, &entry) ||
                        !ng_m68k_validate(&entry) ||
                        entry.mnemonic != NG_M68K_JMP ||
                        entry.byte_length != stride ||
                        !is_direct_function_target(&entry)) {
                        break;
                    }
                }
            }

            ng_function_discovery_add(out, rom, entry_addr);
            if (UINT32_MAX - entry_addr < stride) {
                break;
            }
            entry_addr += stride;
        }
        return;
    }

    for (uint32_t i = 0; i < NG_FUNCTION_DISCOVERY_TABLE_ENTRIES; ++i) {
        uint32_t entry_addr = pattern->table_addr + i * pattern->entry_size;
        if (!ng_program_rom_addr_is_mapped(rom, entry_addr) ||
            !ng_program_rom_addr_is_mapped(rom, entry_addr + 3u)) {
            break;
        }

        uint32_t target = ng_program_rom_read32(rom, entry_addr);
        if (!is_probable_heuristic_code_target(rom, target)) {
            continue;
        }
        ng_function_discovery_add(out, rom, target);
    }
}

static int config_jump_table_target_allowed(const NgProgramRom *rom,
                                            const NgGameConfigJumpTable *table,
                                            uint32_t target) {
    if (!table) {
        return 0;
    }

    if (table->target_end > table->target_start &&
        (target < table->target_start || target >= table->target_end)) {
        return 0;
    }

    return is_probable_function_target(rom, target);
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
        case NG_GAME_CONFIG_JUMP_TABLE_SCRIPT_PREDICATE:
        case NG_GAME_CONFIG_JUMP_TABLE_TAGGED_ABS32:
        case NG_GAME_CONFIG_JUMP_TABLE_INLINE_CALLBACK:
            break;
        }

        if (have_target &&
            config_jump_table_target_allowed(rom, table, target)) {
            ng_function_discovery_add(out, rom, target);
        }

        if (UINT32_MAX - addr < table->stride) {
            break;
        }
        addr += table->stride;
    }
}

static int is_probable_function_target(const NgProgramRom *rom,
                                       uint32_t addr) {
    if (!rom || (addr & 1u) != 0u ||
        !ng_program_rom_addr_is_mapped(rom, addr)) {
        return 0;
    }

    NgM68kInstr instr;
    return ng_m68k_decode(rom, addr, &instr) &&
           instr.byte_length != 0u &&
           ng_m68k_validate(&instr) &&
           instr.mnemonic != NG_M68K_UNKNOWN &&
           instr.mnemonic != NG_M68K_INVALID;
}

static int heuristic_target_has_code_shape(const NgM68kInstr *instr) {
    if (!instr) {
        return 0;
    }

    switch (instr->mnemonic) {
    case NG_M68K_RTS:
    case NG_M68K_RTE:
    case NG_M68K_RTR:
    case NG_M68K_STOP:
    case NG_M68K_TRAP:
    case NG_M68K_TRAPV:
    case NG_M68K_BRA:
    case NG_M68K_BSR:
    case NG_M68K_BCC:
    case NG_M68K_DBCC:
    case NG_M68K_JMP:
    case NG_M68K_JSR:
        return 1;
    default:
        return 0;
    }
}

static int is_probable_heuristic_code_target(const NgProgramRom *rom,
                                             uint32_t addr) {
    if (!is_probable_function_target(rom, addr)) {
        return 0;
    }

    uint32_t pc = addr;
    for (uint32_t i = 0u;
         i < NG_FUNCTION_DISCOVERY_HEURISTIC_TARGET_MAX_INSTRUCTIONS;
         ++i) {
        NgM68kInstr instr;
        if (!ng_m68k_decode(rom, pc, &instr) ||
            instr.byte_length == 0u ||
            !ng_m68k_validate(&instr) ||
            instr.mnemonic == NG_M68K_UNKNOWN ||
            instr.mnemonic == NG_M68K_INVALID) {
            return 0;
        }

        /*
           A/F-line and raw ILLEGAL opcodes are valid 68000 exception
           encodings, but in heuristic pointer scans they are much more often
           sprite/animation data being decoded as instructions. Direct calls
           and branches still use the weaker entry filter above.
        */
        if (instr.mnemonic == NG_M68K_ILLEGAL ||
            instr.mnemonic == NG_M68K_RESET) {
            return 0;
        }

        if (heuristic_target_has_code_shape(&instr)) {
            return 1;
        }

        if (UINT32_MAX - pc < instr.byte_length) {
            return 0;
        }
        pc += instr.byte_length;
    }

    return 1;
}

static void add_sparse_abs32_table_targets(const NgProgramRom *rom,
                                           uint32_t table_addr,
                                           const NgGameConfigTableCall *call,
                                           uint32_t max_entries,
                                           uint32_t sentinel,
                                           NgFunctionDiscovery *out) {
    if (!rom || !call || !out || max_entries == 0u ||
        !ng_program_rom_addr_is_mapped(rom, table_addr)) {
        return;
    }

    for (uint32_t i = 0; i < max_entries; ++i) {
        uint32_t entry_addr = table_addr + i * 4u;
        if (!ng_program_rom_addr_is_mapped(rom, entry_addr) ||
            !ng_program_rom_addr_is_mapped(rom, entry_addr + 3u)) {
            break;
        }

        uint32_t target = ng_program_rom_read32(rom, entry_addr);
        if (target == sentinel) {
            continue;
        }
        if (!table_call_target_allowed(rom, call, target)) {
            break;
        }

        ng_function_discovery_add(out, rom, target);
    }
}

static int state_table_longword_in_table(const NgProgramRom *rom,
                                         const NgGameConfigStateTable *table,
                                         uint32_t addr) {
    if (!rom || !table ||
        table->table_end <= table->table_start ||
        addr < table->table_start ||
        UINT32_MAX - addr < 3u ||
        addr + 3u >= table->table_end) {
        return 0;
    }
    return ng_program_rom_addr_is_mapped(rom, addr) &&
           ng_program_rom_addr_is_mapped(rom, addr + 3u);
}

static int state_table_target_allowed(const NgProgramRom *rom,
                                      const NgGameConfigStateTable *table,
                                      uint32_t target) {
    if (!rom || !table) {
        return 0;
    }
    if (table->target_end > table->target_start &&
        (target < table->target_start || target >= table->target_end)) {
        return 0;
    }
    return is_probable_function_target(rom, target);
}

static int state_table_seen(const uint32_t *tables,
                            uint32_t count,
                            uint32_t addr) {
    if (!tables) {
        return 0;
    }
    for (uint32_t i = 0; i < count; ++i) {
        if (tables[i] == addr) {
            return 1;
        }
    }
    return 0;
}

static void add_state_table_targets(const NgProgramRom *rom,
                                    const NgGameConfigStateTable *table,
                                    NgFunctionDiscovery *out) {
    if (!rom || !table || !out ||
        table->stride == 0u ||
        table->max_tables == 0u ||
        table->max_entries == 0u ||
        !state_table_longword_in_table(rom, table, table->root)) {
        return;
    }

    uint32_t *tables =
        (uint32_t *)calloc(table->max_tables, sizeof(*tables));
    if (!tables) {
        out->truncated = 1;
        return;
    }

    uint32_t head = 0u;
    uint32_t count = 1u;
    tables[0] = table->root;
    while (head < count) {
        uint32_t entry_addr = tables[head++];
        for (uint32_t i = 0; i < table->max_entries; ++i) {
            if (!state_table_longword_in_table(rom, table, entry_addr)) {
                break;
            }

            uint32_t value = ng_program_rom_read32(rom, entry_addr);
            if (value == table->sentinel) {
                /* sparse sentinel, keep scanning this table */
            } else if (table->follow_chain &&
                       state_table_longword_in_table(rom, table, value)) {
                if (!state_table_seen(tables, count, value)) {
                    if (count >= table->max_tables) {
                        out->truncated = 1;
                        break;
                    }
                    tables[count++] = value;
                }
            } else if (state_table_target_allowed(rom, table, value)) {
                ng_function_discovery_add(out, rom, value);
            } else if (table->target_end > table->target_start &&
                       value >= table->target_start &&
                       value < table->target_end &&
                       ((value & 1u) != 0u || table->skip_invalid_targets)) {
                /* Some state tables interleave odd state markers with code
                   callbacks.  Others interleave even marker values in the
                   same numeric range.  Odd 68k addresses cannot be code
                   targets; even marker values require explicit opt-in through
                   skip_invalid_targets so stricter tables still terminate on
                   malformed entries. */
            } else {
                break;
            }

            if (UINT32_MAX - entry_addr < table->stride) {
                break;
            }
            entry_addr += table->stride;
        }
    }

    free(tables);
}

static void add_state_table_targets_with_scans(
    const NgProgramRom *rom,
    const NgGameConfigStateTable *table,
    NgFunctionDiscovery *out) {
    if (!rom || !table || !out) {
        return;
    }
    if (table->scan_count == 0u) {
        add_state_table_targets(rom, table, out);
        return;
    }

    for (uint32_t i = 0; i < table->scan_count; ++i) {
        if (table->scans[i].kind == NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL) {
            uint32_t bank_count = ng_program_rom_bank_count(rom);
            for (uint32_t bank = 0; bank < bank_count; ++bank) {
                if (!ng_program_rom_bank_is_configured(rom, bank)) {
                    continue;
                }
                NgProgramRom bank_rom = *rom;
                ng_program_rom_select_bank(&bank_rom, bank);
                add_state_table_targets(&bank_rom, table, out);
            }
        } else if (table->scans[i].kind ==
                   NG_GAME_CONFIG_RECORD_SCAN_BANK_ONE) {
            if (!ng_program_rom_bank_is_configured(rom,
                                                   table->scans[i].bank_id)) {
                continue;
            }
            NgProgramRom bank_rom = *rom;
            ng_program_rom_select_bank(&bank_rom, table->scans[i].bank_id);
            add_state_table_targets(&bank_rom, table, out);
        } else {
            add_state_table_targets(rom, table, out);
        }
    }
}

static int table_call_target_allowed(const NgProgramRom *rom,
                                     const NgGameConfigTableCall *call,
                                     uint32_t target) {
    if (!rom || !call) {
        return 0;
    }

    if (call->target_end > call->target_start &&
        (target < call->target_start || target >= call->target_end)) {
        return 0;
    }

    return is_probable_heuristic_code_target(rom, target);
}

static void add_tagged_abs32_stream_targets(const NgProgramRom *rom,
                                            uint32_t stream_addr,
                                            const NgGameConfigTableCall *call,
                                            NgFunctionDiscovery *out) {
    if (!rom || !call || !out || call->max_entries == 0u ||
        call->stride == 0u ||
        !ng_program_rom_addr_is_mapped(rom, stream_addr)) {
        return;
    }

    for (uint32_t i = 0; i < call->max_entries; ++i) {
        uint32_t addr = stream_addr + i * call->stride;
        if (!ng_program_rom_addr_is_mapped(rom, addr) ||
            !ng_program_rom_addr_is_mapped(rom, addr + 1u)) {
            break;
        }

        if (ng_program_rom_read16(rom, addr) != (uint16_t)call->match) {
            continue;
        }

        uint32_t target_addr = addr + call->target_offset;
        if (!ng_program_rom_addr_is_mapped(rom, target_addr) ||
            !ng_program_rom_addr_is_mapped(rom, target_addr + 3u)) {
            continue;
        }

        uint32_t target = ng_program_rom_read32(rom, target_addr);
        if (table_call_target_allowed(rom, call, target)) {
            ng_function_discovery_add(out, rom, target);
        }
    }
}

static void add_config_table_call_targets(const NgProgramRom *rom,
                                          const NgGameConfig *config,
                                          const NgM68kInstr *instr,
                                          const NgM68kStaticAregState *areg,
                                          NgFunctionDiscovery *out) {
    if (!rom || !config || !instr || !areg || !out ||
        instr->mnemonic != NG_M68K_JSR ||
        !is_direct_function_target(instr)) {
        return;
    }

    uint32_t helper = instr->target & 0x00FFFFFFu;
    for (uint32_t i = 0; i < config->table_call_count; ++i) {
        const NgGameConfigTableCall *call = &config->table_calls[i];
        if ((call->helper & 0x00FFFFFFu) != helper ||
            call->table_reg > 7u ||
            !areg->valid[call->table_reg]) {
            continue;
        }

        uint32_t table_addr = areg->target[call->table_reg] & 0x00FFFFFFu;
        if (call->table_end > call->table_start &&
            (table_addr < call->table_start || table_addr >= call->table_end)) {
            continue;
        }

        switch (call->format) {
        case NG_GAME_CONFIG_TABLE_CALL_ABS32_SPARSE:
            add_sparse_abs32_table_targets(rom,
                                           table_addr,
                                           call,
                                           call->max_entries,
                                           call->sentinel,
                                           out);
            break;
        case NG_GAME_CONFIG_TABLE_CALL_TAGGED_ABS32:
            add_tagged_abs32_stream_targets(rom, table_addr, call, out);
            break;
        }
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

static int is_indirect_areg_dispatch(const NgM68kInstr *instr) {
    return instr &&
           (instr->mnemonic == NG_M68K_JSR ||
            instr->mnemonic == NG_M68K_JMP) &&
           instr->src.mode == NG_M68K_EA_AIND &&
           instr->src.reg < 8u;
}

static int abs_pointer_load_addr(const NgM68kInstr *load,
                                 uint8_t reg,
                                 uint32_t *ptr_addr) {
    if (!load || !ptr_addr ||
        load->mnemonic != NG_M68K_MOVEA ||
        load->size != 4u ||
        load->dst.mode != NG_M68K_EA_AREG ||
        load->dst.reg != reg) {
        return 0;
    }

    if (load->src.mode == NG_M68K_EA_ABS_W ||
        load->src.mode == NG_M68K_EA_ABS_L ||
        load->src.mode == NG_M68K_EA_PC_DISP) {
        *ptr_addr = load->src.absolute_addr & 0x00FFFFFFu;
        return 1;
    }
    return 0;
}

static void add_abs_pointer_target_from_view(const NgProgramRom *rom,
                                             uint32_t ptr_addr,
                                             NgFunctionDiscovery *out) {
    if (!rom || !out ||
        !ng_program_rom_addr_is_mapped(rom, ptr_addr) ||
        !ng_program_rom_addr_is_mapped(rom, ptr_addr + 3u)) {
        return;
    }

    uint32_t target = ng_program_rom_read32(rom, ptr_addr);
    if (is_probable_function_target(rom, target)) {
        ng_function_discovery_add(out, rom, target);
    }
}

static void add_abs_pointer_indirect_targets(const NgProgramRom *rom,
                                             uint32_t start_addr,
                                             const NgM68kInstr *load,
                                             const NgM68kInstr *dispatch,
                                             NgFunctionDiscovery *out) {
    if (!rom || !load || !dispatch || !out ||
        !is_indirect_areg_dispatch(dispatch)) {
        return;
    }

    uint32_t ptr_addr = 0;
    if (!abs_pointer_load_addr(load, dispatch->src.reg, &ptr_addr)) {
        return;
    }

    if (ng_program_rom_addr_is_banked(rom, ptr_addr) &&
        !ng_program_rom_addr_is_banked(rom, start_addr) &&
        ng_program_rom_bank_count(rom) > 1u) {
        uint32_t bank_count = ng_program_rom_bank_count(rom);
        for (uint32_t bank = 0; bank < bank_count; ++bank) {
            if (!ng_program_rom_bank_is_configured(rom, bank)) {
                continue;
            }
            NgProgramRom bank_rom = *rom;
            ng_program_rom_select_bank(&bank_rom, bank);
            add_abs_pointer_target_from_view(&bank_rom, ptr_addr, out);
        }
        return;
    }

    add_abs_pointer_target_from_view(rom, ptr_addr, out);
}

static void add_static_areg_indirect_target(const NgProgramRom *rom,
                                            uint32_t start_addr,
                                            const NgM68kInstr *dispatch,
                                            const NgM68kStaticAregState *areg,
                                            NgFunctionDiscovery *out) {
    if (!rom || !dispatch || !areg || !out ||
        !is_indirect_areg_dispatch(dispatch) ||
        dispatch->src.reg >= 8u ||
        !areg->valid[dispatch->src.reg]) {
        return;
    }

    uint32_t target = areg->target[dispatch->src.reg] & 0x00FFFFFFu;
    if (ng_program_rom_addr_is_banked(rom, target) &&
        !ng_program_rom_addr_is_banked(rom, start_addr) &&
        ng_program_rom_bank_count(rom) > 1u) {
        uint32_t bank_count = ng_program_rom_bank_count(rom);
        for (uint32_t bank = 0; bank < bank_count; ++bank) {
            if (!ng_program_rom_bank_is_configured(rom, bank)) {
                continue;
            }
            NgProgramRom bank_rom = *rom;
            ng_program_rom_select_bank(&bank_rom, bank);
            ng_function_discovery_add(out, &bank_rom, target);
        }
        return;
    }

    ng_function_discovery_add(out, rom, target);
}

static void static_dreg_reset(uint8_t *valid) {
    if (valid) {
        memset(valid, 0, 8u * sizeof(*valid));
    }
}

static void static_dreg_update(uint8_t *valid,
                               uint32_t *target,
                               const NgM68kInstr *instr) {
    if (!valid || !target || !instr) {
        return;
    }

    if (instr->mnemonic == NG_M68K_JSR ||
        instr->mnemonic == NG_M68K_BSR) {
        static_dreg_reset(valid);
        return;
    }

    if (instr->mnemonic == NG_M68K_MOVE &&
        instr->dst.mode == NG_M68K_EA_DREG &&
        instr->dst.reg < 8u) {
        if (instr->size == 4u && instr->src.mode == NG_M68K_EA_IMM) {
            target[instr->dst.reg] = instr->src.immediate & 0x00FFFFFFu;
            valid[instr->dst.reg] = 1u;
        } else {
            valid[instr->dst.reg] = 0u;
        }
        return;
    }

    switch (instr->mnemonic) {
    case NG_M68K_MOVEQ:
    case NG_M68K_ADD:
    case NG_M68K_ADDQ:
    case NG_M68K_ADDX:
    case NG_M68K_SUB:
    case NG_M68K_SUBQ:
    case NG_M68K_SUBX:
    case NG_M68K_OR:
    case NG_M68K_AND:
    case NG_M68K_EOR:
    case NG_M68K_MULU:
    case NG_M68K_MULS:
    case NG_M68K_DIVU:
    case NG_M68K_DIVS:
    case NG_M68K_CLR:
    case NG_M68K_NEG:
    case NG_M68K_NEGX:
    case NG_M68K_NBCD:
    case NG_M68K_NOT:
    case NG_M68K_EXT:
    case NG_M68K_SWAP:
    case NG_M68K_ASL:
    case NG_M68K_ASR:
    case NG_M68K_LSL:
    case NG_M68K_LSR:
    case NG_M68K_ROXL:
    case NG_M68K_ROXR:
    case NG_M68K_ROL:
    case NG_M68K_ROR:
    case NG_M68K_ADDI:
    case NG_M68K_SUBI:
    case NG_M68K_ORI:
    case NG_M68K_ANDI:
    case NG_M68K_EORI:
        if (instr->dst.mode == NG_M68K_EA_DREG && instr->dst.reg < 8u) {
            valid[instr->dst.reg] = 0u;
        }
        break;
    default:
        break;
    }
}

static void static_dreg_to_areg_update(NgM68kStaticAregState *areg,
                                       const uint8_t *dreg_valid,
                                       const uint32_t *dreg_target,
                                       const NgM68kInstr *instr) {
    if (!areg || !dreg_valid || !dreg_target || !instr ||
        instr->mnemonic != NG_M68K_MOVEA ||
        instr->size != 4u ||
        instr->src.mode != NG_M68K_EA_DREG ||
        instr->src.reg >= 8u ||
        instr->dst.mode != NG_M68K_EA_AREG ||
        instr->dst.reg >= 8u) {
        return;
    }

    if (dreg_valid[instr->src.reg]) {
        areg->target[instr->dst.reg] =
            dreg_target[instr->src.reg] & 0x00FFFFFFu;
        areg->valid[instr->dst.reg] = 1u;
    }
}

static int scan_state_has_static_targets(const NgM68kStaticAregState *areg,
                                         const uint8_t *dreg_valid) {
    if (!areg || !dreg_valid) {
        return 0;
    }
    for (uint32_t i = 0; i < 8u; ++i) {
        if (areg->valid[i] || dreg_valid[i]) {
            return 1;
        }
    }
    return 0;
}

static void add_branch_target_static_indirects(const NgProgramRom *rom,
                                               uint32_t start_addr,
                                               uint32_t branch_target,
                                               const NgM68kStaticAregState *areg,
                                               const uint8_t *dreg_valid,
                                               const uint32_t *dreg_target,
                                               NgFunctionDiscovery *out) {
    if (!rom || !areg || !dreg_valid || !dreg_target || !out ||
        !scan_state_has_static_targets(areg, dreg_valid) ||
        !is_probable_function_target(rom, branch_target)) {
        return;
    }

    NgM68kStaticAregState probe_areg = *areg;
    uint8_t probe_dreg_valid[8];
    uint32_t probe_dreg_target[8];
    memcpy(probe_dreg_valid, dreg_valid, sizeof(probe_dreg_valid));
    memcpy(probe_dreg_target, dreg_target, sizeof(probe_dreg_target));

    uint32_t pc = branch_target & 0x00FFFFFFu;
    for (uint32_t i = 0u;
         i < NG_FUNCTION_DISCOVERY_BRANCH_PROBE_MAX_INSTRUCTIONS;
         ++i) {
        NgM68kInstr instr;
        if (!ng_m68k_decode(rom, pc, &instr) ||
            instr.byte_length == 0u ||
            !ng_m68k_validate(&instr) ||
            instr.mnemonic == NG_M68K_UNKNOWN ||
            instr.mnemonic == NG_M68K_INVALID) {
            break;
        }

        add_static_areg_indirect_target(rom,
                                        start_addr,
                                        &instr,
                                        &probe_areg,
                                        out);

        if (instr.mnemonic == NG_M68K_BRA ||
            instr.mnemonic == NG_M68K_JMP ||
            instr.mnemonic == NG_M68K_RTS ||
            instr.mnemonic == NG_M68K_RTE ||
            instr.mnemonic == NG_M68K_RTR) {
            break;
        }

        ng_m68k_static_areg_update(&probe_areg, &instr);
        static_dreg_to_areg_update(&probe_areg,
                                   probe_dreg_valid,
                                   probe_dreg_target,
                                   &instr);
        static_dreg_update(probe_dreg_valid, probe_dreg_target, &instr);

        pc += instr.byte_length;
    }
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

static int legacy_task_state_store_slot_allowed(const NgM68kInstr *store) {
    if (!store) {
        return 0;
    }
    if (store->dst.mode == NG_M68K_EA_AIND) {
        return 1;
    }
    return store->dst.mode == NG_M68K_EA_ADISP &&
           store->dst.displacement == 0x70;
}

static int dispatcher_install_slot_matches(
    const NgGameConfigDispatcher *dispatcher,
    uint32_t slot) {
    if (!dispatcher ||
        dispatcher->kind != NG_GAME_CONFIG_DISPATCHER_OBJECT_STATE) {
        return 0;
    }

    for (uint32_t i = 0; i < dispatcher->install_slot_count; ++i) {
        if (dispatcher->install_slots[i] == slot) {
            return 1;
        }
    }
    return dispatcher->has_state_slot && dispatcher->state_slot == slot;
}

static int dispatcher_task_state_store_slot_allowed(
    const NgGameConfig *config,
    const NgM68kInstr *store) {
    if (!store) {
        return 0;
    }

    if (!config || config->dispatcher_count == 0u) {
        return legacy_task_state_store_slot_allowed(store);
    }

    uint32_t slot = 0;
    if (store->dst.mode == NG_M68K_EA_AIND) {
        slot = 0u;
    } else if (store->dst.mode == NG_M68K_EA_ADISP) {
        slot = (uint32_t)(uint16_t)store->dst.displacement;
    } else {
        return 0;
    }

    for (uint32_t i = 0; i < config->dispatcher_count; ++i) {
        if (dispatcher_install_slot_matches(&config->dispatchers[i], slot)) {
            return 1;
        }
    }
    return 0;
}

static int config_state_table_contains_addr(const NgGameConfig *config,
                                            uint32_t addr) {
    if (!config) {
        return 0;
    }
    addr &= 0x00FFFFFFu;
    for (uint32_t i = 0; i < config->state_table_count; ++i) {
        const NgGameConfigStateTable *table = &config->state_tables[i];
        if (table->table_end > table->table_start &&
            addr >= (table->table_start & 0x00FFFFFFu) &&
            addr < (table->table_end & 0x00FFFFFFu)) {
            return 1;
        }
    }
    return 0;
}

static int legacy_task_spawn_helper_allowed(uint32_t helper) {
    return helper == 0x0004AEu || helper == 0x0006FEu;
}

static int dispatcher_task_spawn_helper_allowed(const NgGameConfig *config,
                                                uint32_t helper) {
    if (!config || config->dispatcher_count == 0u) {
        return legacy_task_spawn_helper_allowed(helper);
    }

    for (uint32_t i = 0; i < config->dispatcher_count; ++i) {
        const NgGameConfigDispatcher *dispatcher = &config->dispatchers[i];
        if (dispatcher->kind != NG_GAME_CONFIG_DISPATCHER_OBJECT_STATE) {
            continue;
        }
        for (uint32_t j = 0; j < dispatcher->spawn_helper_count; ++j) {
            if (dispatcher->spawn_helpers[j] == helper) {
                return 1;
            }
        }
    }
    return 0;
}

static int instr_writes_areg_number(const NgM68kInstr *instr, uint8_t reg) {
    return instr &&
           instr->dst.mode == NG_M68K_EA_AREG &&
           instr->dst.reg == reg;
}

static int dispatcher_task_spawn_wrapper_allowed(const NgProgramRom *rom,
                                                 const NgGameConfig *config,
                                                 uint32_t helper) {
    if (!rom || !ng_program_rom_addr_is_mapped(rom, helper)) {
        return 0;
    }

    uint32_t pc = helper;
    for (uint32_t i = 0; i < 12u; ++i) {
        NgM68kInstr instr;
        if (!ng_m68k_decode(rom, pc, &instr) ||
            !ng_m68k_validate(&instr) ||
            instr.byte_length == 0u ||
            instr.mnemonic == NG_M68K_UNKNOWN ||
            instr.mnemonic == NG_M68K_INVALID) {
            return 0;
        }

        if (instr.mnemonic == NG_M68K_JSR &&
            instr.src.mode == NG_M68K_EA_ABS_L &&
            dispatcher_task_spawn_helper_allowed(
                config,
                instr.target & 0x00FFFFFFu)) {
            return 1;
        }

        if (instr_writes_areg_number(&instr, 1u)) {
            return 0;
        }
        if (instr.mnemonic == NG_M68K_RTS ||
            instr.mnemonic == NG_M68K_RTE ||
            instr.mnemonic == NG_M68K_RTR ||
            instr.mnemonic == NG_M68K_JMP) {
            return 0;
        }
        pc += instr.byte_length;
    }
    return 0;
}

static int is_task_state_store(const NgProgramRom *rom,
                               const NgGameConfig *config,
                               const NgM68kInstr *load,
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
        store->dst.reg != 6u) {
        return 0;
    }
    /* Metal Slug object setup also stores secondary state callbacks in the
       A6-relative $70 slot.  Keep ADISP matching narrow so ordinary object-data
       pointer stores do not explode discovery, and do not treat bank-window
       animation/sprite data pointers as code just because they are stored in
       the same field. */
    if (store->dst.mode == NG_M68K_EA_ADISP) {
        if ((load->target & 0x00FFFFFFu) >= 0x00100000u) {
            return 0;
        }
    }
    if (!dispatcher_task_state_store_slot_allowed(config, store)) {
        return 0;
    }
    if (!is_probable_heuristic_code_target(rom, load->target) &&
        !config_state_table_contains_addr(config, load->target)) {
        return 0;
    }

    *target = load->target;
    return 1;
}

static int is_task_spawn_call(const NgProgramRom *rom,
                              const NgGameConfig *config,
                              const NgM68kInstr *load,
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
    if (!dispatcher_task_spawn_helper_allowed(config, helper) &&
        !dispatcher_task_spawn_wrapper_allowed(rom, config, helper)) {
        return 0;
    }
    if (!is_probable_heuristic_code_target(rom, load->target)) {
        return 0;
    }

    *target = load->target;
    return 1;
}

static int is_work_ram_absolute(uint32_t addr) {
    return addr >= 0x00100000u && addr <= 0x0010FFFFu;
}

static int is_script_predicate_candidate(const NgProgramRom *rom,
                                         uint32_t addr) {
    NgM68kInstr cmpi;
    if (!ng_m68k_decode(rom, addr, &cmpi) ||
        !ng_m68k_validate(&cmpi) ||
        cmpi.mnemonic != NG_M68K_CMPI ||
        cmpi.byte_length == 0u ||
        cmpi.dst.mode != NG_M68K_EA_ABS_L ||
        !is_work_ram_absolute(cmpi.dst.absolute_addr)) {
        return 0;
    }

    uint32_t pc = addr + cmpi.byte_length;
    NgM68kInstr scc;
    if (!ng_m68k_decode(rom, pc, &scc) ||
        !ng_m68k_validate(&scc) ||
        scc.mnemonic != NG_M68K_SCC ||
        scc.byte_length == 0u ||
        scc.dst.mode != NG_M68K_EA_DREG ||
        scc.dst.reg != 0u) {
        return 0;
    }

    pc += scc.byte_length;
    NgM68kInstr lea;
    if (!ng_m68k_decode(rom, pc, &lea) ||
        !ng_m68k_validate(&lea) ||
        lea.mnemonic != NG_M68K_LEA ||
        lea.byte_length == 0u ||
        lea.dst.mode != NG_M68K_EA_AREG ||
        lea.dst.reg != 1u ||
        lea.src.mode != NG_M68K_EA_PC_DISP) {
        return 0;
    }

    pc += lea.byte_length;
    NgM68kInstr rts;
    return ng_m68k_decode(rom, pc, &rts) &&
           ng_m68k_validate(&rts) &&
           rts.mnemonic == NG_M68K_RTS;
}

static void add_script_predicate_targets(const NgProgramRom *rom,
                                         const NgGameConfigJumpTable *table,
                                         NgFunctionDiscovery *out) {
    if (!rom || !table || !out ||
        table->stride == 0u ||
        table->end <= table->start) {
        return;
    }

    for (uint32_t addr = table->start; addr < table->end;) {
        if (is_script_predicate_candidate(rom, addr)) {
            ng_function_discovery_add(out, rom, addr);
        }
        if (UINT32_MAX - addr < table->stride) {
            break;
        }
        addr += table->stride;
    }
}

static void add_tagged_abs32_targets(const NgProgramRom *rom,
                                     const NgGameConfigJumpTable *table,
                                     NgFunctionDiscovery *out) {
    if (!rom || !table || !out ||
        table->stride == 0u ||
        table->end <= table->start) {
        return;
    }

    for (uint32_t addr = table->start; addr < table->end;) {
        if (ng_program_rom_addr_is_mapped(rom, addr) &&
            ng_program_rom_addr_is_mapped(rom, addr + 1u) &&
            ng_program_rom_read16(rom, addr) == (uint16_t)table->match) {
            uint32_t target_addr = addr + table->target_offset;
            if (ng_program_rom_addr_is_mapped(rom, target_addr) &&
                ng_program_rom_addr_is_mapped(rom, target_addr + 3u)) {
                uint32_t target = ng_program_rom_read32(rom, target_addr);
                if ((table->target_end <= table->target_start ||
                     (target >= table->target_start &&
                      target < table->target_end)) &&
                    is_probable_function_target(rom, target)) {
                    ng_function_discovery_add(out, rom, target);
                }
            }
        }

        if (UINT32_MAX - addr < table->stride) {
            break;
        }
        addr += table->stride;
    }
}

static int is_inline_script_callback_target(const NgProgramRom *rom,
                                            const NgGameConfigJumpTable *table,
                                            uint32_t target) {
    if (!rom || !table ||
        (table->target_end > table->target_start &&
         (target < table->target_start || target >= table->target_end)) ||
        !is_probable_function_target(rom, target)) {
        return 0;
    }

    uint32_t pc = target;
    int saw_script_store = 0;
    for (uint32_t i = 0; i < 32u; ++i) {
        NgM68kInstr instr;
        if (!ng_m68k_decode(rom, pc, &instr) ||
            !ng_m68k_validate(&instr) ||
            instr.byte_length == 0u ||
            instr.mnemonic == NG_M68K_UNKNOWN ||
            instr.mnemonic == NG_M68K_INVALID) {
            return 0;
        }

        if (instr.mnemonic == NG_M68K_MOVE &&
            instr.size == 4u &&
            instr.src.mode == NG_M68K_EA_IMM &&
            instr.dst.mode == NG_M68K_EA_AIND &&
            instr.dst.reg == 0u) {
            saw_script_store = 1;
        }

        if (instr.mnemonic == NG_M68K_RTS) {
            return saw_script_store;
        }
        if (instr.mnemonic == NG_M68K_RTE ||
            instr.mnemonic == NG_M68K_RTR ||
            instr.mnemonic == NG_M68K_STOP ||
            instr.mnemonic == NG_M68K_TRAP ||
            instr.mnemonic == NG_M68K_ILLEGAL ||
            instr.mnemonic == NG_M68K_JMP) {
            return 0;
        }
        if (UINT32_MAX - pc < instr.byte_length) {
            return 0;
        }
        pc += instr.byte_length;
    }
    return 0;
}

static void add_inline_callback_targets(const NgProgramRom *rom,
                                        const NgGameConfigJumpTable *table,
                                        NgFunctionDiscovery *out) {
    if (!rom || !table || !out ||
        table->stride == 0u ||
        table->end <= table->start) {
        return;
    }

    for (uint32_t addr = table->start; addr < table->end;) {
        if (ng_program_rom_addr_is_mapped(rom, addr) &&
            ng_program_rom_addr_is_mapped(rom, addr + 1u)) {
            uint16_t opcode = ng_program_rom_read16(rom, addr);
            if ((table->match == 0u &&
                 (opcode == 0x0003u || opcode == 0x0004u)) ||
                opcode == (uint16_t)table->match) {
                uint32_t target = addr + table->target_offset;
                if (is_inline_script_callback_target(rom, table, target)) {
                    ng_function_discovery_add(out, rom, target);
                }
            }
        }

        if (UINT32_MAX - addr < table->stride) {
            break;
        }
        addr += table->stride;
    }
}

static int record_format_target_allowed(const NgProgramRom *rom,
                                        const NgGameConfigRecordFormat *record,
                                        uint32_t target,
                                        int strict_target_shape) {
    if (!rom || !record) {
        return 0;
    }
    if (record->target_end > record->target_start &&
        (target < record->target_start || target >= record->target_end)) {
        return 0;
    }
    return strict_target_shape ?
        is_probable_heuristic_code_target(rom, target) :
        is_probable_function_target(rom, target);
}

static int record_scan_bounds(const NgProgramRom *rom,
                              const NgGameConfigRecordScan *scan,
                              uint32_t *out_start,
                              uint32_t *out_end) {
    if (!rom || !scan || !out_start || !out_end) {
        return 0;
    }

    switch (scan->kind) {
    case NG_GAME_CONFIG_RECORD_SCAN_FIXED:
    case NG_GAME_CONFIG_RECORD_SCAN_FIXED_AUTO:
        if (rom->address_map_enabled) {
            if (rom->fixed_size == 0u ||
                UINT32_MAX - rom->fixed_base < rom->fixed_size) {
                return 0;
            }
            *out_start = rom->fixed_base;
            *out_end = rom->fixed_base + rom->fixed_size;
            return 1;
        }
        *out_start = 0u;
        *out_end = rom->size;
        return 1;
    case NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL:
    case NG_GAME_CONFIG_RECORD_SCAN_BANK_ONE:
    case NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL_AUTO:
    case NG_GAME_CONFIG_RECORD_SCAN_BANK_ONE_AUTO:
        if (!rom->address_map_enabled ||
            rom->bank_window_size == 0u ||
            UINT32_MAX - rom->bank_window_base < rom->bank_window_size) {
            return 0;
        }
        if (scan->end > scan->start) {
            *out_start = scan->start;
            *out_end = scan->end;
            return 1;
        }
        *out_start = rom->bank_window_base;
        *out_end = rom->bank_window_base + rom->bank_window_size;
        return 1;
    case NG_GAME_CONFIG_RECORD_SCAN_RANGE:
    case NG_GAME_CONFIG_RECORD_SCAN_RANGE_AUTO:
        if (scan->end <= scan->start) {
            return 0;
        }
        *out_start = scan->start;
        *out_end = scan->end;
        return 1;
    default:
        return 0;
    }
}

static int record_scan_is_auto(const NgGameConfigRecordScan *scan) {
    return scan &&
           (scan->kind == NG_GAME_CONFIG_RECORD_SCAN_FIXED_AUTO ||
            scan->kind == NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL_AUTO ||
            scan->kind == NG_GAME_CONFIG_RECORD_SCAN_BANK_ONE_AUTO ||
            scan->kind == NG_GAME_CONFIG_RECORD_SCAN_RANGE_AUTO);
}

static int record_scan_is_bank_all(const NgGameConfigRecordScan *scan) {
    return scan &&
           (scan->kind == NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL ||
            scan->kind == NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL_AUTO);
}

static int record_scan_is_bank_one(const NgGameConfigRecordScan *scan) {
    return scan &&
           (scan->kind == NG_GAME_CONFIG_RECORD_SCAN_BANK_ONE ||
            scan->kind == NG_GAME_CONFIG_RECORD_SCAN_BANK_ONE_AUTO);
}

static int record_format_entry_matches(const NgProgramRom *rom,
                                       const NgGameConfigRecordFormat *record,
                                       uint32_t addr) {
    if (!rom || !record) {
        return 0;
    }
    if (!record->has_tag) {
        return 1;
    }

    uint32_t tag_addr = addr + record->tag_offset;
    return tag_addr >= addr &&
           UINT32_MAX - tag_addr >= 1u &&
           ng_program_rom_addr_is_mapped(rom, tag_addr) &&
           ng_program_rom_addr_is_mapped(rom, tag_addr + 1u) &&
           ng_program_rom_read16(rom, tag_addr) == (uint16_t)record->tag;
}

static int record_format_entry_has_valid_target(
    const NgProgramRom *rom,
    const NgGameConfigRecordFormat *record,
    uint32_t addr,
    int strict_target_shape) {
    if (!rom || !record || !record_format_entry_matches(rom, record, addr)) {
        return 0;
    }

    for (uint32_t i = 0; i < record->callback_offset_count; ++i) {
        uint32_t target_addr = addr + record->callback_offsets[i];
        if (target_addr < addr ||
            UINT32_MAX - target_addr < 3u ||
            !ng_program_rom_addr_is_mapped(rom, target_addr) ||
            !ng_program_rom_addr_is_mapped(rom, target_addr + 3u)) {
            continue;
        }

        uint32_t target = ng_program_rom_read32(rom, target_addr);
        if (record->has_sentinel && target == record->sentinel) {
            continue;
        }
        if (record_format_target_allowed(rom,
                                         record,
                                         target,
                                         strict_target_shape)) {
            return 1;
        }
    }
    return 0;
}

static void add_record_format_entry_targets(
    const NgProgramRom *rom,
    const NgGameConfigRecordFormat *record,
    uint32_t addr,
    int strict_target_shape,
    NgFunctionDiscovery *out) {
    if (!rom || !record || !out ||
        !record_format_entry_matches(rom, record, addr)) {
        return;
    }

    for (uint32_t i = 0; i < record->callback_offset_count; ++i) {
        uint32_t target_addr = addr + record->callback_offsets[i];
        if (target_addr < addr ||
            UINT32_MAX - target_addr < 3u ||
            !ng_program_rom_addr_is_mapped(rom, target_addr) ||
            !ng_program_rom_addr_is_mapped(rom, target_addr + 3u)) {
            continue;
        }

        uint32_t target = ng_program_rom_read32(rom, target_addr);
        if (record->has_sentinel && target == record->sentinel) {
            continue;
        }
        if (record_format_target_allowed(rom,
                                         record,
                                         target,
                                         strict_target_shape)) {
            ng_function_discovery_add(out, rom, target);
        }
    }
}

static void add_record_format_run_targets(
    const NgProgramRom *rom,
    const NgGameConfigRecordFormat *record,
    uint32_t run_start,
    uint32_t run_entries,
    uint32_t stride,
    int strict_target_shape,
    NgFunctionDiscovery *out) {
    if (!rom || !record || !out || run_entries == 0u || stride == 0u) {
        return;
    }

    uint32_t addr = run_start;
    for (uint32_t i = 0; i < run_entries; ++i) {
        add_record_format_entry_targets(rom,
                                        record,
                                        addr,
                                        strict_target_shape,
                                        out);
        if (UINT32_MAX - addr < stride) {
            break;
        }
        addr += stride;
    }
}

static void flush_record_format_cluster(
    const NgProgramRom *rom,
    const NgGameConfigRecordFormat *record,
    uint32_t run_start,
    uint32_t run_entries,
    uint32_t stride,
    int strict_target_shape,
    NgFunctionDiscovery *out) {
    if (run_entries >= record->cluster_min_entries &&
        (record->cluster_max_entries == 0u ||
         run_entries <= record->cluster_max_entries)) {
        add_record_format_run_targets(rom,
                                      record,
                                      run_start,
                                      run_entries,
                                      stride,
                                      strict_target_shape,
                                      out);
    }
}

static uint32_t record_format_auto_phase_step(
    const NgGameConfigRecordFormat *record,
    uint32_t start,
    uint32_t stride) {
    if (!record || (stride & 1u) != 0u || (start & 1u) != 0u) {
        return 1u;
    }
    if (record->has_tag && (record->tag_offset & 1u) != 0u) {
        return 1u;
    }
    for (uint32_t i = 0; i < record->callback_offset_count; ++i) {
        if ((record->callback_offsets[i] & 1u) != 0u) {
            return 1u;
        }
    }
    return 2u;
}

static void add_record_format_region_targets(
    const NgProgramRom *rom,
    const NgGameConfigRecordFormat *record,
    uint32_t start,
    uint32_t end,
    int strict_target_shape,
    NgFunctionDiscovery *out) {
    if (!rom || !record || !out ||
        record->callback_offset_count == 0u ||
        end <= start) {
        return;
    }

    uint32_t stride = record->stride != 0u ? record->stride : record->width;
    if (stride == 0u) {
        return;
    }

    uint32_t cluster_min_entries = record->cluster_min_entries;
    if (cluster_min_entries <= 1u) {
        cluster_min_entries = 0u;
    }
    uint32_t run_start = 0u;
    uint32_t run_entries = 0u;

    for (uint32_t addr = start; addr < end;) {
        if (record->width != 0u &&
            (UINT32_MAX - addr < record->width - 1u ||
             addr + record->width > end)) {
            break;
        }

        if (cluster_min_entries != 0u) {
            if (record_format_entry_has_valid_target(rom,
                                                     record,
                                                     addr,
                                                     strict_target_shape)) {
                if (run_entries == 0u) {
                    run_start = addr;
                }
                ++run_entries;
            } else if (run_entries != 0u) {
                flush_record_format_cluster(rom,
                                            record,
                                            run_start,
                                            run_entries,
                                            stride,
                                            strict_target_shape,
                                            out);
                run_entries = 0u;
            }
        } else {
            add_record_format_entry_targets(rom,
                                            record,
                                            addr,
                                            strict_target_shape,
                                            out);
        }

        if (UINT32_MAX - addr < stride) {
            break;
        }
        addr += stride;
    }

    if (cluster_min_entries != 0u && run_entries != 0u) {
        flush_record_format_cluster(rom,
                                    record,
                                    run_start,
                                    run_entries,
                                    stride,
                                    strict_target_shape,
                                    out);
    }
}

static void add_record_format_auto_region_targets(
    const NgProgramRom *rom,
    const NgGameConfigRecordFormat *record,
    uint32_t start,
    uint32_t end,
    NgFunctionDiscovery *out) {
    if (!rom || !record || !out ||
        record->callback_offset_count == 0u ||
        end <= start) {
        return;
    }

    uint32_t stride = record->stride != 0u ? record->stride : record->width;
    if (stride == 0u) {
        return;
    }

    NgGameConfigRecordFormat clustered = *record;
    if (clustered.cluster_min_entries <= 1u) {
        clustered.cluster_min_entries = 3u;
    }

    uint32_t phase_step =
        record_format_auto_phase_step(record, start, stride);
    for (uint32_t phase = 0u; phase < stride; phase += phase_step) {
        if (UINT32_MAX - start < phase) {
            break;
        }
        uint32_t phase_start = start + phase;
        if (phase_start >= end) {
            break;
        }
        add_record_format_region_targets(rom,
                                         &clustered,
                                         phase_start,
                                         end,
                                         1,
                                         out);
    }
}

static void add_record_format_targets(const NgProgramRom *rom,
                                      const NgGameConfigRecordFormat *record,
                                      NgFunctionDiscovery *out) {
    if (!rom || !record || !out) {
        return;
    }

    for (uint32_t i = 0; i < record->scan_count; ++i) {
        uint32_t start = 0u;
        uint32_t end = 0u;
        if (record_scan_is_bank_all(&record->scans[i])) {
            uint32_t bank_count = ng_program_rom_bank_count(rom);
            for (uint32_t bank = 0; bank < bank_count; ++bank) {
                if (!ng_program_rom_bank_is_configured(rom, bank)) {
                    continue;
                }
                NgProgramRom bank_rom = *rom;
                ng_program_rom_select_bank(&bank_rom, bank);
                if (!record_scan_bounds(&bank_rom,
                                        &record->scans[i],
                                        &start,
                                        &end)) {
                    continue;
                }
                if (record_scan_is_auto(&record->scans[i])) {
                    add_record_format_auto_region_targets(&bank_rom,
                                                          record,
                                                          start,
                                                          end,
                                                          out);
                } else {
                    add_record_format_region_targets(&bank_rom,
                                                     record,
                                                     start,
                                                     end,
                                                     0,
                                                     out);
                }
            }
            continue;
        }
        if (record_scan_is_bank_one(&record->scans[i])) {
            if (!ng_program_rom_bank_is_configured(
                    rom,
                    record->scans[i].bank_id)) {
                continue;
            }
            NgProgramRom bank_rom = *rom;
            ng_program_rom_select_bank(&bank_rom, record->scans[i].bank_id);
            if (!record_scan_bounds(&bank_rom,
                                    &record->scans[i],
                                    &start,
                                    &end)) {
                continue;
            }
            if (record_scan_is_auto(&record->scans[i])) {
                add_record_format_auto_region_targets(&bank_rom,
                                                      record,
                                                      start,
                                                      end,
                                                      out);
            } else {
                add_record_format_region_targets(&bank_rom,
                                                 record,
                                                 start,
                                                 end,
                                                 0,
                                                 out);
            }
            continue;
        }
        if (!record_scan_bounds(rom, &record->scans[i], &start, &end)) {
            continue;
        }
        if (record_scan_is_auto(&record->scans[i])) {
            add_record_format_auto_region_targets(rom, record, start, end, out);
        } else {
            add_record_format_region_targets(rom,
                                             record,
                                             start,
                                             end,
                                             0,
                                             out);
        }
    }
}

static int routine_table_entry_terminal(const NgM68kInstr *instr) {
    return instr &&
           (instr->mnemonic == NG_M68K_RTS ||
            instr->mnemonic == NG_M68K_RTE ||
            instr->mnemonic == NG_M68K_RTR ||
            instr->mnemonic == NG_M68K_BRA ||
            instr->mnemonic == NG_M68K_JMP);
}

static int routine_table_entry_valid(const NgProgramRom *rom,
                                     const NgGameConfigRoutineTable *table,
                                     uint32_t addr,
                                     uint32_t width) {
    if (!rom || !table || width == 0u || (addr & 1u) != 0u) {
        return 0;
    }
    if (UINT32_MAX - addr < width ||
        !ng_program_rom_addr_is_mapped(rom, addr)) {
        return 0;
    }

    uint32_t pc = addr;
    uint32_t limit = addr + width;
    uint32_t decoded = 0u;
    uint32_t min_instructions =
        table->min_instructions == 0u ? 1u : table->min_instructions;
    while (pc < limit) {
        NgM68kInstr instr;
        if (!ng_program_rom_addr_is_mapped(rom, pc) ||
            !ng_m68k_decode(rom, pc, &instr) ||
            instr.byte_length == 0u ||
            !ng_m68k_validate(&instr) ||
            instr.mnemonic == NG_M68K_UNKNOWN ||
            instr.mnemonic == NG_M68K_INVALID ||
            UINT32_MAX - pc < instr.byte_length - 1u ||
            pc + instr.byte_length > limit ||
            !ng_program_rom_addr_is_mapped(rom,
                                           pc + instr.byte_length - 1u)) {
            return 0;
        }

        ++decoded;
        if (routine_table_entry_terminal(&instr)) {
            return decoded >= min_instructions;
        }

        pc += instr.byte_length;
        if (table->has_fallthrough_target &&
            pc == table->fallthrough_target) {
            return decoded >= min_instructions &&
                   is_probable_function_target(rom, pc);
        }
    }

    return 0;
}

static void add_routine_table_region_targets(
    const NgProgramRom *rom,
    const NgGameConfigRoutineTable *table,
    uint32_t start,
    uint32_t end,
    NgFunctionDiscovery *out) {
    if (!rom || !table || !out ||
        table->stride == 0u ||
        end <= start) {
        return;
    }

    uint32_t width = table->width != 0u ? table->width : table->stride;
    if (width == 0u || width > table->stride) {
        return;
    }

    for (uint32_t addr = start; addr < end;) {
        if (UINT32_MAX - addr < width || addr + width > end) {
            break;
        }
        if (routine_table_entry_valid(rom, table, addr, width)) {
            ng_function_discovery_add(out, rom, addr);
        }
        if (UINT32_MAX - addr < table->stride) {
            break;
        }
        addr += table->stride;
    }
}

static void add_routine_table_targets(const NgProgramRom *rom,
                                      const NgGameConfigRoutineTable *table,
                                      NgFunctionDiscovery *out) {
    if (!rom || !table || !out) {
        return;
    }

    for (uint32_t i = 0; i < table->scan_count; ++i) {
        uint32_t start = 0u;
        uint32_t end = 0u;
        if (table->scans[i].kind == NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL) {
            uint32_t bank_count = ng_program_rom_bank_count(rom);
            for (uint32_t bank = 0; bank < bank_count; ++bank) {
                if (!ng_program_rom_bank_is_configured(rom, bank)) {
                    continue;
                }
                NgProgramRom bank_rom = *rom;
                ng_program_rom_select_bank(&bank_rom, bank);
                if (!record_scan_bounds(&bank_rom,
                                        &table->scans[i],
                                        &start,
                                        &end)) {
                    continue;
                }
                add_routine_table_region_targets(&bank_rom,
                                                 table,
                                                 start,
                                                 end,
                                                 out);
            }
            continue;
        }
        if (table->scans[i].kind == NG_GAME_CONFIG_RECORD_SCAN_BANK_ONE) {
            if (!ng_program_rom_bank_is_configured(
                    rom,
                    table->scans[i].bank_id)) {
                continue;
            }
            NgProgramRom bank_rom = *rom;
            ng_program_rom_select_bank(&bank_rom, table->scans[i].bank_id);
            if (!record_scan_bounds(&bank_rom,
                                    &table->scans[i],
                                    &start,
                                    &end)) {
                continue;
            }
            add_routine_table_region_targets(&bank_rom,
                                             table,
                                             start,
                                             end,
                                             out);
            continue;
        }
        if (!record_scan_bounds(rom, &table->scans[i], &start, &end)) {
            continue;
        }
        add_routine_table_region_targets(rom, table, start, end, out);
    }
}

static void discovery_entry_rom_view(const NgProgramRom *rom,
                                     const NgFunctionDiscovery *discovery,
                                     uint32_t index,
                                     NgProgramRom *out) {
    if (!rom || !out) {
        return;
    }
    *out = *rom;
    uint32_t bank = ng_function_discovery_bank_at(discovery, index);
    if (bank != NG_FUNCTION_DISCOVERY_BANK_NONE) {
        ng_program_rom_select_bank(out, bank);
    }
}

static void scan_function_candidate(const NgProgramRom *rom,
                                    uint32_t start_addr,
                                    const NgGameConfig *config,
                                    NgFunctionDiscovery *out) {
    uint32_t pc = start_addr;
    NgM68kInstr previous;
    int have_previous = 0;
    NgM68kStaticAregState areg;
    NgM68kStaticAregState previous_areg;
    uint8_t dreg_valid[8];
    uint32_t dreg_target[8];
    ng_m68k_static_areg_reset(&areg);
    ng_m68k_static_areg_reset(&previous_areg);
    static_dreg_reset(dreg_valid);
    memset(dreg_target, 0, sizeof(dreg_target));

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
            ng_function_discovery_add_label(out, rom, pc);
        }

        if (have_previous) {
            NgM68kJumpTablePattern pattern;
            if (ng_m68k_match_pc_index_jump_table(&previous, &instr, &pattern)) {
                add_jump_table_targets(rom, &pattern, out);
            } else if (ng_m68k_match_static_index_jump_table(&previous,
                                                             &instr,
                                                             &previous_areg,
                                                             &pattern)) {
                add_jump_table_targets(rom, &pattern, out);
            } else if (ng_m68k_match_static_index_branch_table(&instr,
                                                               &areg,
                                                               &pattern)) {
                add_jump_table_targets(rom, &pattern, out);
            } else {
                uint32_t target = 0;
                if (is_task_state_store(rom,
                                        config,
                                        &previous,
                                        &instr,
                                        &target)) {
                    ng_function_discovery_add(out, rom, target);
                } else if (is_task_spawn_call(rom,
                                              config,
                                              &previous,
                                              &instr,
                                              &target)) {
                    ng_function_discovery_add(out, rom, target);
                } else {
                    add_abs_pointer_indirect_targets(rom,
                                                     start_addr,
                                                     &previous,
                                                     &instr,
                                                     out);
                }
            }
        }
        {
            NgM68kJumpTablePattern pattern;
            if (have_previous &&
                ng_m68k_match_pc_index_inline_code_table(rom,
                                                          &previous,
                                                          &instr,
                                                          &pattern)) {
                add_jump_table_targets(rom, &pattern, out);
            } else if (ng_m68k_match_pc_index_branch_table(rom,
                                                           &instr,
                                                           &pattern)) {
                add_jump_table_targets(rom, &pattern, out);
            } else if (ng_m68k_match_repeated_direct_dispatch_table(rom,
                                                                    &instr,
                                                                    &pattern)) {
                add_jump_table_targets(rom, &pattern, out);
            }
        }

        add_config_table_call_targets(rom, config, &instr, &areg, out);
        add_static_areg_indirect_target(rom, start_addr, &instr, &areg, out);

        if (is_direct_function_target(&instr)) {
            ng_function_discovery_add(out, rom, instr.target);
        }
        if (is_branch_target_candidate(&instr)) {
            ng_function_discovery_add(out, rom, instr.target);
            if ((instr.target & 0x00FFFFFFu) > pc) {
                add_branch_target_static_indirects(rom,
                                                   start_addr,
                                                   instr.target,
                                                   &areg,
                                                   dreg_valid,
                                                   dreg_target,
                                                   out);
            }
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

        previous_areg = areg;
        ng_m68k_static_areg_update(&areg, &instr);
        static_dreg_to_areg_update(&areg,
                                   dreg_valid,
                                   dreg_target,
                                   &instr);
        static_dreg_update(dreg_valid, dreg_target, &instr);

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
    return ng_function_discover_from_game_config_limited(
        rom,
        seeds,
        seed_count,
        config,
        NG_FUNCTION_DISCOVERY_MAX_CANDIDATES,
        out);
}

int ng_function_discover_from_game_config_limited(
    const NgProgramRom *rom,
    const uint32_t *seeds,
    uint32_t seed_count,
    const NgGameConfig *config,
    uint32_t max_candidates,
    NgFunctionDiscovery *out) {
    if (!rom || !out) {
        return 0;
    }

    ng_function_discovery_init(out);
    ng_function_discovery_set_max_candidates(out, max_candidates);

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
        for (uint32_t i = 0; i < config->state_table_count; ++i) {
            add_state_table_targets_with_scans(rom,
                                               &config->state_tables[i],
                                               out);
        }
        for (uint32_t i = 0; i < config->record_format_count; ++i) {
            add_record_format_targets(rom, &config->record_formats[i], out);
        }
        for (uint32_t i = 0; i < config->routine_table_count; ++i) {
            add_routine_table_targets(rom, &config->routine_tables[i], out);
        }
        for (uint32_t i = 0; i < config->jump_table_count; ++i) {
            if (config->jump_tables[i].format ==
                NG_GAME_CONFIG_JUMP_TABLE_SCRIPT_PREDICATE) {
                add_script_predicate_targets(rom, &config->jump_tables[i], out);
            } else if (config->jump_tables[i].format ==
                       NG_GAME_CONFIG_JUMP_TABLE_TAGGED_ABS32) {
                add_tagged_abs32_targets(rom, &config->jump_tables[i], out);
            } else if (config->jump_tables[i].format ==
                       NG_GAME_CONFIG_JUMP_TABLE_INLINE_CALLBACK) {
                add_inline_callback_targets(rom, &config->jump_tables[i], out);
            } else {
                add_config_jump_table_targets(rom, &config->jump_tables[i], out);
            }
        }
    }
    if (out->count == 0u) {
        return 0;
    }

    for (uint32_t i = 0; i < out->count; ++i) {
        NgProgramRom view;
        discovery_entry_rom_view(rom, out, i, &view);
        scan_function_candidate(&view, out->addrs[i], config, out);
    }
    return 1;
}
