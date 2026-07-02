#include "dispatch_audit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static NgProgramRom make_rom(uint32_t size) {
    NgProgramRom rom;
    memset(&rom, 0, sizeof(rom));
    rom.size = size;
    rom.data = (uint8_t *)calloc(size ? size : 1u, 1);
    return rom;
}

static void write16(NgProgramRom *rom, uint32_t addr, uint16_t value) {
    rom->data[addr] = (uint8_t)(value >> 8);
    rom->data[addr + 1] = (uint8_t)value;
}

static void write32(NgProgramRom *rom, uint32_t addr, uint32_t value) {
    write16(rom, addr, (uint16_t)(value >> 16));
    write16(rom, addr + 2u, (uint16_t)value);
}

static int read_file(FILE *f, char *buf, size_t buf_size) {
    long size;
    rewind(f);
    if (fseek(f, 0, SEEK_END) != 0) return 0;
    size = ftell(f);
    if (size < 0 || (size_t)size >= buf_size) return 0;
    rewind(f);
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) return 0;
    buf[size] = '\0';
    return 1;
}

int main(void) {
    NgProgramRom rom = make_rom(0xC0u);
    CHECK(rom.data != NULL);

    write16(&rom, 0x10u, 0x4EB9u); /* JSR $000080 */
    write32(&rom, 0x12u, 0x00000080u);
    write16(&rom, 0x16u, 0x207Bu); /* MOVEA.L (4,PC,D0.W),A0 */
    write16(&rom, 0x18u, 0x0004u);
    write16(&rom, 0x1Au, 0x4ED0u); /* JMP (A0) via table at $1C */
    write32(&rom, 0x1Cu, 0x00000090u);
    write32(&rom, 0x20u, 0x000000A0u);
    write32(&rom, 0x24u, 0x00000300u); /* unmapped target */
    write32(&rom, 0x28u, 0x000000B0u);

    write16(&rom, 0x60u, 0x4E91u); /* JSR (A1), computed */
    write16(&rom, 0x62u, 0x4E75u);
    write16(&rom, 0x68u, 0x4EBBu); /* JSR (4,PC,D0.W), computed */
    write16(&rom, 0x6Au, 0x0004u);
    write16(&rom, 0x6Cu, 0x4E75u);
    write16(&rom, 0x70u, 0x4EB9u); /* JSR unmapped $000300 */
    write32(&rom, 0x72u, 0x00000300u);
    write16(&rom, 0x76u, 0x4E75u);
    write16(&rom, 0x78u, 0x4EB9u); /* JSR BIOS/system ROM target */
    write32(&rom, 0x7Au, 0x00C00444u);
    write16(&rom, 0x7Eu, 0x4E75u);

    write16(&rom, 0x80u, 0x4E75u);
    write16(&rom, 0x90u, 0x4E75u);
    write16(&rom, 0xA0u, 0x4E75u);
    write16(&rom, 0xB0u, 0x4E75u);

    const uint32_t seeds[] = {0x10u, 0x60u, 0x68u, 0x70u, 0x78u};
    static NgFunctionDiscovery discovery;
    CHECK(ng_function_discover_from_seeds(&rom, seeds, 5u, &discovery));

    static NgDispatchAudit audit;
    CHECK(ng_dispatch_audit_build(&rom, &discovery, &audit));
    CHECK(audit.direct_count == 3u);
    CHECK(audit.missing_direct_count == 1u);
    CHECK(audit.external_direct_count == 1u);
    CHECK(audit.computed_count == 2u);
    CHECK(audit.runtime_computed_count == 0u);
    CHECK(audit.jump_table_count == 1u);
    CHECK(audit.jump_table_resolved_entries == 3u);
    CHECK(audit.jump_table_missing_entries == 0u);
    CHECK(ng_dispatch_audit_has_gaps(&audit));
    CHECK(!audit.truncated);

    CHECK(audit.count == 6u);
    CHECK(audit.sites[0].kind == NG_DISPATCH_AUDIT_DIRECT);
    CHECK(audit.sites[0].site_addr == 0x10u);
    CHECK(audit.sites[0].target_addr == 0x80u);
    CHECK(audit.sites[0].target_in_discovery);

    CHECK(audit.sites[1].kind == NG_DISPATCH_AUDIT_JUMP_TABLE);
    CHECK(audit.sites[1].site_addr == 0x1Au);
    CHECK(audit.sites[1].table_addr == 0x1Cu);
    CHECK(audit.sites[1].resolved_entries == 3u);
    CHECK(audit.sites[1].missing_entries == 0u);

    CHECK(audit.sites[2].kind == NG_DISPATCH_AUDIT_COMPUTED);
    CHECK(audit.sites[2].site_addr == 0x60u);
    CHECK(!audit.sites[2].target_known);

    CHECK(audit.sites[3].kind == NG_DISPATCH_AUDIT_COMPUTED);
    CHECK(audit.sites[3].site_addr == 0x68u);
    CHECK(!audit.sites[3].target_known);

    CHECK(audit.sites[4].kind == NG_DISPATCH_AUDIT_DIRECT);
    CHECK(audit.sites[4].site_addr == 0x70u);
    CHECK(audit.sites[4].target_addr == 0x300u);
    CHECK(!audit.sites[4].target_in_discovery);
    CHECK(!audit.sites[4].target_external);

    CHECK(audit.sites[5].kind == NG_DISPATCH_AUDIT_DIRECT);
    CHECK(audit.sites[5].site_addr == 0x78u);
    CHECK(audit.sites[5].target_addr == 0xC00444u);
    CHECK(!audit.sites[5].target_in_discovery);
    CHECK(audit.sites[5].target_external);

    FILE *out = tmpfile();
    char text[4096];
    CHECK(out != NULL);
    CHECK(ng_dispatch_audit_write(out, &audit));
    CHECK(read_file(out, text, sizeof(text)));
    fclose(out);
    CHECK(strstr(text, "dispatch audit: sites=6 direct=3 missing_direct=1 external_direct=1 computed=2 runtime_computed=0 jump_tables=1") != NULL);
    CHECK(strstr(text, "$000010 DIRECT JSR target=$000080 discovered=yes") != NULL);
    CHECK(strstr(text, "$00001A JUMP_TABLE JMP table=$00001C resolved=3 missing=0") != NULL);
    CHECK(strstr(text, "$000060 COMPUTED JSR target=<runtime>") != NULL);
    CHECK(strstr(text, "$000068 COMPUTED JSR target=<runtime>") != NULL);
    CHECK(strstr(text, "$000070 DIRECT JSR target=$000300 discovered=no") != NULL);
    CHECK(strstr(text, "$000078 DIRECT JSR target=$C00444 discovered=no external=yes") != NULL);

    out = tmpfile();
    CHECK(out != NULL);
    CHECK(ng_dispatch_audit_write_suggestions(out, &audit));
    CHECK(read_file(out, text, sizeof(text)));
    fclose(out);
    CHECK(strstr(text, "suggestion_count = 3") != NULL);
    CHECK(strstr(text, "kind = \"missing_direct\"") != NULL);
    CHECK(strstr(text, "site = 0x000070") != NULL);
    CHECK(strstr(text, "target = 0x000300") != NULL);
    CHECK(strstr(text, "kind = \"computed_dispatch\"") != NULL);
    CHECK(strstr(text, "site = 0x000060") != NULL);
    CHECK(strstr(text, "site = 0x000068") != NULL);
    CHECK(strstr(text, "kind = \"jump_table_missing\"") == NULL);
    CHECK(strstr(text, "site = 0x000078") == NULL);

    audit.missing_direct_count = 0u;
    audit.computed_count = 0u;
    audit.jump_table_missing_entries = 0u;
    CHECK(!ng_dispatch_audit_has_gaps(&audit));

    NgGameConfig config;
    ng_game_config_init(&config);
    config.runtime_dispatch[config.runtime_dispatch_count++] = 0x60u;
    CHECK(ng_dispatch_audit_build_with_config(&rom, &discovery, &config, &audit));
    CHECK(audit.computed_count == 1u);
    CHECK(audit.runtime_computed_count == 1u);
    CHECK(audit.sites[2].kind == NG_DISPATCH_AUDIT_COMPUTED);
    CHECK(audit.sites[2].site_addr == 0x60u);
    CHECK(audit.sites[2].runtime_allowed);
    CHECK(audit.sites[3].kind == NG_DISPATCH_AUDIT_COMPUTED);
    CHECK(audit.sites[3].site_addr == 0x68u);
    CHECK(!audit.sites[3].runtime_allowed);

    out = tmpfile();
    CHECK(out != NULL);
    CHECK(ng_dispatch_audit_write(out, &audit));
    CHECK(read_file(out, text, sizeof(text)));
    fclose(out);
    CHECK(strstr(text, "computed=1 runtime_computed=1") != NULL);
    CHECK(strstr(text, "$000060 COMPUTED JSR target=<runtime> allowed=yes") != NULL);
    CHECK(strstr(text, "$000068 COMPUTED JSR target=<runtime>\n") != NULL);

    ng_program_rom_free(&rom);

    {
        NgProgramRom table_call_rom = make_rom(0xD0u);
        CHECK(table_call_rom.data != NULL);

        write16(&table_call_rom, 0x00u, 0x41FAu); /* LEA $000040(PC),A0 */
        write16(&table_call_rom, 0x02u, 0x003Eu);
        write16(&table_call_rom, 0x04u, 0x227Cu); /* MOVEA.L #$FFFFFFFF,A1 */
        write32(&table_call_rom, 0x06u, 0xFFFFFFFFu);
        write16(&table_call_rom, 0x0Au, 0x4EB9u); /* JSR table selector */
        write32(&table_call_rom, 0x0Cu, 0x000000C0u);
        write16(&table_call_rom, 0x10u, 0x4ED1u); /* JMP (A1), helper-selected */
        write32(&table_call_rom, 0x40u, 0x00000070u);
        write32(&table_call_rom, 0x44u, 0x00000080u);
        write16(&table_call_rom, 0x70u, 0x4E75u);
        write16(&table_call_rom, 0x80u, 0x4E75u);
        write16(&table_call_rom, 0xC0u, 0x4E75u);

        NgGameConfig table_call_config;
        ng_game_config_init(&table_call_config);
        table_call_config.table_call_count = 1u;
        table_call_config.table_calls[0].helper = 0x000000C0u;
        table_call_config.table_calls[0].table_start = 0x40u;
        table_call_config.table_calls[0].table_end = 0x50u;
        table_call_config.table_calls[0].table_reg = 0u;
        table_call_config.table_calls[0].max_entries = 2u;
        table_call_config.table_calls[0].format =
            NG_GAME_CONFIG_TABLE_CALL_ABS32_SPARSE;

        const uint32_t table_call_seeds[] = {0x00u};
        static NgFunctionDiscovery table_call_discovery;
        CHECK(ng_function_discover_from_game_config(&table_call_rom,
                                                    table_call_seeds,
                                                    1u,
                                                    &table_call_config,
                                                    &table_call_discovery));
        CHECK(ng_dispatch_audit_build_with_config(&table_call_rom,
                                                  &table_call_discovery,
                                                  &table_call_config,
                                                  &audit));
        CHECK(audit.computed_count == 0u);
        CHECK(audit.runtime_computed_count == 1u);
        CHECK(audit.sites[audit.count - 1u].kind ==
              NG_DISPATCH_AUDIT_COMPUTED);
        CHECK(audit.sites[audit.count - 1u].site_addr == 0x10u);
        CHECK(audit.sites[audit.count - 1u].runtime_allowed);

        ng_program_rom_free(&table_call_rom);
    }

    {
        NgProgramRom work_ram_rom = make_rom(0x20u);
        CHECK(work_ram_rom.data != NULL);

        write16(&work_ram_rom, 0x00u, 0x2279u); /* MOVEA.L $00100020,A1 */
        write32(&work_ram_rom, 0x02u, 0x00100020u);
        write16(&work_ram_rom, 0x06u, 0x4E91u); /* JSR (A1), RAM callback */
        write16(&work_ram_rom, 0x08u, 0x4E75u);

        static NgFunctionDiscovery work_ram_discovery;
        CHECK(ng_function_discover_from_entry(&work_ram_rom,
                                              0x00u,
                                              &work_ram_discovery));
        CHECK(ng_dispatch_audit_build(&work_ram_rom,
                                      &work_ram_discovery,
                                      &audit));
        CHECK(audit.computed_count == 0u);
        CHECK(audit.runtime_computed_count == 1u);
        CHECK(audit.sites[0].kind == NG_DISPATCH_AUDIT_COMPUTED);
        CHECK(audit.sites[0].site_addr == 0x06u);
        CHECK(audit.sites[0].runtime_allowed);

        ng_program_rom_free(&work_ram_rom);
    }

    {
        NgProgramRom inline_rom = make_rom(0x40u);
        CHECK(inline_rom.data != NULL);

        write16(&inline_rom, 0x10u, 0xDC46u);       /* ADD.W D6,D6 */
        write16(&inline_rom, 0x12u, 0x4EFBu);       /* JMP ($2,PC,D6.W) */
        write16(&inline_rom, 0x14u, 0x6002u);
        for (uint32_t addr = 0x16u; addr < 0x26u; addr += 4u) {
            write16(&inline_rom, addr, 0x2887u);    /* MOVE.L D7,(A4) */
            write16(&inline_rom, addr + 2u, 0xDE84u); /* ADD.L D4,D7 */
        }
        write16(&inline_rom, 0x26u, 0x4E75u);

        static NgFunctionDiscovery inline_discovery;
        CHECK(ng_function_discover_from_entry(&inline_rom,
                                              0x10u,
                                              &inline_discovery));
        CHECK(ng_dispatch_audit_build(&inline_rom,
                                      &inline_discovery,
                                      &audit));
        CHECK(audit.count == 1u);
        CHECK(audit.jump_table_count == 1u);
        CHECK(audit.computed_count == 0u);
        CHECK(audit.sites[0].kind == NG_DISPATCH_AUDIT_JUMP_TABLE);
        CHECK(audit.sites[0].site_addr == 0x12u);
        CHECK(audit.sites[0].table_addr == 0x16u);
        CHECK(audit.sites[0].resolved_entries == 4u);
        CHECK(audit.sites[0].missing_entries == 0u);

        ng_program_rom_free(&inline_rom);
    }

    {
        NgProgramRom bank_rom = make_rom(0x180u);
        CHECK(bank_rom.data != NULL);
        ng_program_rom_set_address_map(&bank_rom,
                                       0x000000u,
                                       0x100u,
                                       0x200000u,
                                       0x40u);

        write16(&bank_rom, 0x100u, 0x4EB9u); /* bank 0 JSR $200020 */
        write32(&bank_rom, 0x102u, 0x00200020u);
        write16(&bank_rom, 0x106u, 0x4E75u);
        write16(&bank_rom, 0x120u, 0x4E75u);
        write16(&bank_rom, 0x140u, 0x4EB9u); /* bank 1 JSR $200020 */
        write32(&bank_rom, 0x142u, 0x00200020u);
        write16(&bank_rom, 0x146u, 0x4E75u);
        write16(&bank_rom, 0x160u, 0x4E75u);

        static NgFunctionDiscovery bank_discovery;
        ng_function_discovery_init(&bank_discovery);
        NgProgramRom bank_view = bank_rom;
        ng_program_rom_select_bank(&bank_view, 0u);
        CHECK(ng_function_discovery_add(&bank_discovery,
                                        &bank_view,
                                        0x200000u));
        CHECK(ng_function_discovery_add(&bank_discovery,
                                        &bank_view,
                                        0x200020u));
        bank_view = bank_rom;
        ng_program_rom_select_bank(&bank_view, 1u);
        CHECK(ng_function_discovery_add(&bank_discovery,
                                        &bank_view,
                                        0x200000u));
        CHECK(ng_function_discovery_add(&bank_discovery,
                                        &bank_view,
                                        0x200020u));

        CHECK(ng_dispatch_audit_build(&bank_rom, &bank_discovery, &audit));
        CHECK(audit.count == 2u);
        CHECK(audit.direct_count == 2u);
        CHECK(audit.missing_direct_count == 0u);
        CHECK(audit.sites[0].site_banked);
        CHECK(audit.sites[0].site_bank == 0u);
        CHECK(audit.sites[0].target_banked);
        CHECK(audit.sites[0].target_bank == 0u);
        CHECK(audit.sites[1].site_banked);
        CHECK(audit.sites[1].site_bank == 1u);
        CHECK(audit.sites[1].target_banked);
        CHECK(audit.sites[1].target_bank == 1u);

        ng_program_rom_free(&bank_rom);
    }

    {
        NgProgramRom dispatcher_rom = make_rom(0x80u);
        CHECK(dispatcher_rom.data != NULL);

        write16(&dispatcher_rom, 0x20u, 0x2056u); /* MOVEA.L (A6),A0 */
        write16(&dispatcher_rom, 0x22u, 0x4E90u); /* JSR (A0), object dispatch */
        write16(&dispatcher_rom, 0x24u, 0x4E75u);

        write16(&dispatcher_rom, 0x30u, 0x4E91u); /* JSR (A1), not derived */
        write16(&dispatcher_rom, 0x32u, 0x4E75u);

        write16(&dispatcher_rom, 0x40u, 0x206Eu); /* MOVEA.L ($70,A6),A0 */
        write16(&dispatcher_rom, 0x42u, 0x0070u);
        write16(&dispatcher_rom, 0x44u, 0x2050u); /* MOVEA.L (A0),A0 */
        write16(&dispatcher_rom, 0x46u, 0x4ED0u); /* JMP (A0), state dispatch */

        const uint32_t dispatcher_seeds[] = {0x20u, 0x30u, 0x40u};
        static NgFunctionDiscovery dispatcher_discovery;
        CHECK(ng_function_discover_from_seeds(&dispatcher_rom,
                                              dispatcher_seeds,
                                              3u,
                                              &dispatcher_discovery));

        ng_game_config_init(&config);
        config.dispatcher_count = 1u;
        config.dispatchers[0].kind = NG_GAME_CONFIG_DISPATCHER_OBJECT_STATE;
        config.dispatchers[0].has_state_slot = 1;
        config.dispatchers[0].state_slot = 0x70u;
        config.dispatchers[0].install_slot_count = 1u;
        config.dispatchers[0].install_slots[0] = 0x00u;

        CHECK(ng_dispatch_audit_build_with_config(&dispatcher_rom,
                                                  &dispatcher_discovery,
                                                  &config,
                                                  &audit));
        CHECK(audit.computed_count == 1u);
        CHECK(audit.runtime_computed_count == 2u);

        int saw_direct_slot = 0;
        int saw_plain_computed = 0;
        int saw_state_slot = 0;
        for (uint32_t i = 0; i < audit.count; ++i) {
            if (audit.sites[i].site_addr == 0x22u &&
                audit.sites[i].runtime_allowed) {
                saw_direct_slot = 1;
            } else if (audit.sites[i].site_addr == 0x30u &&
                       !audit.sites[i].runtime_allowed) {
                saw_plain_computed = 1;
            } else if (audit.sites[i].site_addr == 0x46u &&
                       audit.sites[i].runtime_allowed) {
                saw_state_slot = 1;
            }
        }
        CHECK(saw_direct_slot);
        CHECK(saw_plain_computed);
        CHECK(saw_state_slot);

        ng_program_rom_free(&dispatcher_rom);
    }

    {
        NgProgramRom idx_rom = make_rom(0xC0u);
        CHECK(idx_rom.data != NULL);

        write16(&idx_rom, 0x00u, 0x207Cu);       /* MOVEA.L #$40,A0 */
        write32(&idx_rom, 0x02u, 0x00000040u);
        write16(&idx_rom, 0x06u, 0xE548u);       /* LSL.W #2,D0 */
        write16(&idx_rom, 0x08u, 0x2270u);       /* MOVEA.L ($0,A0,D0.W),A1 */
        write16(&idx_rom, 0x0Au, 0x0000u);
        write16(&idx_rom, 0x0Cu, 0x4ED1u);       /* JMP (A1) */
        write32(&idx_rom, 0x40u, 0x00000070u);
        write32(&idx_rom, 0x44u, 0x00000080u);
        write32(&idx_rom, 0x48u, 0x00000090u);
        write32(&idx_rom, 0x4Cu, 0x000000A0u);
        write16(&idx_rom, 0x70u, 0x4E75u);
        write16(&idx_rom, 0x80u, 0x4E75u);
        write16(&idx_rom, 0x90u, 0x4E75u);
        write16(&idx_rom, 0xA0u, 0x4E75u);

        const uint32_t idx_seeds[] = {0x00u};
        static NgFunctionDiscovery idx_discovery;
        CHECK(ng_function_discover_from_seeds(&idx_rom,
                                              idx_seeds,
                                              1u,
                                              &idx_discovery));
        CHECK(ng_dispatch_audit_build(&idx_rom, &idx_discovery, &audit));
        CHECK(audit.computed_count == 0u);
        CHECK(audit.jump_table_count == 1u);
        CHECK(audit.jump_table_resolved_entries == 4u);
        CHECK(audit.jump_table_missing_entries == 0u);
        CHECK(audit.sites[0].kind == NG_DISPATCH_AUDIT_JUMP_TABLE);
        CHECK(audit.sites[0].site_addr == 0x0Cu);
        CHECK(audit.sites[0].table_addr == 0x40u);

        ng_program_rom_free(&idx_rom);
    }

    {
        NgProgramRom self_idx_rom = make_rom(0xC0u);
        CHECK(self_idx_rom.data != NULL);

        write16(&self_idx_rom, 0x00u, 0x207Cu);       /* MOVEA.L #$40,A0 */
        write32(&self_idx_rom, 0x02u, 0x00000040u);
        write16(&self_idx_rom, 0x06u, 0xE548u);       /* LSL.W #2,D0 */
        write16(&self_idx_rom, 0x08u, 0x2070u);       /* MOVEA.L ($0,A0,D0.W),A0 */
        write16(&self_idx_rom, 0x0Au, 0x0000u);
        write16(&self_idx_rom, 0x0Cu, 0x4E90u);       /* JSR (A0) */
        write16(&self_idx_rom, 0x0Eu, 0x4E75u);
        write32(&self_idx_rom, 0x40u, 0x00000070u);
        write32(&self_idx_rom, 0x44u, 0x00000080u);
        write32(&self_idx_rom, 0x48u, 0x00000090u);
        write32(&self_idx_rom, 0x4Cu, 0x000000A0u);
        write16(&self_idx_rom, 0x70u, 0x4E75u);
        write16(&self_idx_rom, 0x80u, 0x4E75u);
        write16(&self_idx_rom, 0x90u, 0x4E75u);
        write16(&self_idx_rom, 0xA0u, 0x4E75u);

        const uint32_t self_idx_seeds[] = {0x00u};
        static NgFunctionDiscovery self_idx_discovery;
        CHECK(ng_function_discover_from_seeds(&self_idx_rom,
                                              self_idx_seeds,
                                              1u,
                                              &self_idx_discovery));
        CHECK(ng_dispatch_audit_build(&self_idx_rom,
                                      &self_idx_discovery,
                                      &audit));
        CHECK(audit.computed_count == 0u);
        CHECK(audit.jump_table_count == 1u);
        CHECK(audit.jump_table_resolved_entries == 4u);
        CHECK(audit.jump_table_missing_entries == 0u);
        CHECK(audit.sites[0].kind == NG_DISPATCH_AUDIT_JUMP_TABLE);
        CHECK(audit.sites[0].site_addr == 0x0Cu);
        CHECK(audit.sites[0].table_addr == 0x40u);

        ng_program_rom_free(&self_idx_rom);
    }

    {
        NgProgramRom branch_rom = make_rom(0xA0u);
        CHECK(branch_rom.data != NULL);

        write16(&branch_rom, 0x00u, 0x45FAu);       /* LEA $000040(PC),A2 */
        write16(&branch_rom, 0x02u, 0x003Eu);
        write16(&branch_rom, 0x04u, 0x4EF2u);       /* JMP ($0,A2,D0.W) */
        write16(&branch_rom, 0x06u, 0x0000u);
        write16(&branch_rom, 0x40u, 0x6000u);       /* inline BRA table */
        write16(&branch_rom, 0x42u, 0x003Eu);
        write16(&branch_rom, 0x44u, 0x6000u);
        write16(&branch_rom, 0x46u, 0x003Eu);
        write16(&branch_rom, 0x48u, 0x6000u);
        write16(&branch_rom, 0x4Au, 0x003Eu);
        write16(&branch_rom, 0x4Cu, 0x6000u);
        write16(&branch_rom, 0x4Eu, 0x003Eu);
        write16(&branch_rom, 0x50u, 0x4E75u);
        write16(&branch_rom, 0x80u, 0x4E75u);
        write16(&branch_rom, 0x84u, 0x4E75u);
        write16(&branch_rom, 0x88u, 0x4E75u);
        write16(&branch_rom, 0x8Cu, 0x4E75u);

        const uint32_t branch_seeds[] = {0x00u};
        static NgFunctionDiscovery branch_discovery;
        CHECK(ng_function_discover_from_seeds(&branch_rom,
                                              branch_seeds,
                                              1u,
                                              &branch_discovery));
        CHECK(ng_dispatch_audit_build(&branch_rom,
                                      &branch_discovery,
                                      &audit));
        CHECK(audit.computed_count == 0u);
        CHECK(audit.jump_table_count == 1u);
        CHECK(audit.jump_table_resolved_entries == 4u);
        CHECK(audit.jump_table_missing_entries == 0u);
        CHECK(audit.sites[0].kind == NG_DISPATCH_AUDIT_JUMP_TABLE);
        CHECK(audit.sites[0].site_addr == 0x04u);
        CHECK(audit.sites[0].table_addr == 0x40u);

        ng_program_rom_free(&branch_rom);
    }

    {
        NgProgramRom direct_dispatch_rom = make_rom(0xC0u);
        CHECK(direct_dispatch_rom.data != NULL);

        write16(&direct_dispatch_rom, 0x20u, 0x4EF9u); /* JMP $000080 */
        write32(&direct_dispatch_rom, 0x22u, 0x00000080u);
        write16(&direct_dispatch_rom, 0x26u, 0x4EF9u); /* JMP $000090 */
        write32(&direct_dispatch_rom, 0x28u, 0x00000090u);
        write16(&direct_dispatch_rom, 0x2Cu, 0x4EF9u); /* JMP $0000A0 */
        write32(&direct_dispatch_rom, 0x2Eu, 0x000000A0u);
        write16(&direct_dispatch_rom, 0x32u, 0x4E75u);
        write16(&direct_dispatch_rom, 0x80u, 0x4E75u);
        write16(&direct_dispatch_rom, 0x90u, 0x4E75u);
        write16(&direct_dispatch_rom, 0xA0u, 0x4E75u);

        const uint32_t direct_dispatch_seeds[] = {0x20u};
        static NgFunctionDiscovery direct_dispatch_discovery;
        CHECK(ng_function_discover_from_seeds(&direct_dispatch_rom,
                                              direct_dispatch_seeds,
                                              1u,
                                              &direct_dispatch_discovery));
        CHECK(ng_dispatch_audit_build(&direct_dispatch_rom,
                                      &direct_dispatch_discovery,
                                      &audit));
        CHECK(audit.computed_count == 0u);
        CHECK(audit.jump_table_count == 1u);
        CHECK(audit.jump_table_resolved_entries == 3u);
        CHECK(audit.jump_table_missing_entries == 0u);
        CHECK(audit.sites[0].kind == NG_DISPATCH_AUDIT_JUMP_TABLE);
        CHECK(audit.sites[0].site_addr == 0x20u);
        CHECK(audit.sites[0].table_addr == 0x20u);

        ng_program_rom_free(&direct_dispatch_rom);
    }

    {
        NgProgramRom pc_branch_rom = make_rom(0x80u);
        CHECK(pc_branch_rom.data != NULL);

        write16(&pc_branch_rom, 0x00u, 0x4EFBu); /* JMP (2,PC,D0.W) */
        write16(&pc_branch_rom, 0x02u, 0x0002u);
        write16(&pc_branch_rom, 0x04u, 0x6000u); /* BRA $40 */
        write16(&pc_branch_rom, 0x06u, 0x003Au);
        write16(&pc_branch_rom, 0x08u, 0x6000u); /* BRA $50 */
        write16(&pc_branch_rom, 0x0Au, 0x0046u);
        write16(&pc_branch_rom, 0x0Cu, 0x6000u); /* BRA $60 */
        write16(&pc_branch_rom, 0x0Eu, 0x0052u);
        write16(&pc_branch_rom, 0x10u, 0x4E75u);
        write16(&pc_branch_rom, 0x40u, 0x4E75u);
        write16(&pc_branch_rom, 0x50u, 0x4E75u);
        write16(&pc_branch_rom, 0x60u, 0x4E75u);

        const uint32_t pc_branch_seeds[] = {0x00u};
        static NgFunctionDiscovery pc_branch_discovery;
        CHECK(ng_function_discover_from_seeds(&pc_branch_rom,
                                              pc_branch_seeds,
                                              1u,
                                              &pc_branch_discovery));
        CHECK(ng_dispatch_audit_build(&pc_branch_rom,
                                      &pc_branch_discovery,
                                      &audit));
        CHECK(audit.computed_count == 0u);
        CHECK(audit.jump_table_count == 1u);
        CHECK(audit.jump_table_resolved_entries == 3u);
        CHECK(audit.jump_table_missing_entries == 0u);
        CHECK(audit.sites[0].kind == NG_DISPATCH_AUDIT_JUMP_TABLE);
        CHECK(audit.sites[0].site_addr == 0x00u);
        CHECK(audit.sites[0].table_addr == 0x04u);

        ng_program_rom_free(&pc_branch_rom);
    }

    {
        NgProgramRom terminal_branch_rom = make_rom(0xA0u);
        CHECK(terminal_branch_rom.data != NULL);

        write16(&terminal_branch_rom, 0x00u, 0x4EFBu); /* JMP (2,PC,D0.W) */
        write16(&terminal_branch_rom, 0x02u, 0x0002u);
        for (uint32_t i = 0; i < 9u; ++i) {
            uint32_t entry = 0x04u + i * 4u;
            uint32_t target = 0x82u - i * 0x0Au;
            write16(&terminal_branch_rom, entry, 0x6000u);
            write16(&terminal_branch_rom, entry + 2u,
                    (uint16_t)(target - (entry + 2u)));
            write16(&terminal_branch_rom, target, 0x4E75u);
        }
        write16(&terminal_branch_rom, 0x28u, 0x3EDEu);
        write16(&terminal_branch_rom, 0x2Au, 0x2887u);
        write16(&terminal_branch_rom, 0x2Cu, 0xDE84u);
        write16(&terminal_branch_rom, 0x2Eu, 0x4E75u);

        const uint32_t terminal_branch_seeds[] = {0x00u};
        static NgFunctionDiscovery terminal_branch_discovery;
        CHECK(ng_function_discover_from_seeds(&terminal_branch_rom,
                                              terminal_branch_seeds,
                                              1u,
                                              &terminal_branch_discovery));
        CHECK(ng_dispatch_audit_build(&terminal_branch_rom,
                                      &terminal_branch_discovery,
                                      &audit));
        CHECK(audit.computed_count == 0u);
        CHECK(audit.jump_table_count == 1u);
        CHECK(audit.jump_table_resolved_entries == 10u);
        CHECK(audit.jump_table_missing_entries == 0u);
        CHECK(audit.sites[0].kind == NG_DISPATCH_AUDIT_JUMP_TABLE);
        CHECK(audit.sites[0].site_addr == 0x00u);
        CHECK(audit.sites[0].table_addr == 0x04u);

        ng_program_rom_free(&terminal_branch_rom);
    }

    {
        NgProgramRom pc_call_branch_rom = make_rom(0x80u);
        CHECK(pc_call_branch_rom.data != NULL);

        write16(&pc_call_branch_rom, 0x00u, 0x4EBBu); /* JSR (2,PC,D0.W) */
        write16(&pc_call_branch_rom, 0x02u, 0x0002u);
        write16(&pc_call_branch_rom, 0x04u, 0x6000u); /* BRA $40 */
        write16(&pc_call_branch_rom, 0x06u, 0x003Au);
        write16(&pc_call_branch_rom, 0x08u, 0x6000u); /* BRA $50 */
        write16(&pc_call_branch_rom, 0x0Au, 0x0046u);
        write16(&pc_call_branch_rom, 0x0Cu, 0x6000u); /* BRA $60 */
        write16(&pc_call_branch_rom, 0x0Eu, 0x0052u);
        write16(&pc_call_branch_rom, 0x10u, 0x4E75u);
        write16(&pc_call_branch_rom, 0x40u, 0x4E75u);
        write16(&pc_call_branch_rom, 0x50u, 0x4E75u);
        write16(&pc_call_branch_rom, 0x60u, 0x4E75u);

        const uint32_t pc_call_branch_seeds[] = {0x00u};
        static NgFunctionDiscovery pc_call_branch_discovery;
        CHECK(ng_function_discover_from_seeds(&pc_call_branch_rom,
                                              pc_call_branch_seeds,
                                              1u,
                                              &pc_call_branch_discovery));
        CHECK(ng_dispatch_audit_build(&pc_call_branch_rom,
                                      &pc_call_branch_discovery,
                                      &audit));
        CHECK(audit.computed_count == 0u);
        CHECK(audit.jump_table_count == 1u);
        CHECK(audit.jump_table_resolved_entries == 3u);
        CHECK(audit.jump_table_missing_entries == 0u);
        CHECK(audit.sites[0].kind == NG_DISPATCH_AUDIT_JUMP_TABLE);
        CHECK(audit.sites[0].site_addr == 0x00u);
        CHECK(audit.sites[0].table_addr == 0x04u);

        ng_program_rom_free(&pc_call_branch_rom);
    }

    NgProgramRom stop_rom = make_rom(0x80u);
    CHECK(stop_rom.data != NULL);

    write16(&stop_rom, 0x10u, 0x42C0u); /* UNKNOWN; do not scan into $12 */
    write16(&stop_rom, 0x12u, 0x4EB9u);
    write32(&stop_rom, 0x14u, 0x00000300u);

    write16(&stop_rom, 0x20u, 0x4AFCu); /* ILLEGAL-style assertion; may fall through */
    write16(&stop_rom, 0x22u, 0x4EB9u);
    write32(&stop_rom, 0x24u, 0x00000300u);

    write16(&stop_rom, 0x30u, 0x4E41u); /* TRAP #1 assertion; may fall through */
    write16(&stop_rom, 0x32u, 0x4EB9u);
    write32(&stop_rom, 0x34u, 0x00000300u);

    write16(&stop_rom, 0x40u, 0x4E73u); /* RTE; do not scan into $42 */
    write16(&stop_rom, 0x42u, 0x4EB9u);
    write32(&stop_rom, 0x44u, 0x00000300u);

    write16(&stop_rom, 0x50u, 0x4E77u); /* RTR; do not scan into $52 */
    write16(&stop_rom, 0x52u, 0x4EB9u);
    write32(&stop_rom, 0x54u, 0x00000300u);

    write16(&stop_rom, 0x60u, 0x6006u); /* BRA $68; do not scan into $62 */
    write16(&stop_rom, 0x62u, 0x4EB9u);
    write32(&stop_rom, 0x64u, 0x00000300u);
    write16(&stop_rom, 0x68u, 0x4E75u);

    static NgFunctionDiscovery stop_discovery;
    ng_function_discovery_init(&stop_discovery);
    CHECK(!ng_function_discovery_add(&stop_discovery, &stop_rom, 0x10u));
    CHECK(ng_function_discovery_add(&stop_discovery, &stop_rom, 0x20u));
    CHECK(ng_function_discovery_add(&stop_discovery, &stop_rom, 0x30u));
    CHECK(ng_function_discovery_add(&stop_discovery, &stop_rom, 0x40u));
    CHECK(ng_function_discovery_add(&stop_discovery, &stop_rom, 0x50u));
    CHECK(ng_function_discovery_add(&stop_discovery, &stop_rom, 0x60u));

    CHECK(ng_dispatch_audit_build(&stop_rom, &stop_discovery, &audit));
    CHECK(audit.count == 2u);
    CHECK(audit.direct_count == 2u);
    CHECK(audit.missing_direct_count == 2u);
    CHECK(audit.computed_count == 0u);
    CHECK(audit.jump_table_count == 0u);
    CHECK(ng_dispatch_audit_has_gaps(&audit));
    CHECK(!audit.truncated);

    ng_program_rom_free(&stop_rom);

    {
        const uint32_t seed_count = 1500u;
        NgProgramRom many_rom = make_rom(seed_count * 2u);
        uint32_t seeds[1500];
        CHECK(many_rom.data != NULL);

        for (uint32_t i = 0; i < seed_count; ++i) {
            uint32_t addr = i * 2u;
            write16(&many_rom, addr, 0x4E75u); /* RTS */
            seeds[i] = addr;
        }

        static NgFunctionDiscovery many_discovery;
        CHECK(ng_function_discover_from_seeds(&many_rom,
                                              seeds,
                                              seed_count,
                                              &many_discovery));
        CHECK(many_discovery.count == seed_count);
        CHECK(!many_discovery.truncated);
        CHECK(ng_dispatch_audit_build(&many_rom, &many_discovery, &audit));
        CHECK(audit.count == 0u);
        CHECK(!audit.truncated);
        CHECK(!ng_dispatch_audit_has_gaps(&audit));

        ng_program_rom_free(&many_rom);
    }

    {
        const uint32_t dispatch_count = 8300u;
        NgProgramRom many_dispatch_rom = make_rom(dispatch_count * 8u);
        static NgFunctionDiscovery many_dispatch_discovery;
        CHECK(many_dispatch_rom.data != NULL);

        ng_function_discovery_init(&many_dispatch_discovery);
        for (uint32_t i = 0; i < dispatch_count; ++i) {
            uint32_t addr = i * 8u;
            write16(&many_dispatch_rom, addr, 0x4EB9u); /* JSR $000000 */
            write32(&many_dispatch_rom, addr + 2u, 0x00000000u);
            write16(&many_dispatch_rom, addr + 6u, 0x4E75u); /* RTS */
            many_dispatch_discovery.addrs[many_dispatch_discovery.count++] = addr;
        }

        CHECK(ng_dispatch_audit_build(&many_dispatch_rom,
                                      &many_dispatch_discovery,
                                      &audit));
        CHECK(audit.count == dispatch_count);
        CHECK(audit.direct_count == dispatch_count);
        CHECK(!audit.truncated);

        ng_program_rom_free(&many_dispatch_rom);
    }

    return 0;
}
