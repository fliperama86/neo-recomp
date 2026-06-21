#include "ngrecomp/neogeo_runtime.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static uint32_t g_external_dispatch_addr;
static uint32_t g_external_dispatch_count;

static int record_external_dispatch(uint32_t addr) {
    g_external_dispatch_addr = addr;
    ++g_external_dispatch_count;
    return 1;
}

int main(void) {
    uint8_t level = 0;
    uint8_t vector = 0;
    const uint8_t program_rom[] = {
        0x12u, 0x34u, 0x56u, 0x78u,
        0x9Au, 0xBCu, 0xDEu, 0xF0u,
    };
    const uint8_t system_rom[] = {
        0xA1u, 0xB2u, 0xC3u, 0xD4u,
        0xE5u, 0xF6u, 0x07u, 0x18u,
    };
    const uint8_t vector_program_rom[0x84] = {
        [0x00] = 0x11u, [0x01] = 0x22u,
        [0x02] = 0x33u, [0x03] = 0x44u,
        [0x7F] = 0x7Fu, [0x80] = 0x80u,
    };
    const uint8_t vector_system_rom[0x84] = {
        [0x00] = 0xAAu, [0x01] = 0xBBu,
        [0x02] = 0xCCu, [0x03] = 0xDDu,
        [0x7F] = 0xF7u, [0x80] = 0x55u,
    };

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    ng_neogeo_reset_runtime();
    ng_m68k_clear_interrupt_level();
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(ng_neogeo_vblank_interrupts() == 0u);
    CHECK(ng_neogeo_timer_interrupts() == 0u);
    CHECK(ng_neogeo_irq_ack_writes() == 0u);
    CHECK(ng_neogeo_irq_pending() == 0u);
    CHECK(ng_neogeo_frame_count() == 0u);
    CHECK(ng_neogeo_work_ram_nonzero_bytes() == 0u);
    CHECK(ng_neogeo_work_ram_checksum() == 0u);
    CHECK(ng_neogeo_palette_ram_nonzero_bytes() == 0u);
    CHECK(ng_neogeo_palette_ram_checksum() == 0u);
    CHECK(ng_neogeo_palette_write_count() == 0u);
    CHECK(ng_neogeo_palette_nonzero_write_count() == 0u);
    CHECK(ng_neogeo_palette_last_addr() == 0u);
    CHECK(ng_neogeo_palette_last_value() == 0u);
    CHECK(ng_neogeo_palette_last_bank() == 0u);
    CHECK(ng_neogeo_palette_peak_nonzero_bytes() == 0u);
    CHECK(ng_neogeo_palette_peak_checksum() == 0u);
    CHECK(ng_neogeo_vram_nonzero_words() == 0u);
    CHECK(ng_neogeo_vram_checksum() == 0u);

    ng_neogeo_set_external_dispatch(record_external_dispatch);
    ng_call_by_address(0x01C00444u);
    CHECK(g_external_dispatch_count == 1u);
    CHECK(g_external_dispatch_addr == 0x00C00444u);
    ng_neogeo_reset_runtime();
    ng_call_by_address(0x00C004C2u);
    CHECK(g_external_dispatch_count == 2u);
    CHECK(g_external_dispatch_addr == 0x00C004C2u);
    ng_neogeo_set_external_dispatch(NULL);

    CHECK(ng68k_read8(NG_NEO_REG_DIPSW) == 0xFFu);
    CHECK(ng68k_read8(0x00310001u) == 0xFFu);
    CHECK(ng_neogeo_watchdog_kicks() == 0u);
    ng68k_write8(NG_NEO_REG_DIPSW, 0x00u);
    CHECK(ng_neogeo_watchdog_kicks() == 1u);
    ng68k_write8(0x00310001u, 0xA5u);
    CHECK(ng_neogeo_watchdog_kicks() == 2u);
    CHECK(ng68k_read8(NG_NEO_REG_DIPSW) == 0xFFu);
    ng_neogeo_reset_runtime();
    CHECK(ng_neogeo_watchdog_kicks() == 0u);
    CHECK(ng68k_read8(NG_NEO_REG_DIPSW) == 0xFFu);

    CHECK(ng68k_read8(NG_NEO_REG_P1CNT) == 0xFFu);
    CHECK(ng68k_read8(0x00310000u) == 0xFFu);
    CHECK(ng68k_read8(NG_NEO_REG_P2CNT) == 0xFFu);
    CHECK(ng68k_read8(0x00350000u) == 0xFFu);
    CHECK(ng68k_read8(NG_NEO_REG_STATUS_B) == 0xFFu);
    CHECK(ng68k_read8(0x00390000u) == 0xFFu);
    CHECK(ng_neogeo_port_output() == 0x00u);
    ng68k_write8(NG_NEO_REG_POUTPUT, 0x12u);
    CHECK(ng_neogeo_port_output() == 0x12u);
    CHECK(ng68k_read8(NG_NEO_REG_POUTPUT) == 0x12u);
    ng68k_write8(0x00380003u, 0x1Bu);
    CHECK(ng_neogeo_port_output() == 0x1Bu);
    ng68k_write8(0x00380021u, 0x2Cu);
    CHECK(ng_neogeo_port_output() == 0x2Cu);
    CHECK(ng68k_read8(0x00390021u) == 0x2Cu);
    ng_neogeo_reset_runtime();
    CHECK(ng_neogeo_port_output() == 0x00u);

    CHECK(ng_neogeo_sound_command() == 0x00u);
    CHECK(ng_neogeo_sound_reply() == 0xFFu);
    CHECK(ng68k_read8(NG_NEO_REG_SOUND) == 0xFFu);
    CHECK(ng68k_read8(0x00330000u) == 0xFFu);
    ng68k_write8(NG_NEO_REG_SOUND, 0x34u);
    CHECK(ng_neogeo_sound_command() == 0x34u);
    ng68k_write8(0x00330000u, 0x56u);
    CHECK(ng_neogeo_sound_command() == 0x56u);
    CHECK(ng_neogeo_sound_reply() == 0xFFu);
    ng_neogeo_reset_runtime();
    CHECK(ng_neogeo_sound_command() == 0x00u);
    CHECK(ng_neogeo_sound_reply() == 0xFFu);

    CHECK(ng_neogeo_shadow_enabled() == 0u);
    CHECK(ng_neogeo_bios_vectors_enabled() == 0u);
    CHECK(ng_neogeo_board_fix_enabled() == 0u);
    ng68k_write8(NG_NEO_REG_SHADOW, 0xFFu);
    CHECK(ng_neogeo_shadow_enabled() == 1u);
    ng68k_write8(NG_NEO_REG_NOSHADOW, 0xFFu);
    CHECK(ng_neogeo_shadow_enabled() == 0u);
    ng68k_write8(NG_NEO_REG_SWPBIOS, 0xFFu);
    CHECK(ng_neogeo_bios_vectors_enabled() == 1u);
    ng68k_write8(NG_NEO_REG_SWPROM, 0xFFu);
    CHECK(ng_neogeo_bios_vectors_enabled() == 0u);
    ng68k_write8(NG_NEO_REG_BRDFIX, 0xFFu);
    CHECK(ng_neogeo_board_fix_enabled() == 1u);
    ng68k_write8(NG_NEO_REG_CRTFIX, 0xFFu);
    CHECK(ng_neogeo_board_fix_enabled() == 0u);

    CHECK(ng_neogeo_interrupt_polls() == 0u);
    ng_neogeo_set_auto_vblank_interval(3u);
    CHECK(!ng_m68k_take_interrupt(7, &level, &vector));
    CHECK(!ng_m68k_take_interrupt(7, &level, &vector));
    CHECK(!ng_m68k_take_interrupt(7, &level, &vector));
    CHECK(ng_neogeo_interrupt_polls() == 3u);
    CHECK(ng_neogeo_vblank_interrupts() == 1u);
    CHECK(ng_neogeo_irq_pending() == NG_NEO_IRQACK_VBLANK);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_VBLANK);
    CHECK(ng_neogeo_irq_ack_writes() == 1u);
    CHECK(ng_neogeo_irq_pending() == 0u);
    ng_neogeo_set_auto_vblank_interval(0);
    ng_neogeo_reset_runtime();
    CHECK(ng_neogeo_vblank_interrupts() == 0u);
    CHECK(ng_neogeo_irq_ack_writes() == 0u);

    CHECK(ng_neogeo_current_scanline() == 0u);
    CHECK(ng_neogeo_frame_count() == 0u);
    ng_neogeo_set_auto_scanline_interval(2u);
    CHECK(!ng_m68k_take_interrupt(7, &level, &vector));
    CHECK(ng_neogeo_current_scanline() == 0u);
    CHECK(!ng_m68k_take_interrupt(7, &level, &vector));
    CHECK(ng_neogeo_current_scanline() == 1u);
    CHECK(ng_neogeo_frame_count() == 0u);
    CHECK(ng_neogeo_vblank_interrupts() == 0u);
    ng_neogeo_set_auto_scanline_interval(1u);
    for (uint32_t i = 1; i < NG_NEO_NTSC_SCANLINES_PER_FRAME; ++i) {
        CHECK(!ng_m68k_take_interrupt(7, &level, &vector));
    }
    CHECK(ng_neogeo_current_scanline() == 0u);
    CHECK(ng_neogeo_frame_count() == 1u);
    CHECK(ng_neogeo_vblank_interrupts() == 1u);
    CHECK(ng_neogeo_irq_pending() == NG_NEO_IRQACK_VBLANK);
    ng_neogeo_set_auto_scanline_interval(0);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_VBLANK);
    ng_neogeo_reset_runtime();

    ng_neogeo_set_program_rom(program_rom, (uint32_t)sizeof(program_rom));
    CHECK(ng68k_read8(0x000000u) == 0x12u);
    CHECK(ng68k_read16(0x000000u) == 0x1234u);
    CHECK(ng68k_read32(0x000000u) == 0x12345678u);
    CHECK(ng68k_read8(0x01000000u) == 0x12u);
    CHECK(ng68k_read8(0x000007u) == 0xF0u);
    CHECK(ng68k_read8(0x000008u) == 0xFFu);
    ng68k_write8(0x000000u, 0xAAu);
    CHECK(ng68k_read8(0x000000u) == 0x12u);

    ng_neogeo_set_system_rom(system_rom, (uint32_t)sizeof(system_rom));
    CHECK(ng68k_read8(0x00C00000u) == 0xA1u);
    CHECK(ng68k_read16(0x00C00000u) == 0xA1B2u);
    CHECK(ng68k_read32(0x00C00000u) == 0xA1B2C3D4u);
    CHECK(ng68k_read32(0x00C20000u) == 0xA1B2C3D4u);
    CHECK(ng68k_read8(0x00C00008u) == 0xFFu);
    ng68k_write8(0x00C00000u, 0x55u);
    CHECK(ng68k_read8(0x00C00000u) == 0xA1u);

    ng_neogeo_set_program_rom(vector_program_rom,
                              (uint32_t)sizeof(vector_program_rom));
    ng_neogeo_set_system_rom(vector_system_rom,
                             (uint32_t)sizeof(vector_system_rom));
    CHECK(ng68k_read32(0x000000u) == 0x11223344u);
    CHECK(ng68k_read8(0x00007Fu) == 0x7Fu);
    CHECK(ng68k_read8(0x000080u) == 0x80u);
    ng68k_write8(NG_NEO_REG_SWPBIOS, 0xFFu);
    CHECK(ng_neogeo_bios_vectors_enabled() == 1u);
    CHECK(ng68k_read32(0x000000u) == 0xAABBCCDDu);
    CHECK(ng68k_read8(0x00007Fu) == 0xF7u);
    CHECK(ng68k_read8(0x000080u) == 0x80u);
    ng68k_write8(NG_NEO_REG_SWPROM, 0xFFu);
    CHECK(ng_neogeo_bios_vectors_enabled() == 0u);
    CHECK(ng68k_read32(0x000000u) == 0x11223344u);
    ng_neogeo_reset_runtime();

    ng68k_write8(0x00100000u, 0xA5u);
    CHECK(ng68k_read8(0x00100000u) == 0xA5u);
    ng68k_write16(0x00100002u, 0xBEEFu);
    CHECK(ng68k_read16(0x00100002u) == 0xBEEFu);
    ng68k_write32(0x0010FFFCu, 0xCAFEBABEu);
    CHECK(ng68k_read32(0x0010FFFCu) == 0xCAFEBABEu);
    CHECK(ng68k_read8(0x01100000u) == 0xA5u);
    CHECK(ng_neogeo_work_ram_nonzero_bytes() == 7u);
    CHECK(ng_neogeo_work_ram_checksum() == 0x0592u);
    CHECK(!ng_neogeo_copy_work_ram(NULL, NG_NEO_WORK_RAM_BYTES));
    uint8_t *work_dump = (uint8_t *)calloc(NG_NEO_WORK_RAM_BYTES, 1u);
    CHECK(work_dump != NULL);
    CHECK(!ng_neogeo_copy_work_ram(work_dump, NG_NEO_WORK_RAM_BYTES - 1u));
    CHECK(ng_neogeo_copy_work_ram(work_dump, NG_NEO_WORK_RAM_BYTES));
    CHECK(work_dump[0x0000u] == 0xA5u);
    CHECK(work_dump[0x0002u] == 0xBEu);
    CHECK(work_dump[0x0003u] == 0xEFu);
    CHECK(work_dump[0xFFFCu] == 0xCAu);
    CHECK(work_dump[0xFFFDu] == 0xFEu);
    CHECK(work_dump[0xFFFEu] == 0xBAu);
    CHECK(work_dump[0xFFFFu] == 0xBEu);
    free(work_dump);

    ng68k_write16(0x00400000u, 0x1234u);
    CHECK(ng68k_read16(0x00400000u) == 0x1234u);
    CHECK(ng68k_read16(0x00402000u) == 0x1234u);
    CHECK(ng_neogeo_palette_ram_nonzero_bytes() == 2u);
    CHECK(ng_neogeo_palette_ram_checksum() == 0x46u);
    CHECK(ng_neogeo_palette_write_count() == 1u);
    CHECK(ng_neogeo_palette_nonzero_write_count() == 1u);
    CHECK(ng_neogeo_palette_last_addr() == 0x00400000u);
    CHECK(ng_neogeo_palette_last_value() == 0x1234u);
    CHECK(ng_neogeo_palette_last_bank() == 0u);
    CHECK(ng_neogeo_palette_peak_nonzero_bytes() == 2u);
    CHECK(ng_neogeo_palette_peak_checksum() == 0x46u);
    ng68k_write8(0x00400003u, 0x5Au);
    CHECK(ng68k_read16(0x00400002u) == 0x5A5Au);
    CHECK(ng_neogeo_palette_ram_nonzero_bytes() == 4u);
    CHECK(ng_neogeo_palette_ram_checksum() == 0xFAu);
    CHECK(ng_neogeo_palette_write_count() == 2u);
    CHECK(ng_neogeo_palette_nonzero_write_count() == 2u);
    CHECK(ng_neogeo_palette_last_addr() == 0x00400003u);
    CHECK(ng_neogeo_palette_last_value() == 0x5A5Au);
    CHECK(ng_neogeo_palette_peak_nonzero_bytes() == 4u);
    CHECK(ng_neogeo_palette_peak_checksum() == 0xFAu);
    ng68k_write8(NG_NEO_REG_PALBANK1, 0x00u);
    CHECK(ng68k_read16(0x00400000u) == 0x0000u);
    ng68k_write16(0x00400000u, 0xBEEFu);
    CHECK(ng68k_read16(0x00400000u) == 0xBEEFu);
    CHECK(ng_neogeo_palette_ram_nonzero_bytes() == 6u);
    CHECK(ng_neogeo_palette_ram_checksum() == 0x02A7u);
    CHECK(ng_neogeo_palette_write_count() == 3u);
    CHECK(ng_neogeo_palette_nonzero_write_count() == 3u);
    CHECK(ng_neogeo_palette_last_addr() == 0x00400000u);
    CHECK(ng_neogeo_palette_last_value() == 0xBEEFu);
    CHECK(ng_neogeo_palette_last_bank() == 1u);
    CHECK(ng_neogeo_palette_peak_nonzero_bytes() == 6u);
    CHECK(ng_neogeo_palette_peak_checksum() == 0x02A7u);
    ng68k_write8(0x003B001Fu, 0x00u);
    CHECK(ng68k_read16(0x00400000u) == 0x1234u);
    ng68k_write8(NG_NEO_REG_PALBANK1, 0x00u);
    CHECK(ng68k_read16(0x00400000u) == 0xBEEFu);
    CHECK(!ng_neogeo_copy_palette_ram(NULL, NG_NEO_PALETTE_RAM_BYTES));
    uint8_t *palette_dump = (uint8_t *)calloc(NG_NEO_PALETTE_RAM_BYTES, 1u);
    CHECK(palette_dump != NULL);
    CHECK(!ng_neogeo_copy_palette_ram(palette_dump, NG_NEO_PALETTE_RAM_BYTES - 1u));
    CHECK(ng_neogeo_copy_palette_ram(palette_dump, NG_NEO_PALETTE_RAM_BYTES));
    CHECK(palette_dump[0x0000u] == 0x12u);
    CHECK(palette_dump[0x0001u] == 0x34u);
    CHECK(palette_dump[0x0002u] == 0x5Au);
    CHECK(palette_dump[0x0003u] == 0x5Au);
    CHECK(palette_dump[NG_NEO_PALETTE_BANK_BYTES] == 0xBEu);
    CHECK(palette_dump[NG_NEO_PALETTE_BANK_BYTES + 1u] == 0xEFu);
    free(palette_dump);
    ng_neogeo_reset_runtime();
    CHECK(ng68k_read16(0x00400000u) == 0x0000u);
    CHECK(ng_neogeo_palette_ram_nonzero_bytes() == 0u);
    CHECK(ng_neogeo_palette_ram_checksum() == 0u);
    CHECK(ng_neogeo_palette_write_count() == 0u);
    CHECK(ng_neogeo_palette_nonzero_write_count() == 0u);
    CHECK(ng_neogeo_palette_last_addr() == 0u);
    CHECK(ng_neogeo_palette_last_value() == 0u);
    CHECK(ng_neogeo_palette_last_bank() == 0u);
    CHECK(ng_neogeo_palette_peak_nonzero_bytes() == 0u);
    CHECK(ng_neogeo_palette_peak_checksum() == 0u);

    ng68k_write16(0x00D00000u, 0x1111u);
    CHECK(ng68k_read16(0x00D00000u) == 0x0000u);
    ng68k_write8(0x003B001Du, 0x00u);
    ng68k_write16(0x00D00000u, 0xCAFEu);
    CHECK(ng68k_read16(0x00D00000u) == 0xCAFEu);
    CHECK(ng68k_read16(0x00D10000u) == 0xCAFEu);
    ng68k_write8(0x00D00003u, 0x5Au);
    CHECK(ng68k_read8(0x00D00003u) == 0x5Au);
    ng68k_write8(NG_NEO_REG_SRAMLOCK, 0x00u);
    ng68k_write16(0x00D00000u, 0x1234u);
    CHECK(ng68k_read16(0x00D00000u) == 0xCAFEu);

    ng_neogeo_reset_runtime();
    CHECK(ng_neogeo_work_ram_nonzero_bytes() == 0u);
    CHECK(ng_neogeo_work_ram_checksum() == 0u);
    CHECK(ng_neogeo_vram_addr() == 0u);
    CHECK(ng_neogeo_vram_mod() == 0u);
    CHECK(ng_neogeo_vram_nonzero_words() == 0u);
    CHECK(ng_neogeo_vram_checksum() == 0u);
    ng68k_write16(NG_NEO_REG_VRAMADDR, 0x0100u);
    ng68k_write16(NG_NEO_REG_VRAMMOD, 0x0002u);
    CHECK(ng_neogeo_vram_addr() == 0x0100u);
    CHECK(ng_neogeo_vram_mod() == 0x0002u);
    ng68k_write16(NG_NEO_REG_VRAMRW, 0xBEEFu);
    CHECK(ng_neogeo_vram_addr() == 0x0102u);
    CHECK(ng_neogeo_vram_nonzero_words() == 1u);
    CHECK(ng_neogeo_vram_checksum() == 0xBEEFu);
    ng68k_write16(NG_NEO_REG_VRAMADDR, 0x0100u);
    CHECK(ng68k_read16(NG_NEO_REG_VRAMRW) == 0xBEEFu);
    CHECK(ng_neogeo_vram_addr() == 0x0100u);
    ng68k_write8(NG_NEO_REG_VRAMMOD + 1u, 0xFFu);
    CHECK(ng_neogeo_vram_mod() == 0xFFFFu);
    ng68k_write8(NG_NEO_REG_VRAMADDR, 0x12u);
    CHECK(ng_neogeo_vram_addr() == 0x1212u);
    CHECK(!ng_neogeo_copy_vram(NULL, NG_NEO_VRAM_WORDS));
    uint16_t *vram_dump = (uint16_t *)calloc(NG_NEO_VRAM_WORDS, sizeof(*vram_dump));
    CHECK(vram_dump != NULL);
    CHECK(!ng_neogeo_copy_vram(vram_dump, NG_NEO_VRAM_WORDS - 1u));
    CHECK(ng_neogeo_copy_vram(vram_dump, NG_NEO_VRAM_WORDS));
    CHECK(vram_dump[0x0100u] == 0xBEEFu);
    free(vram_dump);

    uint8_t *large_program_rom = (uint8_t *)calloc(0x100004u, 1u);
    CHECK(large_program_rom != NULL);
    large_program_rom[0x100000u] = 0xCAu;
    large_program_rom[0x100001u] = 0xFEu;
    large_program_rom[0x100002u] = 0xBAu;
    large_program_rom[0x100003u] = 0xBEu;
    ng_neogeo_set_program_rom(large_program_rom, 0x100004u);
    CHECK(ng68k_read32(0x00200000u) == 0xCAFEBABEu);
    CHECK(ng68k_read32(0x01200000u) == 0xCAFEBABEu);
    free(large_program_rom);
    ng_neogeo_set_program_rom(program_rom, (uint32_t)sizeof(program_rom));

    ng_m68k_set_interrupt_level(4, 28);
    CHECK(!ng_m68k_take_interrupt(4, &level, &vector));
    CHECK(ng_m68k_take_interrupt(3, &level, &vector));
    CHECK(level == 4);
    CHECK(vector == 28);
    CHECK(!ng_m68k_take_interrupt(4, &level, &vector));

    ng_m68k_clear_interrupt_level();
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_m68k_set_interrupt_level(7, 31);
    CHECK(ng_m68k_take_interrupt(7, &level, &vector));
    CHECK(level == 7);
    CHECK(vector == 31);
    CHECK(!ng_m68k_take_interrupt(7, &level, &vector));
    CHECK(ng_m68k_take_interrupt(6, &level, &vector));
    CHECK(level == 7);
    CHECK(vector == 31);

    ng_m68k_set_interrupt_level(6, 30);
    CHECK(!ng_m68k_take_interrupt(7, &level, &vector));
    ng_m68k_set_interrupt_level(7, 31);
    CHECK(ng_m68k_take_interrupt(7, &level, &vector));
    CHECK(level == 7);
    CHECK(vector == 31);

    ng_m68k_clear_interrupt_level();
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_neogeo_request_vblank_interrupt();
    CHECK(ng_neogeo_vblank_interrupts() == 1u);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    CHECK(!ng_m68k_take_interrupt(1, &level, &vector));
    ng_neogeo_ack_interrupts(NG_NEO_IRQACK_VBLANK);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_neogeo_request_vblank_interrupt();
    ng_neogeo_request_timer_interrupt();
    CHECK(ng_m68k_take_interrupt(1, &level, &vector));
    CHECK(level == 2);
    CHECK(vector == 26);
    ng_neogeo_ack_interrupts(NG_NEO_IRQACK_TIMER);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    ng_neogeo_ack_interrupts(NG_NEO_IRQACK_VBLANK);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_neogeo_request_reset_interrupt();
    CHECK(ng_m68k_take_interrupt(2, &level, &vector));
    CHECK(level == 3);
    CHECK(vector == 27);
    ng68k_write16(0x003C000Cu, NG_NEO_IRQACK_RESET);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_neogeo_request_vblank_interrupt();
    ng_neogeo_request_timer_interrupt();
    ng68k_write16(0x003C000Cu, NG_NEO_IRQACK_TIMER);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    ng68k_write8(0x003C000Du, NG_NEO_IRQACK_VBLANK);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_neogeo_reset_runtime();
    CHECK(ng_neogeo_lspc_mode() == 0);
    CHECK(ng_neogeo_timer_reload() == 0);
    CHECK(ng_neogeo_timer_counter() == 0);
    ng68k_write16(NG_NEO_REG_TIMERHIGH, 0x1234u);
    ng68k_write16(NG_NEO_REG_TIMERLOW, 0x5678u);
    CHECK(ng_neogeo_timer_reload() == 0x12345678u);
    CHECK(ng_neogeo_timer_counter() == 0);
    ng68k_write16(NG_NEO_REG_LSPCMODE,
                  NG_NEO_LSPCMODE_TIMER_ENABLE |
                  NG_NEO_LSPCMODE_TIMER_RELOAD_ON_WRITE);
    CHECK(ng_neogeo_lspc_mode() == (NG_NEO_LSPCMODE_TIMER_ENABLE |
                                    NG_NEO_LSPCMODE_TIMER_RELOAD_ON_WRITE));
    ng68k_write16(NG_NEO_REG_TIMERHIGH, 0x0000u);
    ng68k_write16(NG_NEO_REG_TIMERLOW, 0x0004u);
    CHECK(ng_neogeo_timer_reload() == 0x00000004u);
    CHECK(ng_neogeo_timer_counter() == 0x00000004u);
    ng_neogeo_advance_timer(4);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));
    ng_neogeo_advance_timer(1);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 2);
    CHECK(vector == 26);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_TIMER);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_neogeo_reset_runtime();
    ng68k_write16(NG_NEO_REG_LSPCMODE,
                  NG_NEO_LSPCMODE_TIMER_ENABLE |
                  NG_NEO_LSPCMODE_TIMER_RELOAD_ON_WRITE |
                  NG_NEO_LSPCMODE_TIMER_RELOAD_ON_ZERO);
    ng68k_write16(NG_NEO_REG_TIMERLOW, 0x0001u);
    CHECK(ng_neogeo_timer_counter() == 1u);
    ng_neogeo_advance_timer(2);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 2);
    CHECK(vector == 26);
    CHECK(ng_neogeo_timer_counter() == 1u);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_TIMER);
    ng_neogeo_advance_timer(1);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));
    ng_neogeo_advance_timer(1);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 2);
    CHECK(vector == 26);

    ng_neogeo_reset_runtime();
    ng68k_write16(NG_NEO_REG_TIMERLOW, 0x0002u);
    ng68k_write16(NG_NEO_REG_LSPCMODE,
                  NG_NEO_LSPCMODE_TIMER_ENABLE |
                  NG_NEO_LSPCMODE_TIMER_RELOAD_ON_FRAME);
    ng_neogeo_begin_vblank();
    CHECK(ng_neogeo_frame_count() == 1u);
    CHECK(ng_neogeo_timer_counter() == 2u);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    ng_neogeo_advance_timer(3);
    CHECK(ng_m68k_take_interrupt(1, &level, &vector));
    CHECK(level == 2);
    CHECK(vector == 26);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_TIMER);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_VBLANK);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));

    ng_neogeo_reset_runtime();
    ng68k_write16(NG_NEO_REG_TIMERLOW, 0x0000u);
    ng_neogeo_advance_timer(16);
    CHECK(!ng_m68k_take_interrupt(0, &level, &vector));
    ng68k_write16(NG_NEO_REG_TIMERSTOP, 0x0001u);
    CHECK(ng_neogeo_timer_stop() == 0x0001u);

    ng_neogeo_reset_runtime();
    ng68k_write8(NG_NEO_REG_LSPCMODE + 1u,
                 NG_NEO_LSPCMODE_TIMER_ENABLE |
                 NG_NEO_LSPCMODE_TIMER_RELOAD_ON_WRITE);
    CHECK(ng_neogeo_lspc_mode() == 0x3030u);
    ng68k_write8(NG_NEO_REG_TIMERSTOP + 1u, 0x01u);
    CHECK(ng_neogeo_timer_stop() == 0x0101u);

    ng_neogeo_reset_runtime();
    CHECK(ng_neogeo_current_scanline() == 0);
    CHECK(ng_neogeo_frame_count() == 0u);
    ng_neogeo_advance_scanline();
    CHECK(ng_neogeo_current_scanline() == 1);
    CHECK(ng_neogeo_frame_count() == 0u);
    ng_neogeo_advance_frame();
    CHECK(ng_neogeo_current_scanline() == 1);
    CHECK(ng_neogeo_frame_count() == 1u);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_VBLANK);

    ng_neogeo_reset_runtime();
    for (uint32_t i = 0; i < NG_NEO_NTSC_SCANLINES_PER_FRAME; ++i) {
        ng_neogeo_advance_scanline();
    }
    CHECK(ng_neogeo_current_scanline() == 0);
    CHECK(ng_neogeo_frame_count() == 1u);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_VBLANK);

    ng_neogeo_reset_runtime();
    ng68k_write16(NG_NEO_REG_TIMERLOW,
                  (uint16_t)(NG_NEO_NTSC_PIXELS_PER_SCANLINE - 1u));
    ng68k_write16(NG_NEO_REG_LSPCMODE,
                  NG_NEO_LSPCMODE_TIMER_ENABLE |
                  NG_NEO_LSPCMODE_TIMER_RELOAD_ON_FRAME);
    ng_neogeo_advance_frame();
    CHECK(ng_neogeo_frame_count() == 1u);
    CHECK(ng_m68k_take_interrupt(1, &level, &vector));
    CHECK(level == 2);
    CHECK(vector == 26);
    ng68k_write16(NG_NEO_REG_IRQACK, NG_NEO_IRQACK_TIMER);
    CHECK(ng_m68k_take_interrupt(0, &level, &vector));
    CHECK(level == 1);
    CHECK(vector == 25);

    return 0;
}
