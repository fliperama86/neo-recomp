#include "function_discovery.h"
#include "game_config.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ORACLE_ROM_SIZE 0x0A00u
#define ORACLE_FIXED_BASE 0x000000u
#define ORACLE_FIXED_SIZE 0x000800u
#define ORACLE_BANK_WINDOW_BASE 0x200000u
#define ORACLE_BANK_WINDOW_SIZE 0x000100u
#define ORACLE_BANK0_OFFSET 0x000800u
#define ORACLE_BANK1_OFFSET 0x000900u

#define ORACLE_VECTOR_PC 0x000004u
#define ORACLE_CART_HEADER 0x000100u
#define ORACLE_CART_ENTRY_PTR 0x000124u
#define ORACLE_ENTRY 0x000200u
#define ORACLE_DIRECT_FUNC 0x000220u
#define ORACLE_OBJECT_STATE_CALLBACK 0x000240u
#define ORACLE_SPAWN_CALLBACK 0x000260u
#define ORACLE_SPAWN_HELPER 0x000280u
#define ORACLE_STATE_CALLBACK 0x0002A0u
#define ORACLE_CHAINED_STATE_CALLBACK 0x0002C0u
#define ORACLE_TAGGED_RECORD_CALLBACK 0x0002E0u
#define ORACLE_STATE_TABLE 0x000300u
#define ORACLE_STATE_TABLE_NEXT 0x000310u
#define ORACLE_TAGGED_RECORDS 0x000320u
#define ORACLE_FIXED_RECORDS 0x000340u
#define ORACLE_FIXED_RECORD_CALLBACK 0x000380u
#define ORACLE_OBJECT_VECTOR_CALLBACK_A 0x0003A0u
#define ORACLE_OBJECT_VECTOR_CALLBACK_B 0x0003C0u
#define ORACLE_WRONG_TAG_DATA 0x0003E0u
#define ORACLE_ROUTINE_ENTRY_A 0x000400u
#define ORACLE_ROUTINE_ENTRY_B 0x000410u
#define ORACLE_ROUTINE_SHARED_TAIL 0x000430u
#define ORACLE_OBJECT_VECTOR 0x000440u
#define ORACLE_VECTOR_DATA 0x000460u
#define ORACLE_BANK_RECORD 0x200000u
#define ORACLE_BANK_CALLBACK 0x200020u

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

typedef struct OracleExtent {
    const char *name;
    uint32_t start;
    uint32_t end;
    uint32_t bank;
} OracleExtent;

typedef struct OracleReloc {
    const char *name;
    uint32_t site;
    uint32_t target;
    uint32_t bank;
    int targets_code;
} OracleReloc;

typedef struct DiscoveryRow {
    uint32_t addr;
    uint32_t bank;
} DiscoveryRow;

static NgProgramRom make_rom(uint32_t size) {
    NgProgramRom rom;
    memset(&rom, 0, sizeof(rom));
    rom.size = size;
    rom.data = (uint8_t *)calloc(size ? size : 1u, 1);
    return rom;
}

static void write16(NgProgramRom *rom, uint32_t addr, uint16_t value) {
    rom->data[addr] = (uint8_t)(value >> 8);
    rom->data[addr + 1u] = (uint8_t)value;
}

static void write32(NgProgramRom *rom, uint32_t addr, uint32_t value) {
    write16(rom, addr, (uint16_t)(value >> 16));
    write16(rom, addr + 2u, (uint16_t)value);
}

static void write_jsr_abs(NgProgramRom *rom, uint32_t addr, uint32_t target) {
    write16(rom, addr, 0x4EB9u);
    write32(rom, addr + 2u, target);
}

static void write_lea_abs_a1(NgProgramRom *rom,
                             uint32_t addr,
                             uint32_t target) {
    write16(rom, addr, 0x43F9u);
    write32(rom, addr + 2u, target);
}

static void write_bra_w(NgProgramRom *rom, uint32_t addr, uint32_t target) {
    write16(rom, addr, 0x6000u);
    write16(rom, addr + 2u, (uint16_t)(target - (addr + 2u)));
}

