#include "dispatch_audit.h"

#include "m68k_analyze.h"
#include "m68k_validate.h"
#include "neogeo_map.h"

#include <stdlib.h>
#include <string.h>

#define NG_DISPATCH_AUDIT_MAX_SEEN NG_FUNCTION_DISCOVERY_MAX_CANDIDATES
#define NG_DISPATCH_AUDIT_BRANCH_TABLE_MAX_ENTRIES 32u
#define NG_DISPATCH_AUDIT_RECENT_INSTRUCTIONS 8u

void ng_dispatch_audit_init(NgDispatchAudit *audit) {
    if (audit) {
        memset(audit, 0, sizeof(*audit));
    }
}

static int instr_is_direct_target(const NgM68kInstr *instr) {
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

static int instr_is_dispatch(const NgM68kInstr *instr) {
    return instr->mnemonic == NG_M68K_JSR ||
           instr->mnemonic == NG_M68K_BSR ||
           instr->mnemonic == NG_M68K_JMP;
}

static int instr_stops_scan(const NgM68kInstr *instr) {
    switch (instr->mnemonic) {
    case NG_M68K_BRA:
    case NG_M68K_ILLEGAL:
    case NG_M68K_JMP:
    case NG_M68K_RTE:
    case NG_M68K_RTR:
    case NG_M68K_RTS:
    case NG_M68K_STOP:
    case NG_M68K_TRAP:
        return 1;
    default:
        return 0;
    }
}

static uint32_t audit_bank_for_addr(const NgProgramRom *rom, uint32_t addr) {
    if (rom &&
        ng_program_rom_bank_count(rom) > 1u &&
        ng_program_rom_addr_is_banked(rom, addr)) {
        return rom->active_bank;
    }
    return NG_FUNCTION_DISCOVERY_BANK_NONE;
}

static int addr_seen_bank(const uint32_t *seen,
                          const uint32_t *seen_banks,
                          uint32_t count,
                          uint32_t addr,
                          uint32_t bank) {
    for (uint32_t i = 0; i < count; ++i) {
        if (seen[i] == addr && seen_banks[i] == bank) {
            return 1;
        }
    }
    return 0;
}

static int runtime_dispatch_allowed(const NgGameConfig *config, uint32_t addr) {
    if (!config) {
        return 0;
    }
    addr &= 0x00FFFFFFu;
    for (uint32_t i = 0; i < config->runtime_dispatch_count; ++i) {
        if ((config->runtime_dispatch[i] & 0x00FFFFFFu) == addr) {
            return 1;
        }
    }
    return 0;
}

static int dispatcher_has_object_state(const NgGameConfig *config) {
    if (!config) {
        return 0;
    }
    for (uint32_t i = 0; i < config->dispatcher_count; ++i) {
        if (config->dispatchers[i].kind ==
            NG_GAME_CONFIG_DISPATCHER_OBJECT_STATE) {
            return 1;
        }
    }
    return 0;
}

static int dispatcher_slot_allowed(const NgGameConfig *config, uint32_t slot) {
    if (!config) {
        return 0;
    }
    for (uint32_t i = 0; i < config->dispatcher_count; ++i) {
        const NgGameConfigDispatcher *dispatcher = &config->dispatchers[i];
        if (dispatcher->kind != NG_GAME_CONFIG_DISPATCHER_OBJECT_STATE) {
            continue;
        }
        if (dispatcher->has_state_slot && dispatcher->state_slot == slot) {
            return 1;
        }
        for (uint32_t j = 0; j < dispatcher->install_slot_count; ++j) {
            if (dispatcher->install_slots[j] == slot) {
                return 1;
            }
        }
    }
    return 0;
}

static int ea_is_a6_slot(const NgM68kEa *ea, uint32_t *slot) {
    if (!ea || !slot || ea->reg != 6u) {
        return 0;
    }
    if (ea->mode == NG_M68K_EA_AIND) {
        *slot = 0u;
        return 1;
    }
    if (ea->mode == NG_M68K_EA_ADISP) {
        *slot = (uint32_t)(uint16_t)ea->displacement;
        return 1;
    }
    return 0;
}

static int instr_writes_areg(const NgM68kInstr *instr, uint8_t areg) {
    return instr &&
           instr->dst.mode == NG_M68K_EA_AREG &&
           instr->dst.reg == areg;
}

static int instr_loads_areg_from_allowed_a6_slot(
    const NgGameConfig *config,
    const NgM68kInstr *instr,
    uint8_t areg) {
    uint32_t slot = 0u;
    return instr &&
           instr->mnemonic == NG_M68K_MOVEA &&
           instr->dst.mode == NG_M68K_EA_AREG &&
           instr->dst.reg == areg &&
           ea_is_a6_slot(&instr->src, &slot) &&
           dispatcher_slot_allowed(config, slot);
}

static int instr_stores_areg_to_allowed_a6_slot(
    const NgGameConfig *config,
    const NgM68kInstr *instr,
    uint8_t areg) {
    uint32_t slot = 0u;
    return instr &&
           instr->mnemonic == NG_M68K_MOVE &&
           instr->size == 4u &&
           instr->src.mode == NG_M68K_EA_AREG &&
           instr->src.reg == areg &&
           ea_is_a6_slot(&instr->dst, &slot) &&
           dispatcher_slot_allowed(config, slot);
}

static int instr_is_areg_self_deref(const NgM68kInstr *instr, uint8_t areg) {
    return instr &&
           instr->mnemonic == NG_M68K_MOVEA &&
           instr->src.mode == NG_M68K_EA_AIND &&
           instr->src.reg == areg &&
           instr->dst.mode == NG_M68K_EA_AREG &&
           instr->dst.reg == areg;
}

static int recent_has_dispatcher_slot_context(
    const NgGameConfig *config,
    const NgM68kInstr *recent,
    uint32_t recent_count,
    uint8_t areg) {
    for (uint32_t i = recent_count; i > 0u; --i) {
        const NgM68kInstr *instr = &recent[i - 1u];
        if (instr_loads_areg_from_allowed_a6_slot(config, instr, areg) ||
            instr_stores_areg_to_allowed_a6_slot(config, instr, areg)) {
            return 1;
        }
    }
    return 0;
}

static int dispatcher_runtime_dispatch_allowed(
    const NgGameConfig *config,
    const NgM68kInstr *instr,
    const NgM68kInstr *recent,
    uint32_t recent_count) {
    if (!dispatcher_has_object_state(config) ||
        !instr ||
        instr->src.mode != NG_M68K_EA_AIND) {
        return 0;
    }

    uint8_t areg = instr->src.reg;
    for (uint32_t i = recent_count; i > 0u; --i) {
        const NgM68kInstr *prev = &recent[i - 1u];
        if (!instr_writes_areg(prev, areg)) {
            continue;
        }
        if (instr_loads_areg_from_allowed_a6_slot(config, prev, areg)) {
            return 1;
        }
        if (instr_is_areg_self_deref(prev, areg)) {
            return recent_has_dispatcher_slot_context(config,
                                                      recent,
                                                      i - 1u,
                                                      areg);
        }
        return 0;
    }
    return 0;
}

static NgDispatchAuditSite *audit_append(const NgProgramRom *rom,
                                         NgDispatchAudit *audit,
                                         NgDispatchAuditKind kind,
                                         const NgM68kInstr *instr) {
    if (audit->count >= NG_DISPATCH_AUDIT_MAX_SITES) {
        audit->truncated = 1;
        return NULL;
    }

    NgDispatchAuditSite *site = &audit->sites[audit->count++];
    memset(site, 0, sizeof(*site));
    site->kind = kind;
    site->site_addr = instr->addr & 0x00FFFFFFu;
    site->site_bank = audit_bank_for_addr(rom, instr->addr);
    site->site_banked =
        site->site_bank != NG_FUNCTION_DISCOVERY_BANK_NONE ? 1u : 0u;
    site->mnemonic = (uint8_t)instr->mnemonic;
    return site;
}

static void audit_record_direct(const NgProgramRom *rom,
                                const NgFunctionDiscovery *discovery,
                                NgDispatchAudit *audit,
                                const NgM68kInstr *instr) {
    NgDispatchAuditSite *site =
        audit_append(rom, audit, NG_DISPATCH_AUDIT_DIRECT, instr);
    ++audit->direct_count;
    if (!site) {
        return;
    }

    site->target_known = 1u;
    site->target_addr = instr->target & 0x00FFFFFFu;
    site->target_bank = audit_bank_for_addr(rom, instr->target);
    site->target_banked =
        site->target_bank != NG_FUNCTION_DISCOVERY_BANK_NONE ? 1u : 0u;

    NgAddressRegion region = ng_address_region(instr->target);
    site->target_external = region != NG_REGION_P_ROM_FIXED &&
                            region != NG_REGION_P_ROM_BANK;
    if (site->target_external) {
        ++audit->external_direct_count;
        return;
    }

    site->target_in_discovery =
        ng_program_rom_addr_is_mapped(rom, instr->target) &&
        ng_function_discovery_contains_for_rom(discovery, rom, instr->target);
    if (!site->target_in_discovery) {
        ++audit->missing_direct_count;
    }
}

static void audit_record_computed(const NgProgramRom *rom,
                                  const NgGameConfig *config,
                                  NgDispatchAudit *audit,
                                  const NgM68kInstr *instr,
                                  const NgM68kInstr *recent,
                                  uint32_t recent_count) {
    NgDispatchAuditSite *site =
        audit_append(rom, audit, NG_DISPATCH_AUDIT_COMPUTED, instr);
    if (site) {
        site->target_known = 0u;
        site->runtime_allowed =
            (uint8_t)(runtime_dispatch_allowed(config, instr->addr) ||
                      dispatcher_runtime_dispatch_allowed(config,
                                                          instr,
                                                          recent,
                                                          recent_count));
    }
    if (site && site->runtime_allowed) {
        ++audit->runtime_computed_count;
    } else {
        ++audit->computed_count;
    }
}

static void audit_record_jump_table(const NgProgramRom *rom,
                                    const NgFunctionDiscovery *discovery,
                                    NgDispatchAudit *audit,
                                    const NgM68kInstr *instr,
                                    const NgM68kJumpTablePattern *pattern) {
    NgDispatchAuditSite *site =
        audit_append(rom, audit, NG_DISPATCH_AUDIT_JUMP_TABLE, instr);
    ++audit->jump_table_count;
    if (!site) {
        return;
    }

    site->table_addr = pattern->table_addr & 0x00FFFFFFu;
    site->table_bank = audit_bank_for_addr(rom, pattern->table_addr);
    site->table_banked =
        site->table_bank != NG_FUNCTION_DISCOVERY_BANK_NONE ? 1u : 0u;
    if (pattern->entry_kind == NG_M68K_JUMP_TABLE_ENTRY_ADDRESS ||
        pattern->entry_kind == NG_M68K_JUMP_TABLE_ENTRY_INLINE_CODE ||
        pattern->entry_kind == NG_M68K_JUMP_TABLE_ENTRY_DIRECT_DISPATCH) {
        uint32_t entry_addr = pattern->table_addr;
        uint32_t stride = pattern->entry_size;
        for (uint32_t i = 0;
             i < NG_DISPATCH_AUDIT_BRANCH_TABLE_MAX_ENTRIES;
             ++i) {
            if (pattern->entry_kind == NG_M68K_JUMP_TABLE_ENTRY_ADDRESS) {
                NgM68kInstr entry;
                if (!ng_m68k_decode(rom, entry_addr, &entry) ||
                    !ng_m68k_validate(&entry) ||
                    entry.mnemonic != NG_M68K_BRA ||
                    entry.byte_length == 0u) {
                    if (pattern->include_terminal_entry) {
                        if (ng_function_discovery_contains_for_rom(discovery,
                                                                   rom,
                                                                   entry_addr)) {
                            ++site->resolved_entries;
                            ++audit->jump_table_resolved_entries;
                        } else {
                            ++site->missing_entries;
                            ++audit->jump_table_missing_entries;
                        }
                    }
                    break;
                }
                if (stride == 0u) {
                    stride = entry.byte_length;
                } else if (entry.byte_length != stride) {
                    break;
                }
            } else if (pattern->entry_kind ==
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
                    !instr_is_direct_target(&entry)) {
                    break;
                }
            }
            if (ng_function_discovery_contains_for_rom(discovery,
                                                       rom,
                                                       entry_addr)) {
                ++site->resolved_entries;
                ++audit->jump_table_resolved_entries;
            } else {
                ++site->missing_entries;
                ++audit->jump_table_missing_entries;
            }
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
        if (ng_program_rom_addr_is_mapped(rom, target) &&
            ng_function_discovery_contains_for_rom(discovery, rom, target)) {
            ++site->resolved_entries;
            ++audit->jump_table_resolved_entries;
        } else {
            ++site->missing_entries;
            ++audit->jump_table_missing_entries;
        }
    }
}

static int audit_jump_table_has_entries(const NgProgramRom *rom,
                                        const NgM68kJumpTablePattern *pattern) {
    if (!rom || !pattern) {
        return 0;
    }
    if (pattern->entry_kind == NG_M68K_JUMP_TABLE_ENTRY_ADDRESS) {
        NgM68kInstr entry;
        return ng_m68k_decode(rom, pattern->table_addr, &entry) &&
               ng_m68k_validate(&entry) &&
               entry.mnemonic == NG_M68K_BRA &&
               entry.byte_length != 0u;
    }
    if (pattern->entry_kind == NG_M68K_JUMP_TABLE_ENTRY_INLINE_CODE) {
        return pattern->entry_size != 0u &&
               ng_program_rom_addr_is_mapped(rom, pattern->table_addr) &&
               ng_program_rom_addr_is_mapped(rom, pattern->table_addr + 3u) &&
               ng_program_rom_read32(rom, pattern->table_addr) ==
                   pattern->entry_signature;
    }
    if (pattern->entry_kind == NG_M68K_JUMP_TABLE_ENTRY_DIRECT_DISPATCH) {
        NgM68kInstr entry;
        return pattern->entry_size != 0u &&
               ng_m68k_decode(rom, pattern->table_addr, &entry) &&
               ng_m68k_validate(&entry) &&
               entry.mnemonic == NG_M68K_JMP &&
               entry.byte_length == pattern->entry_size &&
               instr_is_direct_target(&entry);
    }
    return pattern->entry_size != 0u &&
           ng_program_rom_addr_is_mapped(rom, pattern->table_addr) &&
           ng_program_rom_addr_is_mapped(rom,
                                         pattern->table_addr +
                                             pattern->entry_size - 1u);
}

static void audit_scan_from(const NgProgramRom *rom,
                            const NgFunctionDiscovery *discovery,
                            const NgGameConfig *config,
                            NgDispatchAudit *audit,
                            uint32_t start_addr,
                            uint32_t *seen,
                            uint32_t *seen_banks,
                            uint32_t *seen_count) {
    uint32_t pc = start_addr;
    NgM68kInstr previous;
    int have_previous = 0;
    NgM68kInstr recent[NG_DISPATCH_AUDIT_RECENT_INSTRUCTIONS];
    uint32_t recent_count = 0;
    NgM68kStaticAregState areg;
    NgM68kStaticAregState previous_areg;
    ng_m68k_static_areg_reset(&areg);
    ng_m68k_static_areg_reset(&previous_areg);

    for (uint32_t i = 0; i < NG_FUNCTION_DISCOVERY_MAX_INSTRUCTIONS; ++i) {
        NgM68kInstr instr;
        if (!ng_m68k_decode(rom, pc, &instr)) {
            return;
        }
        if (instr.byte_length == 0u ||
            !ng_m68k_validate(&instr) ||
            instr.mnemonic == NG_M68K_UNKNOWN ||
            instr.mnemonic == NG_M68K_INVALID) {
            return;
        }

        uint32_t pc_bank = audit_bank_for_addr(rom, pc);
        if (!addr_seen_bank(seen, seen_banks, *seen_count, pc, pc_bank)) {
            if (*seen_count < NG_DISPATCH_AUDIT_MAX_SEEN) {
                seen[*seen_count] = pc;
                seen_banks[*seen_count] = pc_bank;
                ++(*seen_count);
            } else {
                audit->truncated = 1;
            }

            NgM68kJumpTablePattern pattern;
            int matched_table = 0;
            if (have_previous) {
                matched_table =
                    ng_m68k_match_pc_index_jump_table(&previous,
                                                      &instr,
                                                      &pattern) ||
                    ng_m68k_match_static_index_jump_table(&previous,
                                                          &instr,
                                                          &previous_areg,
                                                          &pattern) ||
                    ng_m68k_match_static_index_branch_table(&instr,
                                                            &areg,
                                                            &pattern) ||
                    ng_m68k_match_pc_index_inline_code_table(rom,
                                                              &previous,
                                                              &instr,
                                                              &pattern);
            }
            if (!matched_table) {
                matched_table =
                    ng_m68k_match_pc_index_branch_table(rom,
                                                        &instr,
                                                        &pattern);
            }
            if (!matched_table) {
                matched_table =
                    ng_m68k_match_repeated_direct_dispatch_table(rom,
                                                                 &instr,
                                                                 &pattern);
            }
            if (matched_table &&
                audit_jump_table_has_entries(rom, &pattern)) {
                audit_record_jump_table(rom, discovery, audit, &instr, &pattern);
            } else if (instr_is_direct_target(&instr)) {
                audit_record_direct(rom, discovery, audit, &instr);
            } else if (instr_is_dispatch(&instr)) {
                audit_record_computed(rom,
                                      config,
                                      audit,
                                      &instr,
                                      recent,
                                      recent_count);
            }
        }

        if (instr_stops_scan(&instr)) {
            return;
        }

        previous_areg = areg;
        ng_m68k_static_areg_update(&areg, &instr);
        if (recent_count < NG_DISPATCH_AUDIT_RECENT_INSTRUCTIONS) {
            recent[recent_count++] = instr;
        } else {
            memmove(&recent[0],
                    &recent[1],
                    sizeof(recent[0]) *
                        (NG_DISPATCH_AUDIT_RECENT_INSTRUCTIONS - 1u));
            recent[NG_DISPATCH_AUDIT_RECENT_INSTRUCTIONS - 1u] = instr;
        }
        pc += instr.byte_length;
        previous = instr;
        have_previous = 1;
    }
}

int ng_dispatch_audit_build_with_config(const NgProgramRom *rom,
                                        const NgFunctionDiscovery *discovery,
                                        const NgGameConfig *config,
                                        NgDispatchAudit *audit) {
    if (!rom || !discovery || !audit) {
        return 0;
    }

    ng_dispatch_audit_init(audit);
    uint32_t *seen =
        (uint32_t *)calloc(NG_DISPATCH_AUDIT_MAX_SEEN, sizeof(*seen));
    uint32_t *seen_banks =
        (uint32_t *)calloc(NG_DISPATCH_AUDIT_MAX_SEEN, sizeof(*seen_banks));
    uint32_t seen_count = 0;
    if (!seen || !seen_banks) {
        free(seen);
        free(seen_banks);
        return 0;
    }
    for (uint32_t i = 0; i < NG_DISPATCH_AUDIT_MAX_SEEN; ++i) {
        seen_banks[i] = NG_FUNCTION_DISCOVERY_BANK_NONE;
    }

    for (uint32_t i = 0; i < discovery->count; ++i) {
        NgProgramRom view = *rom;
        uint32_t bank = ng_function_discovery_bank_at(discovery, i);
        if (bank != NG_FUNCTION_DISCOVERY_BANK_NONE) {
            ng_program_rom_select_bank(&view, bank);
        }
        audit_scan_from(&view,
                        discovery,
                        config,
                        audit,
                        discovery->addrs[i],
                        seen,
                        seen_banks,
                        &seen_count);
    }
    free(seen);
    free(seen_banks);
    return 1;
}

int ng_dispatch_audit_build(const NgProgramRom *rom,
                            const NgFunctionDiscovery *discovery,
                            NgDispatchAudit *audit) {
    return ng_dispatch_audit_build_with_config(rom, discovery, NULL, audit);
}

int ng_dispatch_audit_has_gaps(const NgDispatchAudit *audit) {
    if (!audit) {
        return 0;
    }
    return audit->missing_direct_count != 0u ||
           audit->computed_count != 0u ||
           audit->jump_table_missing_entries != 0u ||
           audit->truncated;
}

int ng_dispatch_audit_write(FILE *out, const NgDispatchAudit *audit) {
    if (!out || !audit) {
        return 0;
    }

    fprintf(out,
            "dispatch audit: sites=%u direct=%u missing_direct=%u external_direct=%u computed=%u runtime_computed=%u jump_tables=%u table_resolved=%u table_missing=%u%s\n",
            audit->count,
            audit->direct_count,
            audit->missing_direct_count,
            audit->external_direct_count,
            audit->computed_count,
            audit->runtime_computed_count,
            audit->jump_table_count,
            audit->jump_table_resolved_entries,
            audit->jump_table_missing_entries,
            audit->truncated ? " truncated" : "");

    for (uint32_t i = 0; i < audit->count; ++i) {
        const NgDispatchAuditSite *site = &audit->sites[i];
        const char *mnemonic =
            ng_m68k_mnemonic_name((NgM68kMnemonic)site->mnemonic);
        switch (site->kind) {
        case NG_DISPATCH_AUDIT_DIRECT:
            fprintf(out,
                    "$%06X%s DIRECT %s target=$%06X%s discovered=%s%s\n",
                    site->site_addr & 0x00FFFFFFu,
                    site->site_banked ? " banked=yes" : "",
                    mnemonic,
                    site->target_addr & 0x00FFFFFFu,
                    site->target_banked ? " banked=yes" : "",
                    site->target_in_discovery ? "yes" : "no",
                    site->target_external ? " external=yes" : "");
            break;
        case NG_DISPATCH_AUDIT_COMPUTED:
            fprintf(out,
                    "$%06X%s COMPUTED %s target=<runtime>%s\n",
                    site->site_addr & 0x00FFFFFFu,
                    site->site_banked ? " banked=yes" : "",
                    mnemonic,
                    site->runtime_allowed ? " allowed=yes" : "");
            break;
        case NG_DISPATCH_AUDIT_JUMP_TABLE:
            fprintf(out,
                    "$%06X%s JUMP_TABLE %s table=$%06X%s resolved=%u missing=%u\n",
                    site->site_addr & 0x00FFFFFFu,
                    site->site_banked ? " banked=yes" : "",
                    mnemonic,
                    site->table_addr & 0x00FFFFFFu,
                    site->table_banked ? " banked=yes" : "",
                    site->resolved_entries,
                    site->missing_entries);
            break;
        default:
            return 0;
        }
    }

    return ferror(out) == 0;
}

static const char *suggestion_action_for_kind(NgDispatchAuditKind kind) {
    switch (kind) {
    case NG_DISPATCH_AUDIT_DIRECT:
        return "add a seed or describe the data structure that references this target";
    case NG_DISPATCH_AUDIT_COMPUTED:
        return "model this dispatcher/interpreter shape or keep it as an explicit runtime residual";
    case NG_DISPATCH_AUDIT_JUMP_TABLE:
        return "extend discovery for this table shape or add a bounded descriptor";
    default:
        return "review manually";
    }
}

static const char *suggestion_kind_for_site(const NgDispatchAuditSite *site) {
    if (!site) {
        return "unknown";
    }
    switch (site->kind) {
    case NG_DISPATCH_AUDIT_DIRECT:
        return "missing_direct";
    case NG_DISPATCH_AUDIT_COMPUTED:
        return "computed_dispatch";
    case NG_DISPATCH_AUDIT_JUMP_TABLE:
        return "jump_table_missing";
    default:
        return "unknown";
    }
}

static int site_needs_suggestion(const NgDispatchAuditSite *site) {
    if (!site) {
        return 0;
    }
    switch (site->kind) {
    case NG_DISPATCH_AUDIT_DIRECT:
        return site->target_known &&
               !site->target_external &&
               !site->target_in_discovery;
    case NG_DISPATCH_AUDIT_COMPUTED:
        return !site->runtime_allowed;
    case NG_DISPATCH_AUDIT_JUMP_TABLE:
        return site->missing_entries != 0u;
    default:
        return 0;
    }
}

int ng_dispatch_audit_write_suggestions(FILE *out,
                                        const NgDispatchAudit *audit) {
    if (!out || !audit) {
        return 0;
    }

    uint32_t suggestion_count = 0u;
    for (uint32_t i = 0; i < audit->count; ++i) {
        if (site_needs_suggestion(&audit->sites[i])) {
            ++suggestion_count;
        }
    }

    fprintf(out,
            "# Machine-readable dispatch discovery suggestions.\n"
            "# These are generic diagnostics, not auto-applied config.\n"
            "suggestion_count = %u\n"
            "truncated = %s\n\n",
            suggestion_count,
            audit->truncated ? "true" : "false");

    for (uint32_t i = 0; i < audit->count; ++i) {
        const NgDispatchAuditSite *site = &audit->sites[i];
        if (!site_needs_suggestion(site)) {
            continue;
        }

        const char *mnemonic =
            ng_m68k_mnemonic_name((NgM68kMnemonic)site->mnemonic);
        fprintf(out,
                "[[suggestion]]\n"
                "kind = \"%s\"\n"
                "site = 0x%06X\n"
                "mnemonic = \"%s\"\n",
                suggestion_kind_for_site(site),
                site->site_addr & 0x00FFFFFFu,
                mnemonic);
        if (site->site_banked) {
            fprintf(out, "site_bank = %u\n", (unsigned)site->site_bank);
        }

        if (site->kind == NG_DISPATCH_AUDIT_DIRECT) {
            fprintf(out, "target = 0x%06X\n",
                    site->target_addr & 0x00FFFFFFu);
            if (site->target_banked) {
                fprintf(out,
                        "target_bank = %u\n",
                        (unsigned)site->target_bank);
            }
        } else if (site->kind == NG_DISPATCH_AUDIT_JUMP_TABLE) {
            fprintf(out,
                    "table = 0x%06X\n"
                    "missing_entries = %u\n"
                    "resolved_entries = %u\n",
                    site->table_addr & 0x00FFFFFFu,
                    site->missing_entries,
                    site->resolved_entries);
            if (site->table_banked) {
                fprintf(out,
                        "table_bank = %u\n",
                        (unsigned)site->table_bank);
            }
        }

        fprintf(out,
                "action = \"%s\"\n\n",
                suggestion_action_for_kind(site->kind));
    }

    return ferror(out) == 0;
}
