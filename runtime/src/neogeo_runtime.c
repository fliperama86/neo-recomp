#include "ngrecomp/neogeo_runtime.h"

#include <stdio.h>
#include <string.h>

NgM68kState g_ng_m68k;

static const uint8_t *g_ng_neogeo_program_rom;
static uint32_t g_ng_neogeo_program_rom_size;
static uint8_t g_ng_neogeo_work_ram[0x10000u];
static uint8_t g_ng_neogeo_palette_ram[NG_NEO_PALETTE_BANK_BYTES *
                                       NG_NEO_PALETTE_BANKS];
static uint8_t g_ng_neogeo_palette_bank;
static uint8_t g_ng_m68k_interrupt_level;
static uint8_t g_ng_m68k_interrupt_vector;
static uint8_t g_ng_m68k_level7_edge;
static uint16_t g_ng_neogeo_irq_pending;
static uint16_t g_ng_neogeo_lspc_mode;
static uint16_t g_ng_neogeo_timer_stop;
static uint32_t g_ng_neogeo_timer_reload_value;
static uint32_t g_ng_neogeo_timer_counter_value;
static uint8_t g_ng_neogeo_timer_counter_loaded;
static uint16_t g_ng_neogeo_current_scanline;

static void ng_neogeo_reload_timer_counter(void) {
    g_ng_neogeo_timer_counter_value = g_ng_neogeo_timer_reload_value;
    g_ng_neogeo_timer_counter_loaded = 1u;
}

static int ng_neogeo_is_palette_addr(uint32_t addr) {
    return addr >= 0x00400000u && addr <= 0x007FFFFFu;
}

static uint32_t ng_neogeo_palette_offset(uint32_t addr) {
    return ((uint32_t)g_ng_neogeo_palette_bank * NG_NEO_PALETTE_BANK_BYTES) |
           (addr & (NG_NEO_PALETTE_BANK_BYTES - 1u));
}

uint8_t ng68k_read8(uint32_t addr) {
    addr &= 0x00FFFFFFu;
    if (addr <= 0x000FFFFFu) {
        return addr < g_ng_neogeo_program_rom_size && g_ng_neogeo_program_rom ?
            g_ng_neogeo_program_rom[addr] : 0xFFu;
    }
    if (addr >= 0x00100000u && addr <= 0x0010FFFFu) {
        return g_ng_neogeo_work_ram[addr & 0xFFFFu];
    }
    if (addr >= 0x00200000u && addr <= 0x002FFFFFu) {
        uint32_t rom_offset = 0x00100000u + (addr - 0x00200000u);
        return rom_offset < g_ng_neogeo_program_rom_size && g_ng_neogeo_program_rom ?
            g_ng_neogeo_program_rom[rom_offset] : 0xFFu;
    }
    if (ng_neogeo_is_palette_addr(addr)) {
        return g_ng_neogeo_palette_ram[ng_neogeo_palette_offset(addr)];
    }
    fprintf(stderr, "ng68k_read8 miss at $%06X\n", addr & 0xFFFFFFu);
    return 0xFF;
}

uint16_t ng68k_read16(uint32_t addr) {
    uint16_t hi = ng68k_read8(addr);
    uint16_t lo = ng68k_read8(addr + 1);
    return (uint16_t)((hi << 8) | lo);
}

uint32_t ng68k_read32(uint32_t addr) {
    uint32_t hi = ng68k_read16(addr);
    uint32_t lo = ng68k_read16(addr + 2);
    return (hi << 16) | lo;
}

void ng68k_write8(uint32_t addr, uint8_t value) {
    addr &= 0x00FFFFFFu;
    if (addr >= 0x00100000u && addr <= 0x0010FFFFu) {
        g_ng_neogeo_work_ram[addr & 0xFFFFu] = value;
        return;
    }
    if (ng_neogeo_is_palette_addr(addr)) {
        uint32_t offset = ng_neogeo_palette_offset(addr) & ~1u;
        g_ng_neogeo_palette_ram[offset] = value;
        g_ng_neogeo_palette_ram[offset + 1u] = value;
        return;
    }

    switch (addr) {
    case NG_NEO_REG_PALBANK1:
        g_ng_neogeo_palette_bank = 1u;
        return;
    case NG_NEO_REG_PALBANK0:
        g_ng_neogeo_palette_bank = 0u;
        return;
    case NG_NEO_REG_LSPCMODE + 1u:
        ng68k_write16(NG_NEO_REG_LSPCMODE,
                      (uint16_t)(((uint16_t)value << 8) | value));
        return;
    case NG_NEO_REG_TIMERHIGH + 1u:
        ng68k_write16(NG_NEO_REG_TIMERHIGH,
                      (uint16_t)(((uint16_t)value << 8) | value));
        return;
    case NG_NEO_REG_TIMERLOW + 1u:
        ng68k_write16(NG_NEO_REG_TIMERLOW,
                      (uint16_t)(((uint16_t)value << 8) | value));
        return;
    case NG_NEO_REG_IRQACK + 1u:
        ng_neogeo_ack_interrupts(value);
        return;
    case NG_NEO_REG_TIMERSTOP + 1u:
        ng68k_write16(NG_NEO_REG_TIMERSTOP,
                      (uint16_t)(((uint16_t)value << 8) | value));
        return;
    default:
        break;
    }
    fprintf(stderr, "ng68k_write8 miss at $%06X value=$%02X\n",
            addr & 0xFFFFFFu, value);
}