static void write_nop_rts(NgProgramRom *rom, uint32_t addr) {
    write16(rom, addr, 0x4E71u);
    write16(rom, addr + 2u, 0x4E75u);
}

static int compare_discovery_row(const void *a, const void *b) {
    const DiscoveryRow *av = (const DiscoveryRow *)a;
    const DiscoveryRow *bv = (const DiscoveryRow *)b;
    if (av->bank != bv->bank) {
        return (av->bank > bv->bank) - (av->bank < bv->bank);
    }
    return (av->addr > bv->addr) - (av->addr < bv->addr);
}

static int write_discovery_set(const char *path,
                               const NgFunctionDiscovery *discovery) {
    if (!path || !discovery) {
        return 0;
    }

    DiscoveryRow *rows = NULL;
    if (discovery->count != 0u) {
        rows = (DiscoveryRow *)malloc((size_t)discovery->count * sizeof(*rows));
        if (!rows) {
            return 0;
        }
    }
    for (uint32_t i = 0; i < discovery->count; ++i) {
        rows[i].addr = discovery->addrs[i] & 0x00FFFFFFu;
        rows[i].bank = ng_function_discovery_bank_at(discovery, i);
    }
    qsort(rows, discovery->count, sizeof(*rows), compare_discovery_row);

    FILE *out = fopen(path, "w");
    if (!out) {
        free(rows);
        return 0;
    }
    for (uint32_t i = 0; i < discovery->count; ++i) {
        if (rows[i].bank != NG_FUNCTION_DISCOVERY_BANK_NONE) {
            fprintf(out, "bank:%u 0x%06X\n", (unsigned)rows[i].bank, rows[i].addr);
        } else {
            fprintf(out, "0x%06X\n", rows[i].addr);
        }
    }
    int ok = fclose(out) == 0;
    free(rows);
    return ok;
}

static int extent_contains(const OracleExtent *extent,
                           uint32_t addr,
                           uint32_t bank) {
    return extent &&
           extent->bank == bank &&
           addr >= extent->start &&
           addr < extent->end;
}

static int any_extent_contains(const OracleExtent *extents,
                               uint32_t extent_count,
                               uint32_t addr,
                               uint32_t bank) {
    for (uint32_t i = 0; i < extent_count; ++i) {
        if (extent_contains(&extents[i], addr, bank)) {
            return 1;
        }
    }
    return 0;
}

static int oracle_symbol_discovered(const NgFunctionDiscovery *discovery,
                                    const OracleExtent *extent) {
    return ng_function_discovery_contains_bank(discovery,
                                               extent->start,
                                               extent->bank);
}

static int oracle_reloc_discovered(const NgFunctionDiscovery *discovery,
                                   const OracleReloc *reloc) {
    return ng_function_discovery_contains_bank(discovery,
                                               reloc->target,
                                               reloc->bank);
}

