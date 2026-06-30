#include "function_discovery.h"
#include "game_config.h"

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

int main(void) {
    NgFunctionDiscovery discovery;

    {
        NgProgramRom rom = make_rom(0xC0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x40u, 0x207Bu); /* MOVEA.L (4,PC,D0.W),A0 */
        write16(&rom, 0x42u, 0x0004u);
        write16(&rom, 0x44u, 0x4ED0u); /* JMP (A0) */
        write32(&rom, 0x46u, 0x00000080u);
        write32(&rom, 0x4Au, 0x00000090u);
        write32(&rom, 0x4Eu, 0x000000A0u);
        write32(&rom, 0x52u, 0x000000B0u);
        write16(&rom, 0x80u, 0x4E75u);
        write16(&rom, 0x90u, 0x4E75u);
        write16(&rom, 0xA0u, 0x4E75u);
        write16(&rom, 0xB0u, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x40u, &discovery));
        CHECK(discovery.count == 6u);
        CHECK(discovery.addrs[0] == 0x40u);
        CHECK(discovery.addrs[1] == 0x44u);
        CHECK(discovery.addrs[2] == 0x80u);
        CHECK(discovery.addrs[3] == 0x90u);
        CHECK(discovery.addrs[4] == 0xA0u);
        CHECK(discovery.addrs[5] == 0xB0u);
        CHECK(!discovery.truncated);
        CHECK(ng_function_discovery_contains(&discovery, 0xA0u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xA2u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x200u);
        CHECK(rom.data != NULL);
        ng_program_rom_set_address_map(&rom, 0x000000u, 0x100u, 0x200000u, 0x100u);

        write16(&rom, 0x20u, 0x4E75u);
        write16(&rom, 0x110u, 0x4EB9u); /* banked JSR $000020 */
        write32(&rom, 0x112u, 0x00000020u);
        write16(&rom, 0x116u, 0x4E75u);

        CHECK(ng_program_rom_addr_is_mapped(&rom, 0x000020u));
        CHECK(ng_program_rom_addr_is_mapped(&rom, 0x200010u));
        CHECK(!ng_program_rom_addr_is_mapped(&rom, 0x100010u));
        CHECK(ng_program_rom_read16(&rom, 0x200010u) == 0x4EB9u);

        CHECK(ng_function_discover_from_entry(&rom, 0x200010u, &discovery));
        CHECK(discovery.count == 3u);
        CHECK(discovery.addrs[0] == 0x200010u);
        CHECK(discovery.addrs[1] == 0x000020u);
        CHECK(discovery.addrs[2] == 0x200016u);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x180u);
        CHECK(rom.data != NULL);
        ng_program_rom_set_address_map(&rom,
                                       0x000000u,
                                       0x100u,
                                       0x200000u,
                                       0x40u);
        CHECK(ng_program_rom_configure_bank(&rom, 1u, 0x100u, 0x40u));

        write16(&rom, 0x10u, 0x4E75u);
        write32(&rom, 0x100u, 0x00200020u);  /* explicit bank 1 record */
        write16(&rom, 0x120u, 0x4E75u);      /* explicit bank 1 target */

        NgGameConfig config;
        ng_game_config_init(&config);
        config.state_table_count = 1u;
        config.state_tables[0].root = 0x200000u;
        config.state_tables[0].table_start = 0x200000u;
        config.state_tables[0].table_end = 0x200010u;
        config.state_tables[0].stride = 4u;
        config.state_tables[0].sentinel = 0xFFFFFFFFu;
        config.state_tables[0].target_start = 0x200000u;
        config.state_tables[0].target_end = 0x200040u;
        config.state_tables[0].max_tables = 1u;
        config.state_tables[0].max_entries = 4u;
        config.state_tables[0].scan_count = 1u;
        config.state_tables[0].scans[0].kind =
            NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL;

        const uint32_t seeds[] = {0x00000010u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(discovery.count == 3u);
        CHECK(ng_function_discovery_contains_bank(&discovery,
                                                  0x200020u,
                                                  0u));
        CHECK(ng_function_discovery_contains_bank(&discovery,
                                                  0x200020u,
                                                  1u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x80u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x4EFBu); /* JMP (2,PC,D0.W), inline BRA table */
        write16(&rom, 0x02u, 0x0002u);
        write16(&rom, 0x04u, 0x6000u); /* BRA $40 */
        write16(&rom, 0x06u, 0x003Au);
        write16(&rom, 0x08u, 0x6000u); /* BRA $50 */
        write16(&rom, 0x0Au, 0x0046u);
        write16(&rom, 0x0Cu, 0x6000u); /* BRA $60 */
        write16(&rom, 0x0Eu, 0x0052u);
        write16(&rom, 0x10u, 0x4E75u); /* stops the table scan */
        write16(&rom, 0x40u, 0x4E75u);
        write16(&rom, 0x50u, 0x4E75u);
        write16(&rom, 0x60u, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x04u));
        CHECK(ng_function_discovery_contains(&discovery, 0x08u));
        CHECK(ng_function_discovery_contains(&discovery, 0x0Cu));
        CHECK(ng_function_discovery_contains(&discovery, 0x40u));
        CHECK(ng_function_discovery_contains(&discovery, 0x50u));
        CHECK(ng_function_discovery_contains(&discovery, 0x60u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xA0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x4EFBu); /* JMP (2,PC,D0.W), BRA table plus inline terminal body */
        write16(&rom, 0x02u, 0x0002u);
        for (uint32_t i = 0; i < 9u; ++i) {
            uint32_t entry = 0x04u + i * 4u;
            uint32_t target = 0x82u - i * 0x0Au;
            write16(&rom, entry, 0x6000u);
            write16(&rom, entry + 2u,
                    (uint16_t)(target - (entry + 2u)));
            write16(&rom, target, 0x4E75u);
        }
        write16(&rom, 0x28u, 0x3EDEu); /* MOVE.W (A6)+,D7 */
        write16(&rom, 0x2Au, 0x2887u); /* MOVE.L D7,(A4) */
        write16(&rom, 0x2Cu, 0xDE84u); /* ADD.L D4,D7 */
        write16(&rom, 0x2Eu, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x24u));
        CHECK(ng_function_discovery_contains(&discovery, 0x28u));
        CHECK(ng_function_discovery_contains(&discovery, 0x2Au));
        CHECK(ng_function_discovery_contains(&discovery, 0x2Cu));
        CHECK(ng_function_discovery_contains(&discovery, 0x2Eu));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x80u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x4EBBu); /* JSR (2,PC,D0.W), inline BRA table */
        write16(&rom, 0x02u, 0x0002u);
        write16(&rom, 0x04u, 0x6000u); /* BRA $40 */
        write16(&rom, 0x06u, 0x003Au);
        write16(&rom, 0x08u, 0x6000u); /* BRA $50 */
        write16(&rom, 0x0Au, 0x0046u);
        write16(&rom, 0x0Cu, 0x6000u); /* BRA $60 */
        write16(&rom, 0x0Eu, 0x0052u);
        write16(&rom, 0x10u, 0x4E75u); /* stops the table scan */
        write16(&rom, 0x40u, 0x4E75u);
        write16(&rom, 0x50u, 0x4E75u);
        write16(&rom, 0x60u, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x04u));
        CHECK(ng_function_discovery_contains(&discovery, 0x08u));
        CHECK(ng_function_discovery_contains(&discovery, 0x0Cu));
        CHECK(ng_function_discovery_contains(&discovery, 0x40u));
        CHECK(ng_function_discovery_contains(&discovery, 0x50u));
        CHECK(ng_function_discovery_contains(&discovery, 0x60u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xA0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x20u, 0x207Bu);
        write16(&rom, 0x22u, 0x0004u);
        write16(&rom, 0x24u, 0x4ED0u);
        write32(&rom, 0x26u, 0x00000080u);
        write32(&rom, 0x2Au, 0x00000080u);
        write32(&rom, 0x2Eu, 0x00000200u);
        write32(&rom, 0x32u, 0x00000090u);
        write16(&rom, 0x80u, 0x4E75u);
        write16(&rom, 0x90u, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x20u, &discovery));
        CHECK(discovery.count == 4u);
        CHECK(discovery.addrs[0] == 0x20u);
        CHECK(discovery.addrs[1] == 0x24u);
        CHECK(discovery.addrs[2] == 0x80u);
        CHECK(discovery.addrs[3] == 0x90u);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xC0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x20u, 0x4EF9u); /* JMP $000080 */
        write32(&rom, 0x22u, 0x00000080u);
        write16(&rom, 0x26u, 0x4EF9u); /* JMP $000090 */
        write32(&rom, 0x28u, 0x00000090u);
        write16(&rom, 0x2Cu, 0x4EF9u); /* JMP $0000A0 */
        write32(&rom, 0x2Eu, 0x000000A0u);
        write16(&rom, 0x32u, 0x4E75u);
        write16(&rom, 0x80u, 0x4E75u);
        write16(&rom, 0x90u, 0x4E75u);
        write16(&rom, 0xA0u, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x20u, &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x20u));
        CHECK(ng_function_discovery_contains(&discovery, 0x26u));
        CHECK(ng_function_discovery_contains(&discovery, 0x2Cu));
        CHECK(ng_function_discovery_contains(&discovery, 0x80u));
        CHECK(ng_function_discovery_contains(&discovery, 0x90u));
        CHECK(ng_function_discovery_contains(&discovery, 0xA0u));
        CHECK(!ng_function_discovery_contains(&discovery, 0x32u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xC0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x207Cu);       /* MOVEA.L #$40,A0 */
        write32(&rom, 0x02u, 0x00000040u);
        write16(&rom, 0x06u, 0xE548u);       /* LSL.W #2,D0 */
        write16(&rom, 0x08u, 0x2270u);       /* MOVEA.L ($0,A0,D0.W),A1 */
        write16(&rom, 0x0Au, 0x0000u);
        write16(&rom, 0x0Cu, 0x4ED1u);       /* JMP (A1) */
        write32(&rom, 0x40u, 0x00000070u);
        write32(&rom, 0x44u, 0x00000080u);
        write32(&rom, 0x48u, 0x00000090u);
        write32(&rom, 0x4Cu, 0x000000A0u);
        write16(&rom, 0x70u, 0x4E75u);
        write16(&rom, 0x80u, 0x4E75u);
        write16(&rom, 0x90u, 0x4E75u);
        write16(&rom, 0xA0u, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x70u));
        CHECK(ng_function_discovery_contains(&discovery, 0x80u));
        CHECK(ng_function_discovery_contains(&discovery, 0x90u));
        CHECK(ng_function_discovery_contains(&discovery, 0xA0u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xA0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x45FAu);       /* LEA $000040(PC),A2 */
        write16(&rom, 0x02u, 0x003Eu);
        write16(&rom, 0x04u, 0x4EF2u);       /* JMP ($0,A2,D0.W) */
        write16(&rom, 0x06u, 0x0000u);
        write16(&rom, 0x40u, 0x6000u);       /* inline BRA table */
        write16(&rom, 0x42u, 0x003Eu);
        write16(&rom, 0x44u, 0x6000u);
        write16(&rom, 0x46u, 0x003Eu);
        write16(&rom, 0x48u, 0x6000u);
        write16(&rom, 0x4Au, 0x003Eu);
        write16(&rom, 0x4Cu, 0x6000u);
        write16(&rom, 0x4Eu, 0x003Eu);
        write16(&rom, 0x50u, 0x4E75u);       /* first non-BRA stops scan */
        write16(&rom, 0x80u, 0x4E75u);
        write16(&rom, 0x84u, 0x4E75u);
        write16(&rom, 0x88u, 0x4E75u);
        write16(&rom, 0x8Cu, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x40u));
        CHECK(ng_function_discovery_contains(&discovery, 0x44u));
        CHECK(ng_function_discovery_contains(&discovery, 0x48u));
        CHECK(ng_function_discovery_contains(&discovery, 0x4Cu));
        CHECK(!ng_function_discovery_contains(&discovery, 0x50u));
        CHECK(ng_function_discovery_contains(&discovery, 0x80u));
        CHECK(ng_function_discovery_contains(&discovery, 0x84u));
        CHECK(ng_function_discovery_contains(&discovery, 0x88u));
        CHECK(ng_function_discovery_contains(&discovery, 0x8Cu));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xC0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x207Cu);       /* MOVEA.L #$40,A0 */
        write32(&rom, 0x02u, 0x00000040u);
        write16(&rom, 0x06u, 0xE548u);       /* LSL.W #2,D0 */
        write16(&rom, 0x08u, 0x2070u);       /* MOVEA.L ($0,A0,D0.W),A0 */
        write16(&rom, 0x0Au, 0x0000u);
        write16(&rom, 0x0Cu, 0x4E90u);       /* JSR (A0) */
        write16(&rom, 0x0Eu, 0x4E75u);
        write32(&rom, 0x40u, 0x00000070u);
        write32(&rom, 0x44u, 0x00000080u);
        write32(&rom, 0x48u, 0x00000090u);
        write32(&rom, 0x4Cu, 0x000000A0u);
        write16(&rom, 0x70u, 0x4E75u);
        write16(&rom, 0x80u, 0x4E75u);
        write16(&rom, 0x90u, 0x4E75u);
        write16(&rom, 0xA0u, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x70u));
        CHECK(ng_function_discovery_contains(&discovery, 0x80u));
        CHECK(ng_function_discovery_contains(&discovery, 0x90u));
        CHECK(ng_function_discovery_contains(&discovery, 0xA0u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x40u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0xDC46u);       /* ADD.W D6,D6 */
        write16(&rom, 0x12u, 0x4EFBu);       /* JMP ($2,PC,D6.W) */
        write16(&rom, 0x14u, 0x6002u);
        for (uint32_t addr = 0x16u; addr < 0x26u; addr += 4u) {
            write16(&rom, addr, 0x2887u);    /* MOVE.L D7,(A4) */
            write16(&rom, addr + 2u, 0xDE84u); /* ADD.L D4,D7 */
        }
        write16(&rom, 0x26u, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x10u, &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x16u));
        CHECK(ng_function_discovery_contains(&discovery, 0x1Au));
        CHECK(ng_function_discovery_contains(&discovery, 0x1Eu));
        CHECK(ng_function_discovery_contains(&discovery, 0x22u));
        CHECK(!ng_function_discovery_contains(&discovery, 0x2Au));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x120u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x41FAu);       /* LEA $000040(PC),A0 */
        write16(&rom, 0x02u, 0x003Eu);
        write16(&rom, 0x04u, 0x227Cu);       /* MOVEA.L #$FFFFFFFF,A1 */
        write32(&rom, 0x06u, 0xFFFFFFFFu);
        write16(&rom, 0x0Au, 0x4EB9u);       /* JSR table selector */
        write32(&rom, 0x0Cu, 0x000000C0u);
        write16(&rom, 0x10u, 0x4E75u);       /* caller continuation */
        write32(&rom, 0x40u, 0xFFFFFFFFu);   /* sparse leading sentinel */
        write32(&rom, 0x44u, 0x00000070u);
        write32(&rom, 0x48u, 0xFFFFFFFFu);   /* sparse interior sentinel */
        write32(&rom, 0x4Cu, 0x00000090u);
        write32(&rom, 0x50u, 0xDEAD0000u);   /* non-target terminates scan */
        write16(&rom, 0x70u, 0x4E75u);
        write16(&rom, 0x90u, 0x4E75u);
        write16(&rom, 0xC0u, 0x4E75u);       /* selector helper */

        NgGameConfig config;
        ng_game_config_init(&config);
        config.table_call_count = 1u;
        config.table_calls[0].helper = 0x000000C0u;
        config.table_calls[0].table_start = 0x40u;
        config.table_calls[0].table_end = 0x60u;
        config.table_calls[0].table_reg = 0u;
        config.table_calls[0].max_entries = 8u;
        config.table_calls[0].sentinel = 0xFFFFFFFFu;
        config.table_calls[0].format =
            NG_GAME_CONFIG_TABLE_CALL_ABS32_SPARSE;

        const uint32_t seeds[] = {0x00000000u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x00u));
        CHECK(ng_function_discovery_contains(&discovery, 0x04u));
        CHECK(ng_function_discovery_contains(&discovery, 0x0Au));
        CHECK(ng_function_discovery_contains(&discovery, 0x10u));
        CHECK(ng_function_discovery_contains(&discovery, 0x70u));
        CHECK(ng_function_discovery_contains(&discovery, 0x90u));
        CHECK(ng_function_discovery_contains(&discovery, 0xC0u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xDEAD0000u));
        CHECK(discovery.count == 7u);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x180u);
        CHECK(rom.data != NULL);
        ng_program_rom_set_address_map(&rom,
                                       0x000000u,
                                       0x100u,
                                       0x200000u,
                                       0x40u);

        write16(&rom, 0x10u, 0x4E75u);       /* fixed seed */
        write32(&rom, 0x100u, 0x00200020u);  /* bank 0 record */
        write16(&rom, 0x120u, 0x4E75u);      /* bank 0 target */
        write32(&rom, 0x140u, 0x00200020u);  /* bank 1 record */
        write16(&rom, 0x160u, 0x4E75u);      /* bank 1 target */

        NgGameConfig config;
        ng_game_config_init(&config);
        config.record_format_count = 1u;
        config.record_formats[0].stride = 4u;
        config.record_formats[0].callback_offset_count = 1u;
        config.record_formats[0].callback_offsets[0] = 0u;
        config.record_formats[0].target_start = 0x200000u;
        config.record_formats[0].target_end = 0x200040u;
        config.record_formats[0].scan_count = 1u;
        config.record_formats[0].scans[0].kind =
            NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL;

        const uint32_t seeds[] = {0x00000010u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(discovery.count == 3u);
        CHECK(ng_function_discovery_contains(&discovery, 0x10u));
        CHECK(ng_function_discovery_contains_bank(&discovery,
                                                  0x200020u,
                                                  0u));
        CHECK(ng_function_discovery_contains_bank(&discovery,
                                                  0x200020u,
                                                  1u));
        CHECK(!ng_function_discovery_contains_bank(&discovery,
                                                   0x200020u,
                                                   2u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x180u);
        CHECK(rom.data != NULL);
        ng_program_rom_set_address_map(&rom,
                                       0x000000u,
                                       0x100u,
                                       0x200000u,
                                       0x40u);
        CHECK(ng_program_rom_configure_bank(&rom, 1u, 0x100u, 0x40u));

        write16(&rom, 0x10u, 0x4E75u);
        write32(&rom, 0x100u, 0x00200020u);
        write16(&rom, 0x120u, 0x4E75u);

        NgGameConfig config;
        ng_game_config_init(&config);
        config.record_format_count = 1u;
        config.record_formats[0].stride = 4u;
        config.record_formats[0].callback_offset_count = 1u;
        config.record_formats[0].target_start = 0x200000u;
        config.record_formats[0].target_end = 0x200040u;
        config.record_formats[0].scan_count = 1u;
        config.record_formats[0].scans[0].kind =
            NG_GAME_CONFIG_RECORD_SCAN_BANK_ONE;
        config.record_formats[0].scans[0].bank_id = 1u;

        const uint32_t seeds[] = {0x00000010u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(discovery.count == 2u);
        CHECK(!ng_function_discovery_contains_bank(&discovery,
                                                   0x200020u,
                                                   0u));
        CHECK(ng_function_discovery_contains_bank(&discovery,
                                                  0x200020u,
                                                  1u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xE0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x41FAu);       /* LEA $000040(PC),A0 */
        write16(&rom, 0x02u, 0x003Eu);
        write16(&rom, 0x04u, 0x4EB9u);       /* JSR record interpreter */
        write32(&rom, 0x06u, 0x000000C0u);
        write16(&rom, 0x0Au, 0x4E75u);       /* caller continuation */
        write16(&rom, 0x40u, 0x0700u);       /* unrelated record tag */
        write32(&rom, 0x42u, 0x00000080u);
        write16(&rom, 0x48u, 0x0800u);       /* tagged callback record */
        write32(&rom, 0x4Au, 0x00000090u);
        write16(&rom, 0x50u, 0x0800u);       /* outside target range */
        write32(&rom, 0x52u, 0x000000B0u);
        write16(&rom, 0x58u, 0x0800u);       /* invalid callback */
        write32(&rom, 0x5Au, 0x000000A0u);
        write16(&rom, 0x80u, 0x4E75u);
        write16(&rom, 0x90u, 0x4E75u);
        write16(&rom, 0xA0u, 0x15C0u);
        write16(&rom, 0xB0u, 0x4E75u);
        write16(&rom, 0xC0u, 0x4E75u);       /* record interpreter */

        NgGameConfig config;
        ng_game_config_init(&config);
        config.table_call_count = 1u;
        config.table_calls[0].helper = 0x000000C0u;
        config.table_calls[0].table_start = 0x40u;
        config.table_calls[0].table_end = 0x60u;
        config.table_calls[0].table_reg = 0u;
        config.table_calls[0].max_entries = 16u;
        config.table_calls[0].stride = 2u;
        config.table_calls[0].match = 0x0800u;
        config.table_calls[0].target_offset = 2u;
        config.table_calls[0].target_start = 0x80u;
        config.table_calls[0].target_end = 0xA0u;
        config.table_calls[0].format =
            NG_GAME_CONFIG_TABLE_CALL_TAGGED_ABS32;

        const uint32_t seeds[] = {0x00000000u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x00u));
        CHECK(ng_function_discovery_contains(&discovery, 0x04u));
        CHECK(ng_function_discovery_contains(&discovery, 0x0Au));
        CHECK(ng_function_discovery_contains(&discovery, 0x90u));
        CHECK(ng_function_discovery_contains(&discovery, 0xC0u));
        CHECK(!ng_function_discovery_contains(&discovery, 0x80u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xA0u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xB0u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xC0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x4E75u);       /* base seed */
        write32(&rom, 0x40u, 0xFFFFFFFFu);   /* sparse leading sentinel */
        write32(&rom, 0x44u, 0x00000080u);   /* root table code target */
        write32(&rom, 0x48u, 0x00000060u);   /* chained table pointer */
        write32(&rom, 0x4Cu, 0x00000091u);   /* odd state marker, skipped */
        write32(&rom, 0x50u, 0x00000090u);   /* root table code target */
        write32(&rom, 0x54u, 0xDEAD0000u);   /* data terminates root */
        write32(&rom, 0x58u, 0x000000A8u);   /* would be valid, not scanned */
        write32(&rom, 0x60u, 0xFFFFFFFFu);   /* chained table sentinel */
        write32(&rom, 0x64u, 0x000000A0u);   /* chained table code target */
        write32(&rom, 0x68u, 0x000000B0u);   /* in range but invalid code terminates */
        write32(&rom, 0x6Cu, 0x000000A8u);   /* would be valid, not scanned */
        write16(&rom, 0x80u, 0x4E75u);
        write16(&rom, 0x90u, 0x4E75u);
        write16(&rom, 0xA0u, 0x4E75u);
        write16(&rom, 0xA8u, 0x4E75u);
        write16(&rom, 0xB0u, 0x15C0u);

        NgGameConfig config;
        ng_game_config_init(&config);
        config.state_table_count = 1u;
        config.state_tables[0].root = 0x40u;
        config.state_tables[0].table_start = 0x40u;
        config.state_tables[0].table_end = 0x70u;
        config.state_tables[0].stride = 4u;
        config.state_tables[0].sentinel = 0xFFFFFFFFu;
        config.state_tables[0].follow_chain = 1;
        config.state_tables[0].target_start = 0x80u;
        config.state_tables[0].target_end = 0xC0u;
        config.state_tables[0].max_tables = 4u;
        config.state_tables[0].max_entries = 8u;

        const uint32_t seeds[] = {0x00000010u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(discovery.count == 4u);
        CHECK(ng_function_discovery_contains(&discovery, 0x10u));
        CHECK(ng_function_discovery_contains(&discovery, 0x80u));
        CHECK(ng_function_discovery_contains(&discovery, 0x90u));
        CHECK(ng_function_discovery_contains(&discovery, 0xA0u));
        CHECK(!ng_function_discovery_contains(&discovery, 0x60u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xA8u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xB0u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xDEAD0000u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x80u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x4E75u);       /* base seed */
        write32(&rom, 0x20u, 0x00000040u);   /* valid target */
        write32(&rom, 0x24u, 0x00000050u);   /* even marker in target range */
        write32(&rom, 0x28u, 0x00000060u);   /* valid target after marker */
        write32(&rom, 0x2Cu, 0xDEAD0000u);   /* terminator outside target range */
        write16(&rom, 0x40u, 0x4E75u);
        write16(&rom, 0x50u, 0x15C0u);
        write16(&rom, 0x60u, 0x4E75u);

        NgGameConfig config;
        ng_game_config_init(&config);
        config.state_table_count = 1u;
        config.state_tables[0].root = 0x20u;
        config.state_tables[0].table_start = 0x20u;
        config.state_tables[0].table_end = 0x30u;
        config.state_tables[0].stride = 4u;
        config.state_tables[0].sentinel = 0xFFFFFFFFu;
        config.state_tables[0].target_start = 0x40u;
        config.state_tables[0].target_end = 0x70u;
        config.state_tables[0].max_tables = 1u;
        config.state_tables[0].max_entries = 8u;
        config.state_tables[0].skip_invalid_targets = 1;

        const uint32_t seeds[] = {0x00000010u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(discovery.count == 3u);
        CHECK(ng_function_discovery_contains(&discovery, 0x10u));
        CHECK(ng_function_discovery_contains(&discovery, 0x40u));
        CHECK(ng_function_discovery_contains(&discovery, 0x60u));
        CHECK(!ng_function_discovery_contains(&discovery, 0x50u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x140u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x4E75u);       /* base seed */
        write16(&rom, 0x80u, 0x0800u);       /* tagged callback record */
        write32(&rom, 0x82u, 0x000000C0u);
        write16(&rom, 0x88u, 0x0900u);       /* wrong tag */
        write32(&rom, 0x8Au, 0x000000D0u);
        write16(&rom, 0x90u, 0x0800u);       /* outside target range */
        write32(&rom, 0x92u, 0x00000100u);
        write16(&rom, 0x98u, 0x0800u);       /* odd target */
        write32(&rom, 0x9Au, 0x000000C1u);
        write16(&rom, 0xC0u, 0x4E75u);
        write16(&rom, 0xD0u, 0x4E75u);
        write16(&rom, 0x100u, 0x4E75u);

        NgGameConfig config;
        ng_game_config_init(&config);
        config.jump_table_count = 1u;
        config.jump_tables[0].start = 0x80u;
        config.jump_tables[0].end = 0xA0u;
        config.jump_tables[0].stride = 2u;
        config.jump_tables[0].match = 0x0800u;
        config.jump_tables[0].target_offset = 2u;
        config.jump_tables[0].target_start = 0xC0u;
        config.jump_tables[0].target_end = 0xE0u;
        config.jump_tables[0].format = NG_GAME_CONFIG_JUMP_TABLE_TAGGED_ABS32;

        const uint32_t seeds[] = {0x00000010u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(discovery.count == 2u);
        CHECK(ng_function_discovery_contains(&discovery, 0x10u));
        CHECK(ng_function_discovery_contains(&discovery, 0xC0u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xD0u));
        CHECK(!ng_function_discovery_contains(&discovery, 0x100u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xC1u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xE0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x4E75u);       /* base seed */
        write32(&rom, 0x44u, 0x000000A0u);   /* record 0 callback +4 */
        write32(&rom, 0x48u, 0x000000B0u);   /* record 0 callback +8 */
        write32(&rom, 0x54u, 0xFFFFFFFFu);   /* record 1 sentinel +4 */
        write32(&rom, 0x58u, 0x000000C0u);   /* record 1 callback +8 */
        write32(&rom, 0x64u, 0x000000D0u);   /* outside target range */
        write32(&rom, 0x68u, 0x000000C1u);   /* odd target */
        write16(&rom, 0xA0u, 0x4E75u);
        write16(&rom, 0xB0u, 0x4E75u);
        write16(&rom, 0xC0u, 0x4E75u);
        write16(&rom, 0xD0u, 0x4E75u);

        NgGameConfig config;
        ng_game_config_init(&config);
        config.record_format_count = 1u;
        config.record_formats[0].width = 0x10u;
        config.record_formats[0].callback_offset_count = 2u;
        config.record_formats[0].callback_offsets[0] = 4u;
        config.record_formats[0].callback_offsets[1] = 8u;
        config.record_formats[0].sentinel = 0xFFFFFFFFu;
        config.record_formats[0].has_sentinel = 1;
        config.record_formats[0].target_start = 0xA0u;
        config.record_formats[0].target_end = 0xD0u;
        config.record_formats[0].scan_count = 1u;
        config.record_formats[0].scans[0].kind =
            NG_GAME_CONFIG_RECORD_SCAN_RANGE;
        config.record_formats[0].scans[0].start = 0x40u;
        config.record_formats[0].scans[0].end = 0x70u;

        const uint32_t seeds[] = {0x00000010u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(discovery.count == 4u);
        CHECK(ng_function_discovery_contains(&discovery, 0x10u));
        CHECK(ng_function_discovery_contains(&discovery, 0xA0u));
        CHECK(ng_function_discovery_contains(&discovery, 0xB0u));
        CHECK(ng_function_discovery_contains(&discovery, 0xC0u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xC1u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xD0u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x200u);
        CHECK(rom.data != NULL);
        ng_program_rom_set_address_map(&rom, 0x000000u, 0x100u, 0x200000u, 0x100u);

        write16(&rom, 0x10u, 0x4E75u);       /* base seed */
        write16(&rom, 0x40u, 0x0800u);       /* fixed tagged record */
        write32(&rom, 0x42u, 0x00000080u);
        write16(&rom, 0x48u, 0x0900u);       /* wrong tag */
        write32(&rom, 0x4Au, 0x00000088u);
        write16(&rom, 0x80u, 0x4E75u);
        write16(&rom, 0x88u, 0x4E75u);
        write16(&rom, 0x90u, 0x4E75u);
        write16(&rom, 0x120u, 0x0800u);      /* bank-window tagged record */
        write32(&rom, 0x122u, 0x00000090u);
        write16(&rom, 0x128u, 0x0800u);      /* outside target range */
        write32(&rom, 0x12Au, 0x000000A0u);

        NgGameConfig config;
        ng_game_config_init(&config);
        config.record_format_count = 1u;
        config.record_formats[0].tag = 0x0800u;
        config.record_formats[0].has_tag = 1;
        config.record_formats[0].stride = 2u;
        config.record_formats[0].callback_offset_count = 1u;
        config.record_formats[0].callback_offsets[0] = 2u;
        config.record_formats[0].target_start = 0x80u;
        config.record_formats[0].target_end = 0xA0u;
        config.record_formats[0].scan_count = 2u;
        config.record_formats[0].scans[0].kind =
            NG_GAME_CONFIG_RECORD_SCAN_FIXED;
        config.record_formats[0].scans[1].kind =
            NG_GAME_CONFIG_RECORD_SCAN_BANK_ALL;

        const uint32_t seeds[] = {0x00000010u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(discovery.count == 3u);
        CHECK(ng_function_discovery_contains(&discovery, 0x10u));
        CHECK(ng_function_discovery_contains(&discovery, 0x80u));
        CHECK(ng_function_discovery_contains(&discovery, 0x90u));
        CHECK(!ng_function_discovery_contains(&discovery, 0x88u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xA0u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xE0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x4E75u);       /* base seed */
        write32(&rom, 0x80u, 0x000000C0u);   /* in range and valid */
        write32(&rom, 0x84u, 0x000000D0u);   /* valid but outside range */
        write32(&rom, 0x88u, 0x000000C1u);   /* odd target */
        write32(&rom, 0x8Cu, 0x000000C4u);   /* in range but invalid code */
        write16(&rom, 0xC0u, 0x4E75u);
        write16(&rom, 0xC4u, 0x15C0u);
        write16(&rom, 0xD0u, 0x4E75u);

        NgGameConfig config;
        ng_game_config_init(&config);
        config.jump_table_count = 1u;
        config.jump_tables[0].start = 0x80u;
        config.jump_tables[0].end = 0x90u;
        config.jump_tables[0].stride = 4u;
        config.jump_tables[0].target_start = 0xC0u;
        config.jump_tables[0].target_end = 0xD0u;
        config.jump_tables[0].format = NG_GAME_CONFIG_JUMP_TABLE_ABS32;

        const uint32_t seeds[] = {0x00000010u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(discovery.count == 2u);
        CHECK(ng_function_discovery_contains(&discovery, 0x10u));
        CHECK(ng_function_discovery_contains(&discovery, 0xC0u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xD0u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xC1u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xC4u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x80u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x4E75u);       /* base seed */
        write16(&rom, 0x20u, 0x0004u);       /* inline callback opcode */
        write16(&rom, 0x22u, 0x20BCu);       /* MOVE.L #$30,(A0) */
        write32(&rom, 0x24u, 0x00000030u);
        write16(&rom, 0x28u, 0x4E75u);
        write16(&rom, 0x40u, 0x0004u);       /* callback shape missing */
        write16(&rom, 0x42u, 0x4E75u);       /* script store */

        NgGameConfig config;
        ng_game_config_init(&config);
        config.jump_table_count = 1u;
        config.jump_tables[0].start = 0x20u;
        config.jump_tables[0].end = 0x50u;
        config.jump_tables[0].stride = 2u;
        config.jump_tables[0].target_offset = 2u;
        config.jump_tables[0].target_start = 0x20u;
        config.jump_tables[0].target_end = 0x50u;
        config.jump_tables[0].format =
            NG_GAME_CONFIG_JUMP_TABLE_INLINE_CALLBACK;

        const uint32_t seeds[] = {0x00000010u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x10u));
        CHECK(ng_function_discovery_contains(&discovery, 0x22u));
        CHECK(!ng_function_discovery_contains(&discovery, 0x42u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xC0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x4EB9u); /* JSR $00000080 */
        write32(&rom, 0x12u, 0x00000080u);
        write16(&rom, 0x16u, 0x6100u); /* BSR $00000030 */
        write16(&rom, 0x18u, 0x0018u);
        write16(&rom, 0x1Au, 0x4EFAu); /* JMP $00000050 */
        write16(&rom, 0x1Cu, 0x0034u);
        write16(&rom, 0x30u, 0x4E75u);
        write16(&rom, 0x50u, 0x4E75u);
        write16(&rom, 0x80u, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x10u, &discovery));
        CHECK(discovery.count == 6u);
        CHECK(discovery.addrs[0] == 0x10u);
        CHECK(discovery.addrs[1] == 0x80u);
        CHECK(discovery.addrs[2] == 0x16u);
        CHECK(discovery.addrs[3] == 0x30u);
        CHECK(discovery.addrs[4] == 0x1Au);
        CHECK(discovery.addrs[5] == 0x50u);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x20u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x43FAu); /* LEA $000010(PC),A1 */
        write16(&rom, 0x02u, 0x000Eu);
        write16(&rom, 0x04u, 0x2C89u); /* MOVE.L A1,(A6): task state callback */
        write16(&rom, 0x06u, 0x4E75u);
        write16(&rom, 0x10u, 0x4E75u); /* callback after the caller's RTS */

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(discovery.count == 4u);
        CHECK(discovery.addrs[0] == 0x00u);
        CHECK(discovery.addrs[1] == 0x04u);
        CHECK(discovery.addrs[2] == 0x10u);
        CHECK(discovery.addrs[3] == 0x06u);
        CHECK(ng_function_discovery_contains(&discovery, 0x10u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x40u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x43D0u); /* LEA (A0),A1: no static target */
        write16(&rom, 0x12u, 0x2C89u); /* MOVE.L A1,(A6) */
        write16(&rom, 0x14u, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x10u, &discovery));
        CHECK(!ng_function_discovery_contains(&discovery, 0x00u));
        CHECK(ng_function_discovery_contains(&discovery, 0x10u));
        CHECK(ng_function_discovery_contains(&discovery, 0x12u));
        CHECK(ng_function_discovery_contains(&discovery, 0x14u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x40u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x43FAu);  /* LEA $000020(PC),A1 */
        write16(&rom, 0x02u, 0x001Eu);
        write16(&rom, 0x04u, 0x2D49u);  /* MOVE.L A1,($70,A6): task field callback */
        write16(&rom, 0x06u, 0x0070u);
        write16(&rom, 0x08u, 0x4E75u);
        write16(&rom, 0x20u, 0x4E75u);  /* callback after the caller's RTS */

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x20u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x80u);
        CHECK(rom.data != NULL);
        ng_program_rom_set_address_map(&rom, 0x000000u, 0x40u, 0x200000u, 0x40u);

        write16(&rom, 0x00u, 0x43F9u);  /* LEA $200020,A1: banked data */
        write32(&rom, 0x02u, 0x00200020u);
        write16(&rom, 0x06u, 0x2D49u);  /* MOVE.L A1,($70,A6) */
        write16(&rom, 0x08u, 0x0070u);
        write16(&rom, 0x0Au, 0x4E75u);
        write16(&rom, 0x60u, 0x4E75u);  /* mapped bank-window bytes */

        CHECK(ng_program_rom_addr_is_mapped(&rom, 0x200020u));
        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(!ng_function_discovery_contains(&discovery, 0x200020u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x40u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x43FAu);  /* LEA $000020(PC),A1 */
        write16(&rom, 0x02u, 0x001Eu);
        write16(&rom, 0x04u, 0x2D49u);  /* MOVE.L A1,($8,A6): ordinary field */
        write16(&rom, 0x06u, 0x0008u);
        write16(&rom, 0x08u, 0x4E75u);
        write16(&rom, 0x20u, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(!ng_function_discovery_contains(&discovery, 0x20u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x40u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x43FAu);  /* LEA $000020(PC),A1 */
        write16(&rom, 0x02u, 0x001Eu);
        write16(&rom, 0x04u, 0x2D49u);  /* MOVE.L A1,($3C,A6) */
        write16(&rom, 0x06u, 0x003Cu);
        write16(&rom, 0x08u, 0x4E75u);
        write16(&rom, 0x20u, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(!ng_function_discovery_contains(&discovery, 0x20u));

        NgGameConfig config;
        ng_game_config_init(&config);
        config.dispatcher_count = 1u;
        config.dispatchers[0].kind = NG_GAME_CONFIG_DISPATCHER_OBJECT_STATE;
        config.dispatchers[0].install_slot_count = 1u;
        config.dispatchers[0].install_slots[0] = 0x3Cu;
        uint32_t seed = 0x00u;
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    &seed,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x20u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x180u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x43FAu);       /* LEA $000020(PC),A1 */
        write16(&rom, 0x02u, 0x001Eu);
        write16(&rom, 0x04u, 0x4EB9u);       /* JSR spawn-helper wrapper */
        write32(&rom, 0x06u, 0x00000120u);
        write16(&rom, 0x0Au, 0x4E75u);
        write16(&rom, 0x20u, 0x4E75u);       /* spawned task callback */
        write16(&rom, 0x100u, 0x4E75u);      /* configured helper */
        write16(&rom, 0x120u, 0x4E71u);      /* wrapper preserves A1 */
        write16(&rom, 0x122u, 0x4EB9u);
        write32(&rom, 0x124u, 0x00000100u);
        write16(&rom, 0x128u, 0x4E75u);

        NgGameConfig config;
        ng_game_config_init(&config);
        config.dispatcher_count = 1u;
        config.dispatchers[0].kind = NG_GAME_CONFIG_DISPATCHER_OBJECT_STATE;
        config.dispatchers[0].spawn_helper_count = 1u;
        config.dispatchers[0].spawn_helpers[0] = 0x000100u;
        uint32_t seed = 0x00u;
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    &seed,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x20u));
        CHECK(ng_function_discovery_contains(&discovery, 0x100u));
        CHECK(ng_function_discovery_contains(&discovery, 0x120u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x500u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x43FAu);       /* LEA $000020(PC),A1 */
        write16(&rom, 0x02u, 0x001Eu);
        write16(&rom, 0x04u, 0x4EB9u);       /* JSR task spawn helper $0004AE */
        write32(&rom, 0x06u, 0x000004AEu);
        write16(&rom, 0x0Au, 0x4E75u);       /* caller continuation */
        write16(&rom, 0x20u, 0x4E75u);       /* spawned task callback */
        write16(&rom, 0x4AEu, 0x4E75u);      /* helper */

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x20u));
        CHECK(ng_function_discovery_contains(&discovery, 0x4AEu));
        CHECK(ng_function_discovery_contains(&discovery, 0x0Au));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x180u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x43FAu);       /* LEA $000020(PC),A1 */
        write16(&rom, 0x02u, 0x001Eu);
        write16(&rom, 0x04u, 0x4EB9u);       /* JSR configured spawn helper */
        write32(&rom, 0x06u, 0x00000100u);
        write16(&rom, 0x0Au, 0x4E75u);
        write16(&rom, 0x20u, 0x4E75u);       /* spawned task callback */
        write16(&rom, 0x100u, 0x4E75u);      /* helper */

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(!ng_function_discovery_contains(&discovery, 0x20u));
        CHECK(ng_function_discovery_contains(&discovery, 0x100u));

        NgGameConfig config;
        ng_game_config_init(&config);
        config.dispatcher_count = 1u;
        config.dispatchers[0].kind = NG_GAME_CONFIG_DISPATCHER_OBJECT_STATE;
        config.dispatchers[0].spawn_helper_count = 1u;
        config.dispatchers[0].spawn_helpers[0] = 0x000100u;
        uint32_t seed = 0x00u;
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    &seed,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x20u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x500u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x43D0u);       /* LEA (A0),A1: no static target */
        write16(&rom, 0x12u, 0x4EB9u);       /* JSR task spawn helper $0004AE */
        write32(&rom, 0x14u, 0x000004AEu);
        write16(&rom, 0x18u, 0x4E75u);
        write16(&rom, 0x4AEu, 0x4E75u);      /* helper */

        CHECK(ng_function_discover_from_entry(&rom, 0x10u, &discovery));
        CHECK(!ng_function_discovery_contains(&discovery, 0x00u));
        CHECK(ng_function_discovery_contains(&discovery, 0x4AEu));
        CHECK(ng_function_discovery_contains(&discovery, 0x18u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x20u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x4EBBu); /* JSR (4,PC,D0.W), runtime target */
        write16(&rom, 0x12u, 0x0004u);
        write16(&rom, 0x14u, 0x4E75u); /* continuation */
        write16(&rom, 0x16u, 0x4E75u); /* PC-index base, not a static target */

        CHECK(ng_function_discover_from_entry(&rom, 0x10u, &discovery));
        CHECK(discovery.count == 2u);
        CHECK(discovery.addrs[0] == 0x10u);
        CHECK(discovery.addrs[1] == 0x14u);
        CHECK(!ng_function_discovery_contains(&discovery, 0x16u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x20u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x4E72u); /* STOP #$2000 */
        write16(&rom, 0x02u, 0x2000u);
        write16(&rom, 0x04u, 0x4E75u); /* continuation after interrupt RTE */

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(discovery.count == 2u);
        CHECK(discovery.addrs[0] == 0x00u);
        CHECK(discovery.addrs[1] == 0x04u);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x20u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x7001u); /* MOVEQ #1,D0 */
        write16(&rom, 0x02u, 0x7202u); /* MOVEQ #2,D1 */
        write16(&rom, 0x04u, 0x4E75u); /* interrupt/RTE can resume here too */

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(discovery.count == 3u);
        CHECK(discovery.addrs[0] == 0x00u);
        CHECK(discovery.addrs[1] == 0x02u);
        CHECK(discovery.addrs[2] == 0x04u);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x40u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x6600u); /* BNE.W $000020 */
        write16(&rom, 0x02u, 0x001Eu);
        write16(&rom, 0x04u, 0x4E75u); /* fall-through return */
        write16(&rom, 0x20u, 0x4E75u); /* taken target return */

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x04u));
        CHECK(ng_function_discovery_contains(&discovery, 0x20u));
        CHECK(discovery.count == 3u);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x40u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x6000u); /* BRA.W $000020 */
        write16(&rom, 0x02u, 0x001Eu);
        write16(&rom, 0x04u, 0x4E75u); /* unreachable fall-through data/code */
        write16(&rom, 0x20u, 0x4E75u); /* taken target return */

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x20u));
        CHECK(!ng_function_discovery_contains(&discovery, 0x04u));
        CHECK(discovery.count == 2u);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x20u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x4E73u); /* RTE: returns to stacked PC, not fall-through */
        write16(&rom, 0x02u, 0x4E75u); /* unreachable fall-through data/code */

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(discovery.count == 1u);
        CHECK(discovery.addrs[0] == 0x00u);
        CHECK(!ng_function_discovery_contains(&discovery, 0x02u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x20u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x4E77u); /* RTR: returns to stacked PC, not fall-through */
        write16(&rom, 0x02u, 0x4E75u); /* unreachable fall-through data/code */

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(discovery.count == 1u);
        CHECK(discovery.addrs[0] == 0x00u);
        CHECK(!ng_function_discovery_contains(&discovery, 0x02u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x20u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x7001u); /* MOVEQ #1,D0 */
        write16(&rom, 0x02u, 0x15C0u); /* decoded UNKNOWN / invalid code frontier */
        write16(&rom, 0x04u, 0x4E75u); /* data-looking fall-through must not be seeded */

        CHECK(ng_function_discover_from_entry(&rom, 0x00u, &discovery));
        CHECK(discovery.count == 1u);
        CHECK(discovery.addrs[0] == 0x00u);
        CHECK(!ng_function_discovery_contains(&discovery, 0x02u));
        CHECK(!ng_function_discovery_contains(&discovery, 0x04u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xA0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x4EB9u); /* JSR $00000080 */
        write32(&rom, 0x12u, 0x00000080u);
        write16(&rom, 0x16u, 0x4E75u);
        write16(&rom, 0x40u, 0x4E75u);
        write16(&rom, 0x80u, 0x4E75u);

        const uint32_t seeds[] = {0x00000040u, 0x00000010u, 0x00000200u};
        CHECK(ng_function_discover_from_seeds(&rom, seeds, 3u, &discovery));
        CHECK(discovery.count == 4u);
        CHECK(discovery.addrs[0] == 0x40u);
        CHECK(discovery.addrs[1] == 0x10u);
        CHECK(discovery.addrs[2] == 0x80u);
        CHECK(discovery.addrs[3] == 0x16u);
        CHECK(!ng_function_discovery_contains(&discovery, 0x200u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x100u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x4E75u);       /* base seed */
        write32(&rom, 0x20u, 0x00000040u);   /* manual abs32 table target */
        write32(&rom, 0x24u, 0x00000050u);   /* manual abs32 table target */
        write16(&rom, 0x40u, 0x4E75u);
        write16(&rom, 0x50u, 0x4E75u);
        write16(&rom, 0x60u, 0x0020u);       /* manual pcrel16 target $80 */
        write16(&rom, 0x80u, 0x4E75u);
        write32(&rom, 0x94u, 0x000000C0u);   /* callback at +4 in 10-byte record */
        write32(&rom, 0x9Eu, 0x000000D0u);   /* callback at +4 in next record */
        write16(&rom, 0xC0u, 0x4E75u);
        write16(&rom, 0xD0u, 0x4E75u);

        NgGameConfig config;
        ng_game_config_init(&config);
        config.jump_table_count = 3u;
        config.jump_tables[0].start = 0x20u;
        config.jump_tables[0].end = 0x28u;
        config.jump_tables[0].stride = 4u;
        config.jump_tables[0].format = NG_GAME_CONFIG_JUMP_TABLE_ABS32;
        config.jump_tables[1].start = 0x60u;
        config.jump_tables[1].end = 0x62u;
        config.jump_tables[1].stride = 2u;
        config.jump_tables[1].format = NG_GAME_CONFIG_JUMP_TABLE_PCREL16;
        config.jump_tables[2].start = 0x94u;
        config.jump_tables[2].end = 0xA8u;
        config.jump_tables[2].stride = 0x0Au;
        config.jump_tables[2].format = NG_GAME_CONFIG_JUMP_TABLE_ABS32;

        const uint32_t seeds[] = {0x00000010u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(discovery.count == 6u);
        CHECK(discovery.addrs[0] == 0x10u);
        CHECK(discovery.addrs[1] == 0x40u);
        CHECK(discovery.addrs[2] == 0x50u);
        CHECK(discovery.addrs[3] == 0x80u);
        CHECK(discovery.addrs[4] == 0xC0u);
        CHECK(discovery.addrs[5] == 0xD0u);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xC0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x4E75u);       /* base seed */
        write16(&rom, 0x60u, 0x0C79u);       /* CMPI.W #$123,$00106F50 */
        write16(&rom, 0x62u, 0x0123u);
        write32(&rom, 0x64u, 0x00106F50u);
        write16(&rom, 0x68u, 0x55C0u);       /* SCS D0 */
        write16(&rom, 0x6Au, 0x43FAu);       /* LEA next,A1 */
        write16(&rom, 0x6Cu, 0x0004u);
        write16(&rom, 0x6Eu, 0x4E75u);       /* RTS */
        write16(&rom, 0x80u, 0x0C79u);       /* wrong abs target, ignored */
        write16(&rom, 0x82u, 0x0123u);
        write32(&rom, 0x84u, 0x00006F50u);
        write16(&rom, 0x88u, 0x55C0u);
        write16(&rom, 0x8Au, 0x43FAu);
        write16(&rom, 0x8Cu, 0x0004u);
        write16(&rom, 0x8Eu, 0x4E75u);
        write16(&rom, 0xA0u, 0x0C39u);       /* CMPI.B #$1,$0010E39A */
        write16(&rom, 0xA2u, 0x0001u);
        write32(&rom, 0xA4u, 0x0010E39Au);
        write16(&rom, 0xA8u, 0x56C0u);       /* SNE D0 */
        write16(&rom, 0xAAu, 0x43FAu);       /* LEA next,A1 */
        write16(&rom, 0xACu, 0x0004u);
        write16(&rom, 0xAEu, 0x4E75u);       /* RTS */

        NgGameConfig config;
        ng_game_config_init(&config);
        config.jump_table_count = 1u;
        config.jump_tables[0].start = 0x60u;
        config.jump_tables[0].end = 0xB0u;
        config.jump_tables[0].stride = 2u;
        config.jump_tables[0].format =
            NG_GAME_CONFIG_JUMP_TABLE_SCRIPT_PREDICATE;

        const uint32_t seeds[] = {0x00000010u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x10u));
        CHECK(ng_function_discovery_contains(&discovery, 0x60u));
        CHECK(ng_function_discovery_contains(&discovery, 0xA0u));
        CHECK(!ng_function_discovery_contains(&discovery, 0x80u));
        CHECK(ng_function_discovery_contains(&discovery, 0x68u));
        CHECK(ng_function_discovery_contains(&discovery, 0x6Au));
        CHECK(ng_function_discovery_contains(&discovery, 0x6Eu));
        CHECK(ng_function_discovery_contains(&discovery, 0xA8u));
        CHECK(ng_function_discovery_contains(&discovery, 0xAAu));
        CHECK(ng_function_discovery_contains(&discovery, 0xAEu));
        CHECK(discovery.count == 9u);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xD0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x4E75u);       /* base seed */
        write16(&rom, 0x40u, 0x45F9u);       /* LEA $00000090,A2 */
        write32(&rom, 0x42u, 0x00000090u);
        write16(&rom, 0x46u, 0x4EB9u);       /* JSR helper */
        write32(&rom, 0x48u, 0x000000A0u);
        write16(&rom, 0x4Cu, 0x4E75u);
        write16(&rom, 0x4Eu, 0x45F9u);       /* next fixed stub */
        write32(&rom, 0x50u, 0x00000098u);
        write16(&rom, 0x54u, 0x4EB9u);
        write32(&rom, 0x56u, 0x000000B0u);
        write16(&rom, 0x5Au, 0x4E75u);
        write16(&rom, 0x5Cu, 0x45F9u);       /* missing terminal */
        write32(&rom, 0x5Eu, 0x00000098u);
        write16(&rom, 0x64u, 0x4EB9u);
        write32(&rom, 0x66u, 0x000000B0u);
        write16(&rom, 0x90u, 0x4E75u);
        write16(&rom, 0x98u, 0x4E75u);
        write16(&rom, 0xA0u, 0x4E75u);
        write16(&rom, 0xB0u, 0x4E75u);

        NgGameConfig config;
        ng_game_config_init(&config);
        config.routine_table_count = 1u;
        config.routine_tables[0].stride = 0x0Eu;
        config.routine_tables[0].min_instructions = 3u;
        config.routine_tables[0].scan_count = 1u;
        config.routine_tables[0].scans[0].kind =
            NG_GAME_CONFIG_RECORD_SCAN_RANGE;
        config.routine_tables[0].scans[0].start = 0x40u;
        config.routine_tables[0].scans[0].end = 0x6Au;

        const uint32_t seeds[] = {0x00000010u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x10u));
        CHECK(ng_function_discovery_contains(&discovery, 0x40u));
        CHECK(ng_function_discovery_contains(&discovery, 0x4Eu));
        CHECK(!ng_function_discovery_contains(&discovery, 0x5Cu));
        CHECK(ng_function_discovery_contains(&discovery, 0xA0u));
        CHECK(ng_function_discovery_contains(&discovery, 0xB0u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xA0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x4E75u);       /* base seed */
        write16(&rom, 0x40u, 0x4E71u);       /* NOP */
        write16(&rom, 0x42u, 0x4E71u);       /* NOP */
        write16(&rom, 0x44u, 0x6000u);       /* BRA $000070 */
        write16(&rom, 0x46u, 0x002Au);
        write16(&rom, 0x4Au, 0x4E71u);       /* next fixed routine */
        write16(&rom, 0x4Cu, 0x4E71u);
        write16(&rom, 0x4Eu, 0x6000u);       /* BRA $000080 */
        write16(&rom, 0x50u, 0x0030u);
        write16(&rom, 0x54u, 0x4E71u);       /* no terminal in this slot */
        write16(&rom, 0x56u, 0x4E71u);
        write16(&rom, 0x58u, 0x4E71u);
        write16(&rom, 0x70u, 0x4E75u);
        write16(&rom, 0x80u, 0x4E75u);

        NgGameConfig config;
        ng_game_config_init(&config);
        config.routine_table_count = 1u;
        config.routine_tables[0].stride = 0x0Au;
        config.routine_tables[0].width = 0x08u;
        config.routine_tables[0].min_instructions = 3u;
        config.routine_tables[0].scan_count = 1u;
        config.routine_tables[0].scans[0].kind =
            NG_GAME_CONFIG_RECORD_SCAN_RANGE;
        config.routine_tables[0].scans[0].start = 0x40u;
        config.routine_tables[0].scans[0].end = 0x5Eu;

        const uint32_t seeds[] = {0x00000010u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x10u));
        CHECK(ng_function_discovery_contains(&discovery, 0x40u));
        CHECK(ng_function_discovery_contains(&discovery, 0x4Au));
        CHECK(!ng_function_discovery_contains(&discovery, 0x54u));
        CHECK(ng_function_discovery_contains(&discovery, 0x70u));
        CHECK(ng_function_discovery_contains(&discovery, 0x80u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x80u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x4E75u);       /* base seed */
        write16(&rom, 0x40u, 0x4E71u);       /* shared-tail fallthrough slot */
        write16(&rom, 0x42u, 0x4E71u);
        write16(&rom, 0x44u, 0x4E71u);
        write16(&rom, 0x46u, 0x4E75u);       /* shared tail target */

        NgGameConfig config;
        ng_game_config_init(&config);
        config.routine_table_count = 1u;
        config.routine_tables[0].stride = 0x0Au;
        config.routine_tables[0].width = 0x0Au;
        config.routine_tables[0].min_instructions = 3u;
        config.routine_tables[0].fallthrough_target = 0x46u;
        config.routine_tables[0].has_fallthrough_target = 1;
        config.routine_tables[0].scan_count = 1u;
        config.routine_tables[0].scans[0].kind =
            NG_GAME_CONFIG_RECORD_SCAN_RANGE;
        config.routine_tables[0].scans[0].start = 0x40u;
        config.routine_tables[0].scans[0].end = 0x4Au;

        const uint32_t seeds[] = {0x00000010u};
        CHECK(ng_function_discover_from_game_config(&rom,
                                                    seeds,
                                                    1u,
                                                    &config,
                                                    &discovery));
        CHECK(ng_function_discovery_contains(&discovery, 0x10u));
        CHECK(ng_function_discovery_contains(&discovery, 0x40u));
        CHECK(ng_function_discovery_contains(&discovery, 0x46u));

        ng_program_rom_free(&rom);
    }

    {
        const uint32_t seed_count = 5000u;
        NgProgramRom rom = make_rom(seed_count * 2u);
        uint32_t seeds[5000];
        CHECK(rom.data != NULL);

        for (uint32_t i = 0; i < seed_count; ++i) {
            uint32_t addr = i * 2u;
            write16(&rom, addr, 0x4E75u); /* RTS */
            seeds[i] = addr;
        }

        CHECK(ng_function_discover_from_seeds(&rom, seeds, seed_count, &discovery));
        CHECK(discovery.count == seed_count);
        CHECK(!discovery.truncated);
        CHECK(ng_function_discovery_contains(&discovery, 0u));
        CHECK(ng_function_discovery_contains(&discovery, (seed_count - 1u) * 2u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x20u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x00u, 0x4E75u);
        write16(&rom, 0x10u, 0x15C0u);

        ng_function_discovery_init(&discovery);
        CHECK(ng_function_discovery_add(&discovery, &rom, 0x00u));
        CHECK(!ng_function_discovery_add(&discovery, &rom, 0x10u));
        CHECK(!ng_function_discovery_add(&discovery, &rom, 0x11u));
        CHECK(discovery.count == 1u);
        CHECK(ng_function_discovery_contains(&discovery, 0x00u));
        CHECK(!ng_function_discovery_contains(&discovery, 0x10u));
        CHECK(!discovery.truncated);

        ng_program_rom_free(&rom);
    }

    {
        const uint32_t seed_count = NG_FUNCTION_DISCOVERY_MAX_CANDIDATES + 1u;
        NgProgramRom rom = make_rom(seed_count * 2u);
        CHECK(rom.data != NULL);

        ng_function_discovery_init(&discovery);
        for (uint32_t i = 0; i < seed_count; ++i) {
            uint32_t addr = i * 2u;
            write16(&rom, addr, 0x4E75u);
            ng_function_discovery_add(&discovery, &rom, addr);
        }

        CHECK(discovery.count == NG_FUNCTION_DISCOVERY_MAX_CANDIDATES);
        CHECK(discovery.truncated);
        CHECK(ng_function_discovery_contains(&discovery, 0u));
        CHECK(ng_function_discovery_contains(&discovery,
                                             (NG_FUNCTION_DISCOVERY_MAX_CANDIDATES - 1u) * 2u));
        CHECK(!ng_function_discovery_contains(&discovery,
                                              NG_FUNCTION_DISCOVERY_MAX_CANDIDATES * 2u));

        ng_program_rom_free(&rom);
    }

    return 0;
}