void ng68k_write16(uint32_t addr, uint16_t value) {
    addr &= 0x00FFFFFFu;
    if (ng_neogeo_is_palette_addr(addr)) {
        uint32_t offset = ng_neogeo_palette_offset(addr) & ~1u;
        g_ng_neogeo_palette_ram[offset] = (uint8_t)(value >> 8);
        g_ng_neogeo_palette_ram[offset + 1u] = (uint8_t)value;
        return;
    }

    switch (addr) {
    case NG_NEO_REG_LSPCMODE:
        g_ng_neogeo_lspc_mode = value;
        return;
    case NG_NEO_REG_TIMERHIGH:
        g_ng_neogeo_timer_reload_value =
            (g_ng_neogeo_timer_reload_value & 0x0000FFFFu) |
            ((uint32_t)value << 16);
        return;
    case NG_NEO_REG_TIMERLOW:
        g_ng_neogeo_timer_reload_value =
            (g_ng_neogeo_timer_reload_value & 0xFFFF0000u) |
            (uint32_t)value;
        if (g_ng_neogeo_lspc_mode & NG_NEO_LSPCMODE_TIMER_RELOAD_ON_WRITE) {
            ng_neogeo_reload_timer_counter();
        }
        return;
    case NG_NEO_REG_IRQACK:
        ng_neogeo_ack_interrupts(value);
        return;
    case NG_NEO_REG_TIMERSTOP:
        g_ng_neogeo_timer_stop = value;
        return;
    default:
        break;
    }
    ng68k_write8(addr, (uint8_t)(value >> 8));
    ng68k_write8(addr + 1, (uint8_t)value);
}

void ng68k_write32(uint32_t addr, uint32_t value) {
    ng68k_write16(addr, (uint16_t)(value >> 16));
    ng68k_write16(addr + 2, (uint16_t)value);
}

void ng_call_by_address(uint32_t addr) {
    ng_log_dispatch_miss(addr);
}

void ng_log_dispatch_miss(uint32_t addr) {
    fprintf(stderr, "dispatch miss at $%06X\n", addr & 0xFFFFFFu);
}

void ng_m68k_stop_until_interrupt(uint16_t sr) {
    fprintf(stderr, "m68k STOP until interrupt sr=$%04X\n", sr);
}

void ng_m68k_set_interrupt_level(uint8_t level, uint8_t vector) {
    level = (uint8_t)(level & 7u);
    if (level == 7u && g_ng_m68k_interrupt_level < 7u) {
        g_ng_m68k_level7_edge = 1u;
    } else if (level < 7u) {
        g_ng_m68k_level7_edge = 0u;
    }
    g_ng_m68k_interrupt_level = level;
    g_ng_m68k_interrupt_vector = vector;
}

void ng_m68k_clear_interrupt_level(void) {
    g_ng_m68k_interrupt_level = 0;
    g_ng_m68k_interrupt_vector = 0;
    g_ng_m68k_level7_edge = 0;
}

void ng_neogeo_set_program_rom(const uint8_t *data, uint32_t size) {
    g_ng_neogeo_program_rom = data;
    g_ng_neogeo_program_rom_size = size;
}

void ng_neogeo_reset_runtime(void) {
    g_ng_neogeo_irq_pending = 0;
    g_ng_neogeo_lspc_mode = 0;
    g_ng_neogeo_timer_stop = 0;
    g_ng_neogeo_timer_reload_value = 0;
    g_ng_neogeo_timer_counter_value = 0;
    g_ng_neogeo_timer_counter_loaded = 0;
    g_ng_neogeo_current_scanline = 0;
    memset(g_ng_neogeo_work_ram, 0, sizeof(g_ng_neogeo_work_ram));
    memset(g_ng_neogeo_palette_ram, 0, sizeof(g_ng_neogeo_palette_ram));
    g_ng_neogeo_palette_bank = 0;
    ng_m68k_clear_interrupt_level();
}