static void configure_oracle(NgGameConfig *config) {
    ng_game_config_init(config);

    config->entry_count = 1u;
    config->entry[0] = ORACLE_ENTRY;

    config->dispatcher_count = 1u;
    config->dispatchers[0].kind = NG_GAME_CONFIG_DISPATCHER_OBJECT_STATE;
    config->dispatchers[0].state_slot = 0x70u;
    config->dispatchers[0].has_state_slot = 1;
    config->dispatchers[0].install_slot_count = 1u;
    config->dispatchers[0].install_slots[0] = 0x70u;
    config->dispatchers[0].spawn_helper_count = 1u;
    config->dispatchers[0].spawn_helpers[0] = ORACLE_SPAWN_HELPER;

    config->state_table_count = 1u;
    config->state_tables[0].root = ORACLE_STATE_TABLE;
    config->state_tables[0].table_start = ORACLE_STATE_TABLE;
    config->state_tables[0].table_end = ORACLE_STATE_TABLE + 0x20u;
    config->state_tables[0].stride = 4u;
    config->state_tables[0].sentinel = 0xFFFFFFFFu;
    config->state_tables[0].follow_chain = 1;
    config->state_tables[0].target_start = ORACLE_STATE_CALLBACK;
    config->state_tables[0].target_end = ORACLE_TAGGED_RECORD_CALLBACK;
    config->state_tables[0].max_tables = 4u;
    config->state_tables[0].max_entries = 4u;

    config->record_format_count = 4u;
    snprintf(config->record_formats[0].name,
             sizeof(config->record_formats[0].name),
             "oracle_tagged_records");
    config->record_formats[0].tag = 0x0800u;
    config->record_formats[0].has_tag = 1;
    config->record_formats[0].stride = 2u;
    config->record_formats[0].callback_offset_count = 1u;
    config->record_formats[0].callback_offsets[0] = 2u;
    config->record_formats[0].target_start = ORACLE_TAGGED_RECORD_CALLBACK;
    config->record_formats[0].target_end = ORACLE_ROUTINE_ENTRY_A;
    config->record_formats[0].scan_count = 1u;
    config->record_formats[0].scans[0].kind =
        NG_GAME_CONFIG_RECORD_SCAN_RANGE;
    config->record_formats[0].scans[0].start = ORACLE_TAGGED_RECORDS;
    config->record_formats[0].scans[0].end = ORACLE_TAGGED_RECORDS + 0x20u;

    snprintf(config->record_formats[1].name,
             sizeof(config->record_formats[1].name),
             "oracle_fixed_records");
    config->record_formats[1].width = 0x10u;
    config->record_formats[1].callback_offset_count = 1u;
    config->record_formats[1].callback_offsets[0] = 4u;
    config->record_formats[1].sentinel = 0xFFFFFFFFu;
    config->record_formats[1].has_sentinel = 1;
    config->record_formats[1].target_start = ORACLE_FIXED_RECORD_CALLBACK;
    config->record_formats[1].target_end = ORACLE_ROUTINE_ENTRY_A;
    config->record_formats[1].scan_count = 1u;
    config->record_formats[1].scans[0].kind =
        NG_GAME_CONFIG_RECORD_SCAN_RANGE;
    config->record_formats[1].scans[0].start = ORACLE_FIXED_RECORDS;
    config->record_formats[1].scans[0].end = ORACLE_FIXED_RECORDS + 0x20u;

    snprintf(config->record_formats[2].name,
             sizeof(config->record_formats[2].name),
             "oracle_object_vector");
    config->record_formats[2].stride = 4u;
    config->record_formats[2].callback_offset_count = 1u;
    config->record_formats[2].callback_offsets[0] = 0u;
    config->record_formats[2].sentinel = 0xFFFFFFFFu;
    config->record_formats[2].has_sentinel = 1;
    config->record_formats[2].cluster_min_entries = 2u;
    config->record_formats[2].target_start = ORACLE_OBJECT_VECTOR_CALLBACK_A;
    config->record_formats[2].target_end = ORACLE_ROUTINE_ENTRY_A;
    config->record_formats[2].scan_count = 1u;
    config->record_formats[2].scans[0].kind =
        NG_GAME_CONFIG_RECORD_SCAN_RANGE;
    config->record_formats[2].scans[0].start = ORACLE_OBJECT_VECTOR;
    config->record_formats[2].scans[0].end = ORACLE_OBJECT_VECTOR + 0x10u;

    snprintf(config->record_formats[3].name,
             sizeof(config->record_formats[3].name),
             "oracle_banked_record");
    config->record_formats[3].stride = 4u;
    config->record_formats[3].callback_offset_count = 1u;
    config->record_formats[3].callback_offsets[0] = 0u;
    config->record_formats[3].target_start = ORACLE_BANK_WINDOW_BASE;
    config->record_formats[3].target_end =
        ORACLE_BANK_WINDOW_BASE + ORACLE_BANK_WINDOW_SIZE;
    config->record_formats[3].scan_count = 1u;
    config->record_formats[3].scans[0].kind =
        NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL;

    config->routine_table_count = 1u;
    snprintf(config->routine_tables[0].name,
             sizeof(config->routine_tables[0].name),
             "oracle_routine_table");
    config->routine_tables[0].stride = 0x10u;
    config->routine_tables[0].width = 0x10u;
    config->routine_tables[0].min_instructions = 2u;
    config->routine_tables[0].scan_count = 1u;
    config->routine_tables[0].scans[0].kind =
        NG_GAME_CONFIG_RECORD_SCAN_RANGE;
    config->routine_tables[0].scans[0].start = ORACLE_ROUTINE_ENTRY_A;
    config->routine_tables[0].scans[0].end = ORACLE_ROUTINE_ENTRY_A + 0x20u;
}

