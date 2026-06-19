#include "ngrecomp/neogeo_runtime.h"

#include <stdio.h>
#include <string.h>

NgM68kState g_ng_m68k;

static const uint8_t *g_ng_neogeo_program_rom;
static uint32_t g_ng_neogeo_program_rom_size;
static const uint8_t *g_ng_neogeo_system_rom;
static uint32_t g_ng_neogeo_system_rom_size;
static uint8_t g_ng_neogeo_work_ram[0x10000u];
static uint8_t g_ng_neogeo_palette_ram[NG_NEO_PALETTE_BANK_BYTES *
                                       NG_NEO_PALETTE_BANKS];
static uint8_t g_ng_neogeo_palette_bank;
static uint8_t g_ng_neogeo_backup_ram[NG_NEO_BACKUP_RAM_BYTES];
static uint8_t g_ng_neogeo_backup_ram_unlocked;
static uint16_t g_ng_neogeo_vram[0x10000u];
static uint16_t g_ng_neogeo_vram_addr;
static uint16_t g_ng_neogeo_vram_mod;
static uint8_t g_ng_neogeo_shadow_enabled;
static uint8_t g_ng_neogeo_bios_vectors_enabled;
static uint8_t g_ng_neogeo_board_fix_enabled;
static uint8_t g_ng_neogeo_p1cnt = 0xFFu;
static uint8_t g_ng_neogeo_p2cnt = 0xFFu;
static uint8_t g_ng_neogeo_status_b = 0xFFu;
static uint8_t g_ng_neogeo_port_output;
static uint8_t g_ng_neogeo_sound_command;
static uint8_t g_ng_neogeo_sound_reply = 0xFFu;
static uint8_t g_ng_neogeo_dipswitch = 0xFFu;
static uint32_t g_ng_neogeo_watchdog_kicks;
static uint8_t g_ng_m68k_interrupt_level;
static uint8_t g_ng_m68k_interrupt_vector;
static uint8_t g_ng_m68k_level7_edge;
static uint16_t g_ng_neogeo_irq_pending;
static uint32_t g_ng_neogeo_vblank_interrupts;
static uint32_t g_ng_neogeo_timer_interrupts;
static uint32_t g_ng_neogeo_irq_ack_writes;
static uint16_t g_ng_neogeo_lspc_mode;
static uint16_t g_ng_neogeo_timer_stop;
static uint32_t g_ng_neogeo_timer_reload_value;
static uint32_t g_ng_neogeo_timer_counter_value;
static uint8_t g_ng_neogeo_timer_counter_loaded;
static uint16_t g_ng_neogeo_current_scanline;
static uint32_t g_ng_neogeo_frame_count;
static uint32_t g_ng_neogeo_interrupt_polls;
static uint32_t g_ng_neogeo_auto_vblank_interval;
static uint32_t g_ng_neogeo_auto_scanline_interval;
static NgExternalDispatchHandler g_ng_external_dispatch_handler;

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

static int ng_neogeo_is_backup_ram_addr(uint32_t addr) {
    return addr >= 0x00D00000u && addr <= 0x00DFFFFFu;
}

static int ng_neogeo_is_dipswitch_addr(uint32_t addr) {
    return (addr & 0x00FE0081u) == NG_NEO_REG_DIPSW;
}

static int ng_neogeo_is_p1cnt_addr(uint32_t addr) {
    return (addr & 0x00FE0001u) == NG_NEO_REG_P1CNT;
}

static int ng_neogeo_is_p2cnt_addr(uint32_t addr) {
    return (addr & 0x00FE0001u) == NG_NEO_REG_P2CNT;
}

static int ng_neogeo_is_sound_addr(uint32_t addr) {
    return (addr & 0x00FE0001u) == NG_NEO_REG_SOUND;
}

static int ng_neogeo_is_status_b_addr(uint32_t addr) {
    return (addr & 0x00FE0001u) == NG_NEO_REG_STATUS_B;
}