static void ng_neogeo_refresh_interrupt_level(void) {
    if (g_ng_neogeo_irq_pending & NG_NEO_IRQACK_RESET) {
        ng_m68k_set_interrupt_level(3, 27);
    } else if (g_ng_neogeo_irq_pending & NG_NEO_IRQACK_TIMER) {
        ng_m68k_set_interrupt_level(2, 26);
    } else if (g_ng_neogeo_irq_pending & NG_NEO_IRQACK_VBLANK) {
        ng_m68k_set_interrupt_level(1, 25);
    } else {
        ng_m68k_clear_interrupt_level();
    }
}

void ng_neogeo_request_vblank_interrupt(void) {
    g_ng_neogeo_irq_pending |= NG_NEO_IRQACK_VBLANK;
    ng_neogeo_refresh_interrupt_level();
}

void ng_neogeo_request_timer_interrupt(void) {
    g_ng_neogeo_irq_pending |= NG_NEO_IRQACK_TIMER;
    ng_neogeo_refresh_interrupt_level();
}

void ng_neogeo_request_reset_interrupt(void) {
    g_ng_neogeo_irq_pending |= NG_NEO_IRQACK_RESET;
    ng_neogeo_refresh_interrupt_level();
}

void ng_neogeo_ack_interrupts(uint16_t ack_mask) {
    g_ng_neogeo_irq_pending = (uint16_t)(g_ng_neogeo_irq_pending &
                                         (uint16_t)~(ack_mask & 0x0007u));
    ng_neogeo_refresh_interrupt_level();
}

void ng_neogeo_begin_vblank(void) {
    if (g_ng_neogeo_lspc_mode & NG_NEO_LSPCMODE_TIMER_RELOAD_ON_FRAME) {
        ng_neogeo_reload_timer_counter();
    }
    ng_neogeo_request_vblank_interrupt();
}

void ng_neogeo_advance_timer(uint32_t pixel_ticks) {
    while (pixel_ticks != 0u && g_ng_neogeo_timer_counter_loaded) {
        uint64_t ticks_until_zero =
            (uint64_t)g_ng_neogeo_timer_counter_value + 1u;

        if ((uint64_t)pixel_ticks < ticks_until_zero) {
            g_ng_neogeo_timer_counter_value -= pixel_ticks;
            return;
        }

        pixel_ticks = (uint32_t)((uint64_t)pixel_ticks - ticks_until_zero);
        g_ng_neogeo_timer_counter_value = 0;
        if (g_ng_neogeo_lspc_mode & NG_NEO_LSPCMODE_TIMER_ENABLE) {
            ng_neogeo_request_timer_interrupt();
        }
        if (g_ng_neogeo_lspc_mode & NG_NEO_LSPCMODE_TIMER_RELOAD_ON_ZERO) {
            ng_neogeo_reload_timer_counter();
        } else {
            g_ng_neogeo_timer_counter_loaded = 0;
        }
    }
}

void ng_neogeo_advance_scanline(void) {
    ng_neogeo_advance_timer(NG_NEO_NTSC_PIXELS_PER_SCANLINE);
    g_ng_neogeo_current_scanline =
        (uint16_t)((g_ng_neogeo_current_scanline + 1u) %
                   NG_NEO_NTSC_SCANLINES_PER_FRAME);
    if (g_ng_neogeo_current_scanline == 0u) {
        ng_neogeo_begin_vblank();
    }
}

void ng_neogeo_advance_frame(void) {
    ng_neogeo_begin_vblank();
    ng_neogeo_advance_timer(NG_NEO_NTSC_PIXELS_PER_SCANLINE *
                            NG_NEO_NTSC_SCANLINES_PER_FRAME);
}

uint16_t ng_neogeo_lspc_mode(void) {
    return g_ng_neogeo_lspc_mode;
}

uint16_t ng_neogeo_timer_stop(void) {
    return g_ng_neogeo_timer_stop;
}

uint32_t ng_neogeo_timer_reload(void) {
    return g_ng_neogeo_timer_reload_value;
}

uint32_t ng_neogeo_timer_counter(void) {
    return g_ng_neogeo_timer_counter_value;
}

uint16_t ng_neogeo_current_scanline(void) {
    return g_ng_neogeo_current_scanline;
}

int ng_m68k_take_interrupt(uint8_t current_mask, uint8_t *level, uint8_t *vector) {
    uint8_t pending_level = (uint8_t)(g_ng_m68k_interrupt_level & 7u);

    if (!level || !vector || pending_level == 0u) {
        return 0;
    }

    if (pending_level == 7u && g_ng_m68k_level7_edge) {
        g_ng_m68k_level7_edge = 0;
        *level = pending_level;
        *vector = g_ng_m68k_interrupt_vector;
        return 1;
    }

    if (pending_level <= (current_mask & 7u)) {
        return 0;
    }

    *level = pending_level;
    *vector = g_ng_m68k_interrupt_vector;
    return 1;
}