static void write_cart_header(NgProgramRom *rom) {
    write32(rom, 0x000000u, 0x0010F300u);
    write32(rom, ORACLE_VECTOR_PC, ORACLE_ENTRY);
    memcpy(rom->data + ORACLE_CART_HEADER, "NEO-GEO", 7u);
    rom->data[ORACLE_CART_HEADER + 7u] = 0u;
    write16(rom, 0x000122u, 0x4EF9u);
    write32(rom, ORACLE_CART_ENTRY_PTR, ORACLE_ENTRY);
}

static void build_oracle_rom(NgProgramRom *rom) {
    memset(rom->data, 0xFF, rom->size);
    write_cart_header(rom);

    write_jsr_abs(rom, ORACLE_ENTRY, ORACLE_DIRECT_FUNC);
    write_lea_abs_a1(rom, ORACLE_ENTRY + 0x06u, ORACLE_OBJECT_STATE_CALLBACK);
    write16(rom, ORACLE_ENTRY + 0x0Cu, 0x2D49u); /* MOVE.L A1,($70,A6) */
    write16(rom, ORACLE_ENTRY + 0x0Eu, 0x0070u);
    write_lea_abs_a1(rom, ORACLE_ENTRY + 0x10u, ORACLE_SPAWN_CALLBACK);
    write_jsr_abs(rom, ORACLE_ENTRY + 0x16u, ORACLE_SPAWN_HELPER);
    write16(rom, ORACLE_ENTRY + 0x1Cu, 0x4E75u);

    write_nop_rts(rom, ORACLE_DIRECT_FUNC);
    write_nop_rts(rom, ORACLE_OBJECT_STATE_CALLBACK);
    write_nop_rts(rom, ORACLE_SPAWN_CALLBACK);
    write16(rom, ORACLE_SPAWN_HELPER, 0x4E75u);
    write_nop_rts(rom, ORACLE_STATE_CALLBACK);
    write_nop_rts(rom, ORACLE_CHAINED_STATE_CALLBACK);
    write_nop_rts(rom, ORACLE_TAGGED_RECORD_CALLBACK);
    write_nop_rts(rom, ORACLE_FIXED_RECORD_CALLBACK);
    write_nop_rts(rom, ORACLE_OBJECT_VECTOR_CALLBACK_A);
    write_nop_rts(rom, ORACLE_OBJECT_VECTOR_CALLBACK_B);
    write16(rom, ORACLE_WRONG_TAG_DATA, 0x4E75u);
    write16(rom, ORACLE_VECTOR_DATA, 0x4E75u);

    write32(rom, ORACLE_STATE_TABLE, ORACLE_STATE_CALLBACK);
    write32(rom, ORACLE_STATE_TABLE + 0x04u, ORACLE_STATE_TABLE_NEXT);
    write32(rom, ORACLE_STATE_TABLE + 0x08u, 0xFFFFFFFFu);
    write32(rom, ORACLE_STATE_TABLE + 0x0Cu, 0xDEAD0000u);
    write32(rom, ORACLE_STATE_TABLE_NEXT, ORACLE_CHAINED_STATE_CALLBACK);
    write32(rom, ORACLE_STATE_TABLE_NEXT + 0x04u, 0xDEAD0000u);

    write16(rom, ORACLE_TAGGED_RECORDS, 0x0800u);
    write32(rom, ORACLE_TAGGED_RECORDS + 0x02u, ORACLE_TAGGED_RECORD_CALLBACK);
    write16(rom, ORACLE_TAGGED_RECORDS + 0x10u, 0x0700u);
    write32(rom, ORACLE_TAGGED_RECORDS + 0x12u, ORACLE_WRONG_TAG_DATA);

    write32(rom, ORACLE_FIXED_RECORDS + 0x04u, ORACLE_FIXED_RECORD_CALLBACK);
    write32(rom, ORACLE_FIXED_RECORDS + 0x14u, 0xFFFFFFFFu);

    write16(rom, ORACLE_ROUTINE_ENTRY_A, 0x4E71u);
    write_jsr_abs(rom, ORACLE_ROUTINE_ENTRY_A + 0x02u, ORACLE_DIRECT_FUNC);
    write16(rom, ORACLE_ROUTINE_ENTRY_A + 0x08u, 0x4E75u);
    write16(rom, ORACLE_ROUTINE_ENTRY_B, 0x4E71u);
    write16(rom, ORACLE_ROUTINE_ENTRY_B + 0x02u, 0x4E71u);
    write_bra_w(rom, ORACLE_ROUTINE_ENTRY_B + 0x04u, ORACLE_ROUTINE_SHARED_TAIL);
    write16(rom, ORACLE_ROUTINE_SHARED_TAIL, 0x4E71u);
    write16(rom, ORACLE_ROUTINE_SHARED_TAIL + 0x02u, 0x4E75u);

    write32(rom, ORACLE_OBJECT_VECTOR, ORACLE_OBJECT_VECTOR_CALLBACK_A);
    write32(rom, ORACLE_OBJECT_VECTOR + 0x04u, ORACLE_OBJECT_VECTOR_CALLBACK_B);
    write32(rom, ORACLE_OBJECT_VECTOR + 0x08u, ORACLE_VECTOR_DATA);
    write32(rom, ORACLE_OBJECT_VECTOR + 0x0Cu, 0xFFFFFFFFu);

    write32(rom, ORACLE_BANK0_OFFSET, ORACLE_BANK_CALLBACK);
    write_nop_rts(rom, ORACLE_BANK0_OFFSET + 0x20u);
    write32(rom, ORACLE_BANK1_OFFSET, ORACLE_BANK_CALLBACK);
    write_nop_rts(rom, ORACLE_BANK1_OFFSET + 0x20u);
}

