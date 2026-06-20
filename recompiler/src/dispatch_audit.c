#include "dispatch_audit.h"

#include "m68k_analyze.h"
#include "m68k_validate.h"
#include "neogeo_map.h"

#include <stdlib.h>
#include <string.h>

#define NG_DISPATCH_AUDIT_MAX_SEEN NG_FUNCTION_DISCOVERY_MAX_CANDIDATES

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

static int addr_seen(const uint32_t *seen, uint32_t count, uint32_t addr) {
    for (uint32_t i = 0; i < count; ++i) {
        if (seen[i] == addr) {
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

static NgDispatchAuditSite *audit_append(NgDispatchAudit *audit,
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
    site->mnemonic = (uint8_t)instr->mnemonic;
    return site;
}

static void audit_record_direct(const NgProgramRom *rom,
                                const NgFunctionDiscovery *discovery,
                                NgDispatchAudit *audit,
                                const NgM68kInstr *instr) {
    NgDispatchAuditSite *site =
        audit_append(audit, NG_DISPATCH_AUDIT_DIRECT, instr);
    ++audit->direct_count;
    if (!site) {
        return;
    }

    site->target_known = 1u;
    site->target_addr = instr->target & 0x00FFFFFFu;

    NgAddressRegion region = ng_address_region(instr->target);
    site->target_external = region != NG_REGION_P_ROM_FIXED &&
                            region != NG_REGION_P_ROM_BANK;
    if (site->target_external) {
        ++audit->external_direct_count;
        return;
    }

    site->target_in_discovery =
        ng_program_rom_addr_is_mapped(rom, instr->target) &&
        ng_function_discovery_contains(discovery, instr->target);
    if (!site->target_in_discovery) {
        ++audit->missing_direct_count;
    }
}

static void audit_record_computed(const NgGameConfig *config,
                                  NgDispatchAudit *audit,
                                  const NgM68kInstr *instr) {
    NgDispatchAuditSite *site =
        audit_append(audit, NG_DISPATCH_AUDIT_COMPUTED, instr);
    if (site) {
        site->target_known = 0u;
        site->runtime_allowed =
            (uint8_t)runtime_dispatch_allowed(config, instr->addr);
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
        audit_append(audit, NG_DISPATCH_AUDIT_JUMP_TABLE, instr);
    ++audit->jump_table_count;
    if (!site) {
        return;
    }

    site->table_addr = pattern->table_addr & 0x00FFFFFFu;
    for (uint32_t i = 0; i < NG_FUNCTION_DISCOVERY_TABLE_ENTRIES; ++i) {
        uint32_t entry_addr = pattern->table_addr + i * pattern->entry_size;
        if (!ng_program_rom_addr_is_mapped(rom, entry_addr) ||
            !ng_program_rom_addr_is_mapped(rom, entry_addr + 3u)) {
            break;
        }
        uint32_t target = ng_program_rom_read32(rom, entry_addr);
        if (ng_program_rom_addr_is_mapped(rom, target) &&
            ng_function_discovery_contains(discovery, target)) {
            ++site->resolved_entries;
            ++audit->jump_table_resolved_entries;
        } else {
            ++site->missing_entries;
            ++audit->jump_table_missing_entries;
        }
    }
}

static void audit_scan_from(const NgProgramRom *rom,
                            const NgFunctionDiscovery *discovery,
                            const NgGameConfig *config,
                            NgDispatchAudit *audit,
                            uint32_t start_addr,
                            uint32_t *seen,
                            uint32_t *seen_count) {
    uint32_t pc = start_addr;
    NgM68kInstr previous;
    int have_previous = 0;

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

        if (!addr_seen(seen, *seen_count, pc)) {
            if (*seen_count < NG_DISPATCH_AUDIT_MAX_SEEN) {
                seen[(*seen_count)++] = pc;
            } else {
                audit->truncated = 1;
            }

            NgM68kJumpTablePattern pattern;
            if (have_previous &&
                ng_m68k_match_pc_index_jump_table(&previous, &instr, &pattern)) {
                audit_record_jump_table(rom, discovery, audit, &instr, &pattern);
            } else if (instr_is_direct_target(&instr)) {
                audit_record_direct(rom, discovery, audit, &instr);
            } else if (instr_is_dispatch(&instr)) {
                audit_record_computed(config, audit, &instr);
            }
        }

        if (instr_stops_scan(&instr)) {
            return;
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
    uint32_t seen_count = 0;
    if (!seen) {
        return 0;
    }

    for (uint32_t i = 0; i < discovery->count; ++i) {
        audit_scan_from(rom, discovery, config, audit, discovery->addrs[i],
                        seen, &seen_count);
    }
    free(seen);
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
                    "$%06X DIRECT %s target=$%06X discovered=%s%s\n",
                    site->site_addr & 0x00FFFFFFu,
                    mnemonic,
                    site->target_addr & 0x00FFFFFFu,
                    site->target_in_discovery ? "yes" : "no",
                    site->target_external ? " external=yes" : "");
            break;
        case NG_DISPATCH_AUDIT_COMPUTED:
            fprintf(out,
                    "$%06X COMPUTED %s target=<runtime>%s\n",
                    site->site_addr & 0x00FFFFFFu,
                    mnemonic,
                    site->runtime_allowed ? " allowed=yes" : "");
            break;
        case NG_DISPATCH_AUDIT_JUMP_TABLE:
            fprintf(out,
                    "$%06X JUMP_TABLE %s table=$%06X resolved=%u missing=%u\n",
                    site->site_addr & 0x00FFFFFFu,
                    mnemonic,
                    site->table_addr & 0x00FFFFFFu,
                    site->resolved_entries,
                    site->missing_entries);
            break;
        default:
            return 0;
        }
    }

    return ferror(out) == 0;
}