static int ng_neogeo_is_port_output_addr(uint32_t addr) {
    return (addr & 0x00FE0001u) == NG_NEO_REG_POUTPUT;
}

static int ng_neogeo_is_system_latch_addr(uint32_t addr) {
    return (addr & 0x00FE0001u) == 0x003A0001u;
}

static int ng_neogeo_is_lspc_addr(uint32_t addr) {
    return addr >= NG_NEO_REG_VRAMADDR && addr <= (NG_NEO_REG_TIMERSTOP + 1u);
}

static uint8_t ng_neogeo_hi_or_lo(uint16_t value, uint32_t addr) {
    return (addr & 1u) ? (uint8_t)value : (uint8_t)(value >> 8);
}

static uint16_t ng_neogeo_vram_data(void) {
    return g_ng_neogeo_vram[g_ng_neogeo_vram_addr];
}

static void ng_neogeo_write_lspc_word(uint32_t addr, uint16_t value);

static int ng_neogeo_is_system_rom_addr(uint32_t addr) {
    return addr >= 0x00C00000u && addr <= 0x00CFFFFFu;
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
    if (ng_neogeo_is_backup_ram_addr(addr)) {
        return g_ng_neogeo_backup_ram[addr & (NG_NEO_BACKUP_RAM_BYTES - 1u)];
    }
    if (ng_neogeo_is_p1cnt_addr(addr)) {
        return g_ng_neogeo_p1cnt;
    }
    if (ng_neogeo_is_dipswitch_addr(addr)) {
        return g_ng_neogeo_dipswitch;
    }
    if (ng_neogeo_is_p2cnt_addr(addr)) {
        return g_ng_neogeo_p2cnt;
    }
    if (ng_neogeo_is_sound_addr(addr)) {
        return g_ng_neogeo_sound_reply;
    }
    if (ng_neogeo_is_status_b_addr(addr)) {
        return g_ng_neogeo_status_b;
    }
    if (ng_neogeo_is_port_output_addr(addr)) {
        return g_ng_neogeo_port_output;
    }
    if (ng_neogeo_is_lspc_addr(addr)) {
        switch (addr & 0x00FFFFFEu) {
        case NG_NEO_REG_VRAMADDR:
        case NG_NEO_REG_VRAMRW:
            return ng_neogeo_hi_or_lo(ng_neogeo_vram_data(), addr);
        case NG_NEO_REG_VRAMMOD:
            return ng_neogeo_hi_or_lo(g_ng_neogeo_vram_mod, addr);
        case NG_NEO_REG_LSPCMODE:
            return ng_neogeo_hi_or_lo(g_ng_neogeo_lspc_mode, addr);
        case NG_NEO_REG_TIMERHIGH:
            return ng_neogeo_hi_or_lo((uint16_t)(g_ng_neogeo_timer_reload_value >> 16), addr);
        case NG_NEO_REG_TIMERLOW:
            return ng_neogeo_hi_or_lo((uint16_t)g_ng_neogeo_timer_reload_value, addr);
        case NG_NEO_REG_TIMERSTOP:
            return ng_neogeo_hi_or_lo(g_ng_neogeo_timer_stop, addr);
        default:
            return 0xFFu;
        }
    }
    if (ng_neogeo_is_system_rom_addr(addr)) {
        uint32_t rom_offset = (addr - 0x00C00000u) % NG_NEO_SYSTEM_ROM_BYTES;
        return rom_offset < g_ng_neogeo_system_rom_size && g_ng_neogeo_system_rom ?
            g_ng_neogeo_system_rom[rom_offset] : 0xFFu;
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
    if (ng_neogeo_is_backup_ram_addr(addr)) {
        if (g_ng_neogeo_backup_ram_unlocked) {
            g_ng_neogeo_backup_ram[addr & (NG_NEO_BACKUP_RAM_BYTES - 1u)] = value;
        }
        return;
    }
    if (ng_neogeo_is_system_rom_addr(addr)) {
        return;
    }
    if (ng_neogeo_is_dipswitch_addr(addr)) {
        ++g_ng_neogeo_watchdog_kicks;
        return;
    }
    if (ng_neogeo_is_port_output_addr(addr)) {
        g_ng_neogeo_port_output = value;
        return;
    }
    if (ng_neogeo_is_sound_addr(addr)) {
        g_ng_neogeo_sound_command = value;
        return;
    }
    if (ng_neogeo_is_system_latch_addr(addr)) {
        switch (addr & 0x1Fu) {
        case NG_NEO_REG_NOSHADOW & 0x1Fu:
            g_ng_neogeo_shadow_enabled = 0u;
            return;
        case NG_NEO_REG_SHADOW & 0x1Fu:
            g_ng_neogeo_shadow_enabled = 1u;
            return;
        case NG_NEO_REG_SWPBIOS & 0x1Fu:
            g_ng_neogeo_bios_vectors_enabled = 1u;
            return;
        case NG_NEO_REG_SWPROM & 0x1Fu:
            g_ng_neogeo_bios_vectors_enabled = 0u;
            return;
        case NG_NEO_REG_BRDFIX & 0x1Fu:
            g_ng_neogeo_board_fix_enabled = 1u;
            return;
        case NG_NEO_REG_CRTFIX & 0x1Fu:
            g_ng_neogeo_board_fix_enabled = 0u;
            return;
        case NG_NEO_REG_SRAMLOCK & 0x1Fu:
            g_ng_neogeo_backup_ram_unlocked = 0u;
            return;
        case NG_NEO_REG_SRAMUNLOCK & 0x1Fu:
            g_ng_neogeo_backup_ram_unlocked = 1u;
            return;
        case NG_NEO_REG_PALBANK1 & 0x1Fu:
            g_ng_neogeo_palette_bank = 1u;
            return;
        case NG_NEO_REG_PALBANK0 & 0x1Fu:
            g_ng_neogeo_palette_bank = 0u;
            return;
        default:
            return;
        }
    }
    if (ng_neogeo_is_lspc_addr(addr)) {
        ng_neogeo_write_lspc_word(addr & 0x00FFFFFEu,
                                  (uint16_t)(((uint16_t)value << 8) | value));
        return;
    }

    fprintf(stderr, "ng68k_write8 miss at $%06X value=$%02X\n",
            addr & 0xFFFFFFu, value);
}

static void ng_neogeo_write_lspc_word(uint32_t addr, uint16_t value) {
    switch (addr) {
    case NG_NEO_REG_VRAMADDR:
        g_ng_neogeo_vram_addr = value;
        return;
    case NG_NEO_REG_VRAMRW:
        g_ng_neogeo_vram[g_ng_neogeo_vram_addr] = value;
        g_ng_neogeo_vram_addr = (uint16_t)(g_ng_neogeo_vram_addr +
                                           g_ng_neogeo_vram_mod);
        return;
    case NG_NEO_REG_VRAMMOD:
        g_ng_neogeo_vram_mod = value;
        return;
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
}

void ng68k_write16(uint32_t addr, uint16_t value) {
    addr &= 0x00FFFFFFu;
    if (ng_neogeo_is_palette_addr(addr)) {
        uint32_t offset = ng_neogeo_palette_offset(addr) & ~1u;
        g_ng_neogeo_palette_ram[offset] = (uint8_t)(value >> 8);
        g_ng_neogeo_palette_ram[offset + 1u] = (uint8_t)value;
        return;
    }
    if (ng_neogeo_is_lspc_addr(addr)) {
        ng_neogeo_write_lspc_word(addr & 0x00FFFFFEu, value);
        return;
    }
    ng68k_write8(addr, (uint8_t)(value >> 8));
    ng68k_write8(addr + 1, (uint8_t)value);
}

void ng68k_write32(uint32_t addr, uint32_t value) {
    ng68k_write16(addr, (uint16_t)(value >> 16));
    ng68k_write16(addr + 2, (uint16_t)value);
}

void ng_call_by_address(uint32_t addr) {
    addr &= 0x00FFFFFFu;
    if (g_ng_external_dispatch_handler && g_ng_external_dispatch_handler(addr)) {
        return;
    }
    ng_log_dispatch_miss(addr);
}

void ng_neogeo_set_external_dispatch(NgExternalDispatchHandler handler) {
    g_ng_external_dispatch_handler = handler;
}

void ng_log_dispatch_miss(uint32_t addr) {
    fprintf(stderr, "dispatch miss at $%06X\n", addr & 0xFFFFFFu);
}

void ng_m68k_reset_devices(void) {
    /* RESET asserts the 68000's external reset line. The default runtime has
     * no attached secondary CPU/device reset model yet; keep the hook explicit
     * so generated code has an architectural side-effect boundary. */
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

void ng_neogeo_set_system_rom(const uint8_t *data, uint32_t size) {
    g_ng_neogeo_system_rom = data;
    g_ng_neogeo_system_rom_size = size;
}

void ng_neogeo_reset_runtime(void) {
    g_ng_neogeo_irq_pending = 0;
    g_ng_neogeo_vblank_interrupts = 0;
    g_ng_neogeo_timer_interrupts = 0;
    g_ng_neogeo_irq_ack_writes = 0;
    g_ng_neogeo_lspc_mode = 0;
    g_ng_neogeo_timer_stop = 0;
    g_ng_neogeo_timer_reload_value = 0;
    g_ng_neogeo_timer_counter_value = 0;
    g_ng_neogeo_timer_counter_loaded = 0;
    g_ng_neogeo_current_scanline = 0;
    g_ng_neogeo_frame_count = 0;
    g_ng_neogeo_interrupt_polls = 0;
    g_ng_neogeo_watchdog_kicks = 0;
    g_ng_neogeo_port_output = 0;
    g_ng_neogeo_sound_command = 0;
    g_ng_neogeo_sound_reply = 0xFFu;
    g_ng_neogeo_vram_addr = 0;
    g_ng_neogeo_vram_mod = 0;
    g_ng_neogeo_shadow_enabled = 0;
    g_ng_neogeo_bios_vectors_enabled = 0;
    g_ng_neogeo_board_fix_enabled = 0;
    memset(g_ng_neogeo_work_ram, 0, sizeof(g_ng_neogeo_work_ram));
    memset(g_ng_neogeo_vram, 0, sizeof(g_ng_neogeo_vram));
    memset(g_ng_neogeo_palette_ram, 0, sizeof(g_ng_neogeo_palette_ram));
    memset(g_ng_neogeo_backup_ram, 0, sizeof(g_ng_neogeo_backup_ram));
    g_ng_neogeo_palette_bank = 0;
    g_ng_neogeo_backup_ram_unlocked = 0;
    ng_m68k_clear_interrupt_level();
}

void ng_neogeo_set_auto_vblank_interval(uint32_t interrupt_polls) {
    g_ng_neogeo_auto_vblank_interval = interrupt_polls;
}

void ng_neogeo_set_auto_scanline_interval(uint32_t interrupt_polls) {
    g_ng_neogeo_auto_scanline_interval = interrupt_polls;
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
    ++g_ng_neogeo_vblank_interrupts;
    g_ng_neogeo_irq_pending |= NG_NEO_IRQACK_VBLANK;
    ng_neogeo_refresh_interrupt_level();
}

void ng_neogeo_request_timer_interrupt(void) {
    ++g_ng_neogeo_timer_interrupts;
    g_ng_neogeo_irq_pending |= NG_NEO_IRQACK_TIMER;
    ng_neogeo_refresh_interrupt_level();
}

void ng_neogeo_request_reset_interrupt(void) {
    g_ng_neogeo_irq_pending |= NG_NEO_IRQACK_RESET;
    ng_neogeo_refresh_interrupt_level();
}

void ng_neogeo_ack_interrupts(uint16_t ack_mask) {
    if (ack_mask & 0x0007u) {
        ++g_ng_neogeo_irq_ack_writes;
    }
    g_ng_neogeo_irq_pending = (uint16_t)(g_ng_neogeo_irq_pending &
                                         (uint16_t)~(ack_mask & 0x0007u));
    ng_neogeo_refresh_interrupt_level();
}

void ng_neogeo_begin_vblank(void) {
    ++g_ng_neogeo_frame_count;
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

uint32_t ng_neogeo_watchdog_kicks(void) {
    return g_ng_neogeo_watchdog_kicks;
}

uint32_t ng_neogeo_interrupt_polls(void) {
    return g_ng_neogeo_interrupt_polls;
}

uint8_t ng_neogeo_port_output(void) {
    return g_ng_neogeo_port_output;
}

uint8_t ng_neogeo_sound_command(void) {
    return g_ng_neogeo_sound_command;
}

uint8_t ng_neogeo_sound_reply(void) {
    return g_ng_neogeo_sound_reply;
}

uint8_t ng_neogeo_shadow_enabled(void) {
    return g_ng_neogeo_shadow_enabled;
}

uint8_t ng_neogeo_bios_vectors_enabled(void) {
    return g_ng_neogeo_bios_vectors_enabled;
}

uint8_t ng_neogeo_board_fix_enabled(void) {
    return g_ng_neogeo_board_fix_enabled;
}

uint16_t ng_neogeo_vram_addr(void) {
    return g_ng_neogeo_vram_addr;
}

uint16_t ng_neogeo_vram_mod(void) {
    return g_ng_neogeo_vram_mod;
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

uint32_t ng_neogeo_frame_count(void) {
    return g_ng_neogeo_frame_count;
}

uint32_t ng_neogeo_vblank_interrupts(void) {
    return g_ng_neogeo_vblank_interrupts;
}

uint32_t ng_neogeo_timer_interrupts(void) {
    return g_ng_neogeo_timer_interrupts;
}

uint32_t ng_neogeo_irq_ack_writes(void) {
    return g_ng_neogeo_irq_ack_writes;
}

uint16_t ng_neogeo_irq_pending(void) {
    return g_ng_neogeo_irq_pending;
}

uint32_t ng_neogeo_work_ram_nonzero_bytes(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < sizeof(g_ng_neogeo_work_ram); ++i) {
        if (g_ng_neogeo_work_ram[i] != 0u) {
            ++count;
        }
    }
    return count;
}

uint32_t ng_neogeo_work_ram_checksum(void) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < sizeof(g_ng_neogeo_work_ram); ++i) {
        sum += g_ng_neogeo_work_ram[i];
    }
    return sum;
}

uint32_t ng_neogeo_vram_nonzero_words(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < (uint32_t)(sizeof(g_ng_neogeo_vram) /
                                        sizeof(g_ng_neogeo_vram[0])); ++i) {
        if (g_ng_neogeo_vram[i] != 0u) {
            ++count;
        }
    }
    return count;
}

uint32_t ng_neogeo_vram_checksum(void) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < (uint32_t)(sizeof(g_ng_neogeo_vram) /
                                        sizeof(g_ng_neogeo_vram[0])); ++i) {
        sum += g_ng_neogeo_vram[i];
    }
    return sum;
}

int ng_m68k_take_interrupt(uint8_t current_mask, uint8_t *level, uint8_t *vector) {
    ++g_ng_neogeo_interrupt_polls;
    if (g_ng_neogeo_auto_scanline_interval != 0u &&
        (g_ng_neogeo_interrupt_polls % g_ng_neogeo_auto_scanline_interval) == 0u) {
        ng_neogeo_advance_scanline();
    }
    if (g_ng_neogeo_auto_vblank_interval != 0u &&
        (g_ng_neogeo_interrupt_polls % g_ng_neogeo_auto_vblank_interval) == 0u) {
        ng_neogeo_request_vblank_interrupt();
    }

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