int main(int argc, char **argv) {
    const char *emit_discovery_set = NULL;
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--emit-discovery-set") == 0 && i + 1 < argc) {
            emit_discovery_set = argv[++i];
        } else {
            fprintf(stderr, "usage: %s [--emit-discovery-set OUT]\n", argv[0]);
            return 2;
        }
    }

    static const OracleExtent extents[] = {
        {"entry", ORACLE_ENTRY, ORACLE_ENTRY + 0x1Eu,
         NG_FUNCTION_DISCOVERY_BANK_NONE},
        {"direct_func", ORACLE_DIRECT_FUNC, ORACLE_DIRECT_FUNC + 0x04u,
         NG_FUNCTION_DISCOVERY_BANK_NONE},
        {"object_state_callback", ORACLE_OBJECT_STATE_CALLBACK,
         ORACLE_OBJECT_STATE_CALLBACK + 0x04u, NG_FUNCTION_DISCOVERY_BANK_NONE},
        {"spawn_callback", ORACLE_SPAWN_CALLBACK, ORACLE_SPAWN_CALLBACK + 0x04u,
         NG_FUNCTION_DISCOVERY_BANK_NONE},
        {"spawn_helper", ORACLE_SPAWN_HELPER, ORACLE_SPAWN_HELPER + 0x02u,
         NG_FUNCTION_DISCOVERY_BANK_NONE},
        {"state_table_callback", ORACLE_STATE_CALLBACK,
         ORACLE_STATE_CALLBACK + 0x04u, NG_FUNCTION_DISCOVERY_BANK_NONE},
        {"chained_state_callback", ORACLE_CHAINED_STATE_CALLBACK,
         ORACLE_CHAINED_STATE_CALLBACK + 0x04u, NG_FUNCTION_DISCOVERY_BANK_NONE},
        {"tagged_record_callback", ORACLE_TAGGED_RECORD_CALLBACK,
         ORACLE_TAGGED_RECORD_CALLBACK + 0x04u, NG_FUNCTION_DISCOVERY_BANK_NONE},
        {"fixed_record_callback", ORACLE_FIXED_RECORD_CALLBACK,
         ORACLE_FIXED_RECORD_CALLBACK + 0x04u, NG_FUNCTION_DISCOVERY_BANK_NONE},
        {"object_vector_callback_a", ORACLE_OBJECT_VECTOR_CALLBACK_A,
         ORACLE_OBJECT_VECTOR_CALLBACK_A + 0x04u, NG_FUNCTION_DISCOVERY_BANK_NONE},
        {"object_vector_callback_b", ORACLE_OBJECT_VECTOR_CALLBACK_B,
         ORACLE_OBJECT_VECTOR_CALLBACK_B + 0x04u, NG_FUNCTION_DISCOVERY_BANK_NONE},
        {"routine_table_entry_a", ORACLE_ROUTINE_ENTRY_A,
         ORACLE_ROUTINE_ENTRY_A + 0x0Au, NG_FUNCTION_DISCOVERY_BANK_NONE},
        {"routine_table_entry_b", ORACLE_ROUTINE_ENTRY_B,
         ORACLE_ROUTINE_ENTRY_B + 0x08u, NG_FUNCTION_DISCOVERY_BANK_NONE},
        {"routine_table_shared_tail", ORACLE_ROUTINE_SHARED_TAIL,
         ORACLE_ROUTINE_SHARED_TAIL + 0x04u, NG_FUNCTION_DISCOVERY_BANK_NONE},
        {"bank0_callback", ORACLE_BANK_CALLBACK, ORACLE_BANK_CALLBACK + 0x04u, 0u},
        {"bank1_callback", ORACLE_BANK_CALLBACK, ORACLE_BANK_CALLBACK + 0x04u, 1u},
    };
    static const OracleReloc relocs[] = {
        {"vector_entry", ORACLE_VECTOR_PC, ORACLE_ENTRY,
         NG_FUNCTION_DISCOVERY_BANK_NONE, 1},
        {"cart_header_entry", ORACLE_CART_ENTRY_PTR, ORACLE_ENTRY,
         NG_FUNCTION_DISCOVERY_BANK_NONE, 1},
        {"direct_jsr", ORACLE_ENTRY + 0x02u, ORACLE_DIRECT_FUNC,
         NG_FUNCTION_DISCOVERY_BANK_NONE, 1},
        {"object_state_install", ORACLE_ENTRY + 0x08u,
         ORACLE_OBJECT_STATE_CALLBACK, NG_FUNCTION_DISCOVERY_BANK_NONE, 1},
        {"spawn_install", ORACLE_ENTRY + 0x12u, ORACLE_SPAWN_CALLBACK,
         NG_FUNCTION_DISCOVERY_BANK_NONE, 1},
        {"spawn_helper_call", ORACLE_ENTRY + 0x18u, ORACLE_SPAWN_HELPER,
         NG_FUNCTION_DISCOVERY_BANK_NONE, 1},
        {"routine_table_call", ORACLE_ROUTINE_ENTRY_A + 0x04u,
         ORACLE_DIRECT_FUNC, NG_FUNCTION_DISCOVERY_BANK_NONE, 1},
        {"routine_table_branch", ORACLE_ROUTINE_ENTRY_B + 0x06u,
         ORACLE_ROUTINE_SHARED_TAIL, NG_FUNCTION_DISCOVERY_BANK_NONE, 1},
        {"state_table_entry", ORACLE_STATE_TABLE, ORACLE_STATE_CALLBACK,
         NG_FUNCTION_DISCOVERY_BANK_NONE, 1},
        {"state_table_chain_pointer", ORACLE_STATE_TABLE + 0x04u,
         ORACLE_STATE_TABLE_NEXT, NG_FUNCTION_DISCOVERY_BANK_NONE, 0},
        {"state_table_chain_entry", ORACLE_STATE_TABLE_NEXT,
         ORACLE_CHAINED_STATE_CALLBACK, NG_FUNCTION_DISCOVERY_BANK_NONE, 1},
        {"tagged_record_entry", ORACLE_TAGGED_RECORDS + 0x02u,
         ORACLE_TAGGED_RECORD_CALLBACK, NG_FUNCTION_DISCOVERY_BANK_NONE, 1},
        {"fixed_record_entry", ORACLE_FIXED_RECORDS + 0x04u,
         ORACLE_FIXED_RECORD_CALLBACK, NG_FUNCTION_DISCOVERY_BANK_NONE, 1},
        {"object_vector_a", ORACLE_OBJECT_VECTOR,
         ORACLE_OBJECT_VECTOR_CALLBACK_A, NG_FUNCTION_DISCOVERY_BANK_NONE, 1},
        {"object_vector_b", ORACLE_OBJECT_VECTOR + 0x04u,
         ORACLE_OBJECT_VECTOR_CALLBACK_B, NG_FUNCTION_DISCOVERY_BANK_NONE, 1},
        {"wrong_tag_data_pointer", ORACLE_TAGGED_RECORDS + 0x12u,
         ORACLE_WRONG_TAG_DATA, NG_FUNCTION_DISCOVERY_BANK_NONE, 0},
        {"object_vector_data_pointer", ORACLE_OBJECT_VECTOR + 0x08u,
         ORACLE_VECTOR_DATA, NG_FUNCTION_DISCOVERY_BANK_NONE, 0},
        {"bank0_record_entry", ORACLE_BANK_RECORD, ORACLE_BANK_CALLBACK, 0u, 1},
        {"bank1_record_entry", ORACLE_BANK_RECORD, ORACLE_BANK_CALLBACK, 1u, 1},
    };

    NgProgramRom rom = make_rom(ORACLE_ROM_SIZE);
    CHECK(rom.data != NULL);
    build_oracle_rom(&rom);
    ng_program_rom_set_address_map(&rom,
                                   ORACLE_FIXED_BASE,
                                   ORACLE_FIXED_SIZE,
                                   ORACLE_BANK_WINDOW_BASE,
                                   ORACLE_BANK_WINDOW_SIZE);
    CHECK(ng_program_rom_configure_bank(&rom, 0u, ORACLE_BANK0_OFFSET,
                                        ORACLE_BANK_WINDOW_SIZE));
    CHECK(ng_program_rom_configure_bank(&rom, 1u, ORACLE_BANK1_OFFSET,
                                        ORACLE_BANK_WINDOW_SIZE));

    NgGameConfig config;
    configure_oracle(&config);

    NgFunctionDiscovery discovery;
    const uint32_t seed = ORACLE_ENTRY;
    CHECK(ng_function_discover_from_game_config(&rom,
                                                &seed,
                                                1u,
                                                &config,
                                                &discovery));
    CHECK(!discovery.truncated);

    for (uint32_t i = 0; i < sizeof(extents) / sizeof(extents[0]); ++i) {
        if (!oracle_symbol_discovered(&discovery, &extents[i])) {
            fprintf(stderr,
                    "oracle symbol missing: %s bank=%u addr=0x%06X\n",
                    extents[i].name,
                    extents[i].bank,
                    extents[i].start);
            return 1;
        }
    }

    for (uint32_t i = 0; i < sizeof(relocs) / sizeof(relocs[0]); ++i) {
        int discovered = oracle_reloc_discovered(&discovery, &relocs[i]);
        if (relocs[i].targets_code && !discovered) {
            fprintf(stderr,
                    "oracle code relocation missing: %s target=0x%06X\n",
                    relocs[i].name,
                    relocs[i].target);
            return 1;
        }
        if (!relocs[i].targets_code && discovered) {
            fprintf(stderr,
                    "oracle data relocation discovered as code: %s target=0x%06X\n",
                    relocs[i].name,
                    relocs[i].target);
            return 1;
        }
    }

    for (uint32_t i = 0; i < discovery.count; ++i) {
        uint32_t bank = ng_function_discovery_bank_at(&discovery, i);
        if (!any_extent_contains(extents,
                                 (uint32_t)(sizeof(extents) /
                                            sizeof(extents[0])),
                                 discovery.addrs[i],
                                 bank)) {
            fprintf(stderr,
                    "oracle soundness failure: bank=%u addr=0x%06X\n",
                    bank,
                    discovery.addrs[i]);
            return 1;
        }
    }

    if (emit_discovery_set && !write_discovery_set(emit_discovery_set,
                                                   &discovery)) {
        fprintf(stderr, "cannot write discovery set: %s\n", emit_discovery_set);
        return 1;
    }

    ng_program_rom_free(&rom);
    return 0;
}
