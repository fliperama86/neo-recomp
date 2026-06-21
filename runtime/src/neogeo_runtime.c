#include "ngrecomp/neogeo_runtime.h"

#include <stdio.h>
#include <string.h>

NgM68kState g_ng_m68k;

static const uint8_t *g_ng_neogeo_program_rom;
static uint32_t g_ng_neogeo_program_rom_size;
static const uint8_t *g_ng_neogeo_system_rom;
static uint32_t g_ng_neogeo_system_rom_size;
static uint8_t g_ng_neogeo_work_ram[NG_NEO_WORK_RAM_BYTES];
static uint8_t g_ng_neogeo_palette_ram[NG_NEO_PALETTE_RAM_BYTES];
static uint8_t g_ng_neogeo_palette_bank;
static uint32_t g_ng_neogeo_palette_nonzero_bytes;
static uint32_t g_ng_neogeo_palette_checksum;
static uint32_t g_ng_neogeo_palette_write_count;
static uint32_t g_ng_neogeo_palette_nonzero_write_count;
static uint32_t g_ng_neogeo_palette_last_addr;
static uint16_t g_ng_neogeo_palette_last_value;
static uint8_t g_ng_neogeo_palette_last_bank;
static uint32_t g_ng_neogeo_palette_peak_nonzero_bytes;
static uint32_t g_ng_neogeo_palette_peak_checksum;
static uint8_t g_ng_neogeo_backup_ram[NG_NEO_BACKUP_RAM_BYTES];
static uint8_t g_ng_neogeo_backup_ram_unlocked;
static uint32_t g_ng_neogeo_backup_ram_write_count;
static uint32_t g_ng_neogeo_backup_ram_last_addr;
static uint8_t g_ng_neogeo_backup_ram_last_value;
static uint16_t g_ng_neogeo_vram[NG_NEO_VRAM_WORDS];
static uint16_t g_ng_neogeo_vram_addr;
static uint16_t g_ng_neogeo_vram_mod;
static uint8_t g_ng_neogeo_shadow_enabled;
static uint8_t g_ng_neogeo_bios_vectors_enabled;
static uint8_t g_ng_neogeo_board_fix_enabled;
static uint8_t g_ng_neogeo_p1cnt = 0xFFu;
static uint8_t g_ng_neogeo_p2cnt = 0xFFu;
static uint8_t g_ng_neogeo_status_b = 0xFFu;
static uint8_t g_ng_neogeo_system_input_low = 0xFFu;
static uint32_t g_ng_neogeo_status_a_reads;
static uint8_t g_ng_neogeo_port_output;
static uint8_t g_ng_neogeo_sound_command;
static uint8_t g_ng_neogeo_sound_reply = 0xFFu;
static NgNeoSoundCommandEvent
    g_ng_neogeo_sound_command_queue[NG_NEO_SOUND_COMMAND_QUEUE_CAPACITY];
static uint32_t g_ng_neogeo_sound_command_queue_head;
static uint32_t g_ng_neogeo_sound_command_queue_tail;
static uint32_t g_ng_neogeo_sound_command_queue_count;
static uint32_t g_ng_neogeo_sound_command_queue_overflows;
static uint8_t g_ng_neogeo_dipswitch = 0xFFu;
static uint32_t g_ng_neogeo_watchdog_kicks;
static uint32_t g_ng_neogeo_last_watchdog_pc;
static uint32_t g_ng_neogeo_last_watchdog_addr;
static uint8_t g_ng_neogeo_last_watchdog_value;
static uint32_t g_ng_neogeo_watchdog_timeout_polls;
static uint32_t g_ng_neogeo_watchdog_last_kick_poll;
static uint32_t g_ng_neogeo_watchdog_max_gap_polls;
static uint32_t g_ng_neogeo_watchdog_timeout_cycles;
static uint64_t g_ng_neogeo_watchdog_last_kick_cycle;
static uint64_t g_ng_neogeo_watchdog_max_gap_cycles;
static uint32_t g_ng_neogeo_watchdog_reset_pc;
static uint32_t g_ng_neogeo_watchdog_reset_ssp;
static uint32_t g_ng_neogeo_watchdog_resets;
static uint8_t g_ng_neogeo_watchdog_reset_pending;
static uint32_t g_ng_neogeo_watchdog_last_reset_pc;
static uint32_t g_ng_neogeo_watchdog_last_reset_poll;
static uint64_t g_ng_neogeo_watchdog_last_reset_cycle;
static uint32_t g_ng_neogeo_system_latch_writes;
static uint32_t g_ng_neogeo_last_system_latch_pc;
static uint32_t g_ng_neogeo_last_system_latch_addr;
static uint8_t g_ng_neogeo_last_system_latch_value;
static uint32_t g_ng_neogeo_bios_vector_enable_writes;
static uint32_t g_ng_neogeo_bios_vector_disable_writes;
static uint32_t g_ng_neogeo_last_bios_vector_pc;
static uint32_t g_ng_neogeo_last_bios_vector_addr;
static uint8_t g_ng_neogeo_last_bios_vector_value;
static uint32_t g_ng_neogeo_last_port_output_pc;
static uint32_t g_ng_neogeo_last_port_output_addr;
static uint32_t g_ng_neogeo_last_sound_pc;
static uint32_t g_ng_neogeo_last_sound_addr;
static uint8_t g_ng_m68k_interrupt_level;
static uint8_t g_ng_m68k_interrupt_vector;
static uint8_t g_ng_m68k_level7_edge;
static uint16_t g_ng_neogeo_irq_pending;
static uint32_t g_ng_neogeo_vblank_interrupts;
static uint32_t g_ng_neogeo_timer_interrupts;
static uint32_t g_ng_neogeo_irq_ack_writes;
static uint32_t g_ng_neogeo_last_irq_ack_pc;
static uint16_t g_ng_neogeo_last_irq_ack_value;
static uint32_t g_ng_neogeo_last_interrupt_return_pc;
static uint8_t g_ng_neogeo_last_interrupt_level;
static uint8_t g_ng_neogeo_last_interrupt_vector;
static uint16_t g_ng_neogeo_lspc_mode;
static uint32_t g_ng_neogeo_last_lspc_write_pc;
static uint32_t g_ng_neogeo_last_lspc_write_addr;
static uint16_t g_ng_neogeo_last_lspc_write_value;
static uint16_t g_ng_neogeo_timer_stop;
static uint32_t g_ng_neogeo_timer_reload_value;
static uint32_t g_ng_neogeo_timer_counter_value;
static uint8_t g_ng_neogeo_timer_counter_loaded;
static uint16_t g_ng_neogeo_current_scanline;
static uint32_t g_ng_neogeo_frame_count;
static uint32_t g_ng_neogeo_interrupt_polls;
static uint32_t g_ng_neogeo_auto_vblank_interval;
static uint32_t g_ng_neogeo_auto_scanline_interval;
static uint64_t g_ng_neogeo_cpu_cycles;
static uint32_t g_ng_neogeo_scanline_cycle;
static uint8_t g_ng_neogeo_cycle_timing_active;
static NgExternalDispatchHandler g_ng_external_dispatch_handler;
static uint32_t g_ng_neogeo_read_miss_log_count;

static void ng_neogeo_log_read_miss(uint32_t addr) {
    if (g_ng_neogeo_read_miss_log_count < 32u) {
        fprintf(stderr,
                "ng68k_read8 miss at $%06X pc=$%06X sr=$%04X "
                "a0=$%08X a1=$%08X a2=$%08X a3=$%08X "
                "a4=$%08X a5=$%08X a6=$%08X a7=$%08X\n",
                addr & 0x00FFFFFFu,
                g_ng_m68k.pc & 0x00FFFFFFu,
                g_ng_m68k.sr & 0xFFFFu,
                g_ng_m68k.a[0],
                g_ng_m68k.a[1],
                g_ng_m68k.a[2],
                g_ng_m68k.a[3],
                g_ng_m68k.a[4],
                g_ng_m68k.a[5],
                g_ng_m68k.a[6],
                g_ng_m68k.a[7]);
    } else if (g_ng_neogeo_read_miss_log_count == 32u) {
        fprintf(stderr, "ng68k_read8 miss logging suppressed after 32 entries\n");
    }
    ++g_ng_neogeo_read_miss_log_count;
}

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

static void ng_neogeo_queue_sound_command(uint8_t command) {
    if (g_ng_neogeo_sound_command_queue_count >=
        NG_NEO_SOUND_COMMAND_QUEUE_CAPACITY) {
        g_ng_neogeo_sound_command_queue_tail =
            (g_ng_neogeo_sound_command_queue_tail + 1u) %
            NG_NEO_SOUND_COMMAND_QUEUE_CAPACITY;
        --g_ng_neogeo_sound_command_queue_count;
        ++g_ng_neogeo_sound_command_queue_overflows;
    }

    NgNeoSoundCommandEvent *event =
        &g_ng_neogeo_sound_command_queue[g_ng_neogeo_sound_command_queue_head];
    event->cpu_cycles = g_ng_neogeo_cpu_cycles;
    event->pc = g_ng_m68k.pc & 0x00FFFFFFu;
    event->command = command;
    g_ng_neogeo_sound_command_queue_head =
        (g_ng_neogeo_sound_command_queue_head + 1u) %
        NG_NEO_SOUND_COMMAND_QUEUE_CAPACITY;
    ++g_ng_neogeo_sound_command_queue_count;
}

static int ng_neogeo_is_status_a_addr(uint32_t addr) {
    return (addr & 0x00FE0001u) == 0x00320001u;
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

static int ng_neogeo_is_memcard_addr(uint32_t addr) {
    return addr >= 0x00800000u && addr <= 0x00BFFFFFu;
}

static int ng_neogeo_is_high_unmapped_addr(uint32_t addr) {
    return addr >= 0x00E00000u;
}

static void ng_neogeo_note_watchdog_kick(void) {
    uint32_t gap =
        g_ng_neogeo_interrupt_polls - g_ng_neogeo_watchdog_last_kick_poll;
    if (gap > g_ng_neogeo_watchdog_max_gap_polls) {
        g_ng_neogeo_watchdog_max_gap_polls = gap;
    }
    g_ng_neogeo_watchdog_last_kick_poll = g_ng_neogeo_interrupt_polls;
    uint64_t cycle_gap =
        g_ng_neogeo_cpu_cycles - g_ng_neogeo_watchdog_last_kick_cycle;
    if (cycle_gap > g_ng_neogeo_watchdog_max_gap_cycles) {
        g_ng_neogeo_watchdog_max_gap_cycles = cycle_gap;
    }
    g_ng_neogeo_watchdog_last_kick_cycle = g_ng_neogeo_cpu_cycles;
    g_ng_neogeo_watchdog_reset_pending = 0u;
}

static void ng_neogeo_update_watchdog_timeout(void) {
    if (g_ng_neogeo_watchdog_reset_pending) {
        return;
    }
    if (g_ng_neogeo_cycle_timing_active &&
        g_ng_neogeo_watchdog_timeout_cycles != 0u) {
        uint64_t cycle_gap =
            g_ng_neogeo_cpu_cycles - g_ng_neogeo_watchdog_last_kick_cycle;
        if (cycle_gap < g_ng_neogeo_watchdog_timeout_cycles) {
            return;
        }
        if (cycle_gap > g_ng_neogeo_watchdog_max_gap_cycles) {
            g_ng_neogeo_watchdog_max_gap_cycles = cycle_gap;
        }
        g_ng_neogeo_watchdog_reset_pending = 1u;
        g_ng_neogeo_watchdog_last_reset_pc = g_ng_m68k.pc & 0x00FFFFFFu;
        g_ng_neogeo_watchdog_last_reset_poll = g_ng_neogeo_interrupt_polls;
        g_ng_neogeo_watchdog_last_reset_cycle = g_ng_neogeo_cpu_cycles;
        return;
    }
    if (g_ng_neogeo_watchdog_timeout_polls == 0u) {
        return;
    }
    uint32_t gap =
        g_ng_neogeo_interrupt_polls - g_ng_neogeo_watchdog_last_kick_poll;
    if (gap < g_ng_neogeo_watchdog_timeout_polls) {
        return;
    }
    if (gap > g_ng_neogeo_watchdog_max_gap_polls) {
        g_ng_neogeo_watchdog_max_gap_polls = gap;
    }
    g_ng_neogeo_watchdog_reset_pending = 1u;
    g_ng_neogeo_watchdog_last_reset_pc = g_ng_m68k.pc & 0x00FFFFFFu;
    g_ng_neogeo_watchdog_last_reset_poll = g_ng_neogeo_interrupt_polls;
    g_ng_neogeo_watchdog_last_reset_cycle = g_ng_neogeo_cpu_cycles;
}

static void ng_neogeo_note_backup_ram_write(uint32_t offset, uint8_t value) {
    (void)offset;
    (void)value;
    /* Directory/high-water synthesis was an early partial-BIOS workaround, but
     * it aliases real game save data on Metal Slug.  Live hosts that bypass a
     * full cold BIOS boot should seed the MVS save-RAM directory explicitly. */
}

typedef struct NgNeoBackupSeedRun {
    uint16_t offset;
    uint16_t size;
    const uint8_t *data;
} NgNeoBackupSeedRun;

void ng_neogeo_seed_mslug_backup_ram(void) {
    /* CPU-visible bytes for the minimal Metal Slug MVS save-RAM directory
     * produced by a fresh MAME boot.  This stops BIOS backup services from
     * scanning zeroed directory metadata when the live host starts directly at
     * the cart header; the game-owned high-score block at $D00320 remains
     * zero here and is initialized/saved by cartridge code. */
    static const uint8_t run_0010[] = {
        0x42, 0x41, 0x43, 0x4b, 0x55, 0x50, 0x20, 0x52,
        0x41, 0x4d, 0x20, 0x4f, 0x4b, 0x20, 0x21, 0x80,
        0xa0, 0xe0, 0x40
    };
    static const uint8_t run_003a[] = {
        0x01, 0x01, 0x01, 0x02, 0x01, 0x01, 0x01, 0x03
    };
    static const uint8_t run_0044[] = {0x30, 0x3b};
    static const uint8_t run_0047[] = {0x02};
    static const uint8_t run_004c[] = {0x3b};
    static const uint8_t run_005a[] = {0x48, 0x04, 0x36, 0x14, 0xff, 0xff};
    static const uint8_t run_0104[] = {0x26, 0x06, 0x21};
    static const uint8_t run_010b[] = {0xd0, 0x83, 0x20};
    static const uint8_t run_010f[] = {0xd0, 0x9b, 0x20};
    static const uint8_t run_0113[] = {0xd0, 0x9b, 0xa0};
    static const uint8_t run_0117[] = {0xd0, 0xa0, 0x70};
    static const uint8_t run_0124[] = {0x02, 0x01};
    static const uint8_t run_012a[] = {0xff, 0xff};
    static const uint8_t run_012e[] = {0xff, 0xff};
    static const uint8_t run_0132[] = {0xff, 0xff};
    static const uint8_t run_0136[] = {0xff, 0xff};
    static const uint8_t run_013a[] = {0xff, 0xff};
    static const uint8_t run_013e[] = {0xff, 0xff};
    static const uint8_t run_0142[] = {0xff, 0xff, 0x26, 0x06, 0x21};
    static const uint8_t run_0166[] = {0xff, 0xff};
    static const uint8_t run_016e[] = {0xff, 0xff};
    static const uint8_t run_0176[] = {0xff, 0xff};
    static const uint8_t run_017e[] = {0xff, 0xff};
    static const uint8_t run_0186[] = {0xff, 0xff};
    static const uint8_t run_018e[] = {0xff, 0xff};
    static const uint8_t run_0196[] = {0xff, 0xff};
    static const uint8_t run_019e[] = {0xff, 0xff};
    static const uint8_t run_01a5[] = {0xd0, 0x95, 0x70};
    static const uint8_t run_01a9[] = {
        0xd0, 0xa1, 0xa0, 0xf0, 0xf0, 0xf0, 0xf0, 0xff, 0xff
    };
    static const uint8_t run_0220[] = {
        0xff, 0xff, 0xff, 0xff, 0x03, 0xff, 0x01, 0x03
    };
    static const uint8_t run_0229[] = {0x01, 0x01};
    static const uint8_t run_02a0[] = {
        0x4d, 0x45, 0x54, 0x41, 0x4c, 0x20, 0x53, 0x4c,
        0x55, 0x47, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20
    };
    static const NgNeoBackupSeedRun runs[] = {
        {0x0010u, sizeof(run_0010), run_0010},
        {0x003au, sizeof(run_003a), run_003a},
        {0x0044u, sizeof(run_0044), run_0044},
        {0x0047u, sizeof(run_0047), run_0047},
        {0x004cu, sizeof(run_004c), run_004c},
        {0x005au, sizeof(run_005a), run_005a},
        {0x0104u, sizeof(run_0104), run_0104},
        {0x010bu, sizeof(run_010b), run_010b},
        {0x010fu, sizeof(run_010f), run_010f},
        {0x0113u, sizeof(run_0113), run_0113},
        {0x0117u, sizeof(run_0117), run_0117},
        {0x0124u, sizeof(run_0124), run_0124},
        {0x012au, sizeof(run_012a), run_012a},
        {0x012eu, sizeof(run_012e), run_012e},
        {0x0132u, sizeof(run_0132), run_0132},
        {0x0136u, sizeof(run_0136), run_0136},
        {0x013au, sizeof(run_013a), run_013a},
        {0x013eu, sizeof(run_013e), run_013e},
        {0x0142u, sizeof(run_0142), run_0142},
        {0x0166u, sizeof(run_0166), run_0166},
        {0x016eu, sizeof(run_016e), run_016e},
        {0x0176u, sizeof(run_0176), run_0176},
        {0x017eu, sizeof(run_017e), run_017e},
        {0x0186u, sizeof(run_0186), run_0186},
        {0x018eu, sizeof(run_018e), run_018e},
        {0x0196u, sizeof(run_0196), run_0196},
        {0x019eu, sizeof(run_019e), run_019e},
        {0x01a5u, sizeof(run_01a5), run_01a5},
        {0x01a9u, sizeof(run_01a9), run_01a9},
        {0x0220u, sizeof(run_0220), run_0220},
        {0x0229u, sizeof(run_0229), run_0229},
        {0x02a0u, sizeof(run_02a0), run_02a0},
    };
    for (uint32_t i = 0; i < (uint32_t)(sizeof(runs) / sizeof(runs[0])); ++i) {
        memcpy(&g_ng_neogeo_backup_ram[runs[i].offset],
               runs[i].data,
               runs[i].size);
    }
}

static uint8_t ng_neogeo_hi_or_lo(uint16_t value, uint32_t addr) {
    return (addr & 1u) ? (uint8_t)value : (uint8_t)(value >> 8);
}

static uint16_t ng_neogeo_vram_data(void) {
    return g_ng_neogeo_vram[g_ng_neogeo_vram_addr];
}

static void ng_neogeo_update_palette_peak(void) {
    if (g_ng_neogeo_palette_nonzero_bytes >
        g_ng_neogeo_palette_peak_nonzero_bytes) {
        g_ng_neogeo_palette_peak_nonzero_bytes =
            g_ng_neogeo_palette_nonzero_bytes;
        g_ng_neogeo_palette_peak_checksum =
            g_ng_neogeo_palette_checksum;
    }
}

static void ng_neogeo_write_palette_byte(uint32_t offset, uint8_t value) {
    uint8_t old = g_ng_neogeo_palette_ram[offset];
    if (old == value) {
        return;
    }
    if (old != 0u) {
        --g_ng_neogeo_palette_nonzero_bytes;
    }
    if (value != 0u) {
        ++g_ng_neogeo_palette_nonzero_bytes;
    }
    g_ng_neogeo_palette_checksum =
        g_ng_neogeo_palette_checksum - old + value;
    g_ng_neogeo_palette_ram[offset] = value;
    ng_neogeo_update_palette_peak();
}

static void ng_neogeo_note_palette_write(uint32_t addr, uint16_t value) {
    ++g_ng_neogeo_palette_write_count;
    if (value != 0u) {
        ++g_ng_neogeo_palette_nonzero_write_count;
    }
    g_ng_neogeo_palette_last_addr = addr & 0x00FFFFFFu;
    g_ng_neogeo_palette_last_value = value;
    g_ng_neogeo_palette_last_bank = g_ng_neogeo_palette_bank;
}

static void ng_neogeo_write_lspc_word(uint32_t addr, uint16_t value);

static int ng_neogeo_is_system_rom_addr(uint32_t addr) {
    return addr >= 0x00C00000u && addr <= 0x00CFFFFFu;
}

static uint8_t ng_neogeo_read_system_rom(uint32_t offset) {
    offset %= NG_NEO_SYSTEM_ROM_BYTES;
    return offset < g_ng_neogeo_system_rom_size && g_ng_neogeo_system_rom ?
        g_ng_neogeo_system_rom[offset] : 0xFFu;
}

uint8_t ng68k_read8(uint32_t addr) {
    addr &= 0x00FFFFFFu;
    if (g_ng_neogeo_bios_vectors_enabled && addr <= 0x0000007Fu) {
        return ng_neogeo_read_system_rom(addr);
    }
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
    if (ng_neogeo_is_status_a_addr(addr)) {
        ++g_ng_neogeo_status_a_reads;
        return (uint8_t)(0xBFu | ((g_ng_neogeo_status_a_reads & 1u) ? 0x40u : 0x00u));
    }
    if (ng_neogeo_is_status_b_addr(addr)) {
        return g_ng_neogeo_status_b;
    }
    if (ng_neogeo_is_port_output_addr(addr)) {
        return g_ng_neogeo_system_input_low;
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
    if (ng_neogeo_is_memcard_addr(addr)) {
        return 0xFFu;
    }
    if (ng_neogeo_is_system_rom_addr(addr)) {
        return ng_neogeo_read_system_rom(addr - 0x00C00000u);
    }
    if (ng_neogeo_is_high_unmapped_addr(addr)) {
        /* MAME's MVS map treats $E00000-$FFFFFF as unmapped/open-bus reads.
           Until we model 68000 prefetch/open-bus values precisely, keep the
           existing idle value but do not spam diagnostics for expected probes
           like Metal Slug's $E0000B attract/demo table entry. */
        return 0xFFu;
    }
    ng_neogeo_log_read_miss(addr);
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
        uint16_t word_value = (uint16_t)(((uint16_t)value << 8) | value);
        ng_neogeo_write_palette_byte(offset, value);
        ng_neogeo_write_palette_byte(offset + 1u, value);
        ng_neogeo_note_palette_write(addr, word_value);
        return;
    }
    if (ng_neogeo_is_backup_ram_addr(addr)) {
        if (g_ng_neogeo_backup_ram_unlocked) {
            uint32_t offset = addr & (NG_NEO_BACKUP_RAM_BYTES - 1u);
            g_ng_neogeo_backup_ram[offset] = value;
            ng_neogeo_note_backup_ram_write(offset, value);
            ++g_ng_neogeo_backup_ram_write_count;
            g_ng_neogeo_backup_ram_last_addr = addr & 0x00FFFFFFu;
            g_ng_neogeo_backup_ram_last_value = value;
        }
        return;
    }
    if (ng_neogeo_is_system_rom_addr(addr)) {
        return;
    }
    if (ng_neogeo_is_memcard_addr(addr)) {
        return;
    }
    if (ng_neogeo_is_dipswitch_addr(addr)) {
        ++g_ng_neogeo_watchdog_kicks;
        g_ng_neogeo_last_watchdog_pc = g_ng_m68k.pc & 0x00FFFFFFu;
        g_ng_neogeo_last_watchdog_addr = addr & 0x00FFFFFFu;
        g_ng_neogeo_last_watchdog_value = value;
        ng_neogeo_note_watchdog_kick();
        return;
    }
    if (ng_neogeo_is_port_output_addr(addr)) {
        g_ng_neogeo_port_output = value;
        g_ng_neogeo_last_port_output_pc = g_ng_m68k.pc & 0x00FFFFFFu;
        g_ng_neogeo_last_port_output_addr = addr & 0x00FFFFFFu;
        return;
    }
    if (ng_neogeo_is_sound_addr(addr)) {
        g_ng_neogeo_sound_command = value;
        g_ng_neogeo_last_sound_pc = g_ng_m68k.pc & 0x00FFFFFFu;
        g_ng_neogeo_last_sound_addr = addr & 0x00FFFFFFu;
        ng_neogeo_queue_sound_command(value);
        return;
    }
    if (ng_neogeo_is_system_latch_addr(addr)) {
        ++g_ng_neogeo_system_latch_writes;
        g_ng_neogeo_last_system_latch_pc = g_ng_m68k.pc & 0x00FFFFFFu;
        g_ng_neogeo_last_system_latch_addr = addr & 0x00FFFFFFu;
        g_ng_neogeo_last_system_latch_value = value;
        switch (addr & 0x1Fu) {
        case NG_NEO_REG_NOSHADOW & 0x1Fu:
            g_ng_neogeo_shadow_enabled = 0u;
            return;
        case NG_NEO_REG_SHADOW & 0x1Fu:
            g_ng_neogeo_shadow_enabled = 1u;
            return;
        case NG_NEO_REG_SWPBIOS & 0x1Fu:
            g_ng_neogeo_bios_vectors_enabled = 1u;
            ++g_ng_neogeo_bios_vector_enable_writes;
            g_ng_neogeo_last_bios_vector_pc = g_ng_m68k.pc & 0x00FFFFFFu;
            g_ng_neogeo_last_bios_vector_addr = addr & 0x00FFFFFFu;
            g_ng_neogeo_last_bios_vector_value = value;
            return;
        case NG_NEO_REG_SWPROM & 0x1Fu:
            g_ng_neogeo_bios_vectors_enabled = 0u;
            ++g_ng_neogeo_bios_vector_disable_writes;
            g_ng_neogeo_last_bios_vector_pc = g_ng_m68k.pc & 0x00FFFFFFu;
            g_ng_neogeo_last_bios_vector_addr = addr & 0x00FFFFFFu;
            g_ng_neogeo_last_bios_vector_value = value;
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
    g_ng_neogeo_last_lspc_write_pc = g_ng_m68k.pc & 0x00FFFFFFu;
    g_ng_neogeo_last_lspc_write_addr = addr & 0x00FFFFFFu;
    g_ng_neogeo_last_lspc_write_value = value;
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
        g_ng_neogeo_last_irq_ack_pc = g_ng_m68k.pc & 0x00FFFFFFu;
        g_ng_neogeo_last_irq_ack_value = value;
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
        ng_neogeo_write_palette_byte(offset, (uint8_t)(value >> 8));
        ng_neogeo_write_palette_byte(offset + 1u, (uint8_t)value);
        ng_neogeo_note_palette_write(addr, value);
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
    g_ng_neogeo_last_irq_ack_pc = 0;
    g_ng_neogeo_last_irq_ack_value = 0;
    g_ng_neogeo_last_interrupt_return_pc = 0;
    g_ng_neogeo_last_interrupt_level = 0;
    g_ng_neogeo_last_interrupt_vector = 0;
    g_ng_neogeo_lspc_mode = 0;
    g_ng_neogeo_last_lspc_write_pc = 0;
    g_ng_neogeo_last_lspc_write_addr = 0;
    g_ng_neogeo_last_lspc_write_value = 0;
    g_ng_neogeo_timer_stop = 0;
    g_ng_neogeo_timer_reload_value = 0;
    g_ng_neogeo_timer_counter_value = 0;
    g_ng_neogeo_timer_counter_loaded = 0;
    g_ng_neogeo_current_scanline = 0;
    g_ng_neogeo_frame_count = 0;
    g_ng_neogeo_interrupt_polls = 0;
    g_ng_neogeo_cpu_cycles = 0;
    g_ng_neogeo_scanline_cycle = 0;
    g_ng_neogeo_cycle_timing_active = 0;
    g_ng_neogeo_read_miss_log_count = 0;
    g_ng_neogeo_watchdog_kicks = 0;
    g_ng_neogeo_last_watchdog_pc = 0;
    g_ng_neogeo_last_watchdog_addr = 0;
    g_ng_neogeo_last_watchdog_value = 0;
    g_ng_neogeo_watchdog_timeout_polls = 0;
    g_ng_neogeo_watchdog_last_kick_poll = 0;
    g_ng_neogeo_watchdog_max_gap_polls = 0;
    g_ng_neogeo_watchdog_timeout_cycles = 0;
    g_ng_neogeo_watchdog_last_kick_cycle = 0;
    g_ng_neogeo_watchdog_max_gap_cycles = 0;
    g_ng_neogeo_watchdog_reset_pc = 0;
    g_ng_neogeo_watchdog_reset_ssp = 0;
    g_ng_neogeo_watchdog_resets = 0;
    g_ng_neogeo_watchdog_reset_pending = 0;
    g_ng_neogeo_watchdog_last_reset_pc = 0;
    g_ng_neogeo_watchdog_last_reset_poll = 0;
    g_ng_neogeo_watchdog_last_reset_cycle = 0;
    g_ng_neogeo_system_latch_writes = 0;
    g_ng_neogeo_last_system_latch_pc = 0;
    g_ng_neogeo_last_system_latch_addr = 0;
    g_ng_neogeo_last_system_latch_value = 0;
    g_ng_neogeo_bios_vector_enable_writes = 0;
    g_ng_neogeo_bios_vector_disable_writes = 0;
    g_ng_neogeo_last_bios_vector_pc = 0;
    g_ng_neogeo_last_bios_vector_addr = 0;
    g_ng_neogeo_last_bios_vector_value = 0;
    g_ng_neogeo_p1cnt = 0xFFu;
    g_ng_neogeo_p2cnt = 0xFFu;
    g_ng_neogeo_status_b = 0xFFu;
    g_ng_neogeo_system_input_low = 0xFFu;
    g_ng_neogeo_status_a_reads = 0;
    g_ng_neogeo_port_output = 0;
    g_ng_neogeo_last_port_output_pc = 0;
    g_ng_neogeo_last_port_output_addr = 0;
    g_ng_neogeo_sound_command = 0;
    g_ng_neogeo_last_sound_pc = 0;
    g_ng_neogeo_last_sound_addr = 0;
    g_ng_neogeo_sound_reply = 0xFFu;
    g_ng_neogeo_sound_command_queue_head = 0;
    g_ng_neogeo_sound_command_queue_tail = 0;
    g_ng_neogeo_sound_command_queue_count = 0;
    g_ng_neogeo_sound_command_queue_overflows = 0;
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
    g_ng_neogeo_palette_nonzero_bytes = 0;
    g_ng_neogeo_palette_checksum = 0;
    g_ng_neogeo_palette_write_count = 0;
    g_ng_neogeo_palette_nonzero_write_count = 0;
    g_ng_neogeo_palette_last_addr = 0;
    g_ng_neogeo_palette_last_value = 0;
    g_ng_neogeo_palette_last_bank = 0;
    g_ng_neogeo_palette_peak_nonzero_bytes = 0;
    g_ng_neogeo_palette_peak_checksum = 0;
    g_ng_neogeo_backup_ram_unlocked = 0;
    g_ng_neogeo_backup_ram_write_count = 0;
    g_ng_neogeo_backup_ram_last_addr = 0;
    g_ng_neogeo_backup_ram_last_value = 0;
    ng_m68k_clear_interrupt_level();
}

void ng_neogeo_set_auto_vblank_interval(uint32_t interrupt_polls) {
    g_ng_neogeo_auto_vblank_interval = interrupt_polls;
}

void ng_neogeo_set_auto_scanline_interval(uint32_t interrupt_polls) {
    g_ng_neogeo_auto_scanline_interval = interrupt_polls;
}

void ng_neogeo_set_watchdog_timeout_polls(uint32_t interrupt_polls) {
    g_ng_neogeo_watchdog_timeout_polls = interrupt_polls;
    g_ng_neogeo_watchdog_last_kick_poll = g_ng_neogeo_interrupt_polls;
    g_ng_neogeo_watchdog_reset_pending = 0u;
}

void ng_neogeo_set_watchdog_timeout_cycles(uint32_t cpu_cycles) {
    g_ng_neogeo_watchdog_timeout_cycles = cpu_cycles;
    g_ng_neogeo_watchdog_last_kick_cycle = g_ng_neogeo_cpu_cycles;
    g_ng_neogeo_watchdog_reset_pending = 0u;
}

void ng_neogeo_set_watchdog_reset_vector(uint32_t pc, uint32_t ssp) {
    g_ng_neogeo_watchdog_reset_pc = pc & 0x00FFFFFFu;
    g_ng_neogeo_watchdog_reset_ssp = ssp;
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
    g_ng_neogeo_scanline_cycle = 0;
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

void ng_neogeo_advance_cpu_cycles(uint32_t cpu_cycles) {
    if (cpu_cycles == 0u) {
        return;
    }

    g_ng_neogeo_cycle_timing_active = 1u;
    while (cpu_cycles != 0u) {
        uint32_t cycles_until_scanline =
            NG_NEO_CPU_CYCLES_PER_SCANLINE - g_ng_neogeo_scanline_cycle;
        if (cpu_cycles < cycles_until_scanline) {
            g_ng_neogeo_scanline_cycle += cpu_cycles;
            g_ng_neogeo_cpu_cycles += cpu_cycles;
            ng_neogeo_update_watchdog_timeout();
            return;
        }

        g_ng_neogeo_cpu_cycles += cycles_until_scanline;
        cpu_cycles -= cycles_until_scanline;
        g_ng_neogeo_scanline_cycle = 0;
        ng_neogeo_advance_scanline();
        ng_neogeo_update_watchdog_timeout();
    }
}

uint32_t ng_neogeo_watchdog_kicks(void) {
    return g_ng_neogeo_watchdog_kicks;
}

uint32_t ng_neogeo_watchdog_timeout_polls(void) {
    return g_ng_neogeo_watchdog_timeout_polls;
}

uint32_t ng_neogeo_watchdog_timeout_cycles(void) {
    return g_ng_neogeo_watchdog_timeout_cycles;
}

uint32_t ng_neogeo_watchdog_last_kick_poll(void) {
    return g_ng_neogeo_watchdog_last_kick_poll;
}

uint32_t ng_neogeo_watchdog_max_gap_polls(void) {
    return g_ng_neogeo_watchdog_max_gap_polls;
}

uint64_t ng_neogeo_watchdog_last_kick_cycle(void) {
    return g_ng_neogeo_watchdog_last_kick_cycle;
}

uint64_t ng_neogeo_watchdog_max_gap_cycles(void) {
    return g_ng_neogeo_watchdog_max_gap_cycles;
}

uint32_t ng_neogeo_watchdog_resets(void) {
    return g_ng_neogeo_watchdog_resets;
}

uint8_t ng_neogeo_watchdog_reset_pending(void) {
    return g_ng_neogeo_watchdog_reset_pending;
}

uint32_t ng_neogeo_watchdog_last_reset_pc(void) {
    return g_ng_neogeo_watchdog_last_reset_pc;
}

uint32_t ng_neogeo_watchdog_last_reset_poll(void) {
    return g_ng_neogeo_watchdog_last_reset_poll;
}

uint64_t ng_neogeo_watchdog_last_reset_cycle(void) {
    return g_ng_neogeo_watchdog_last_reset_cycle;
}

uint32_t ng_neogeo_last_watchdog_pc(void) {
    return g_ng_neogeo_last_watchdog_pc;
}

uint32_t ng_neogeo_last_watchdog_addr(void) {
    return g_ng_neogeo_last_watchdog_addr;
}

uint8_t ng_neogeo_last_watchdog_value(void) {
    return g_ng_neogeo_last_watchdog_value;
}

uint32_t ng_neogeo_interrupt_polls(void) {
    return g_ng_neogeo_interrupt_polls;
}

uint64_t ng_neogeo_cpu_cycles(void) {
    return g_ng_neogeo_cpu_cycles;
}

uint32_t ng_neogeo_scanline_cycle(void) {
    return g_ng_neogeo_scanline_cycle;
}

uint8_t ng_neogeo_cycle_timing_active(void) {
    return g_ng_neogeo_cycle_timing_active;
}

uint8_t ng_neogeo_port_output(void) {
    return g_ng_neogeo_port_output;
}

uint32_t ng_neogeo_last_port_output_pc(void) {
    return g_ng_neogeo_last_port_output_pc;
}

uint32_t ng_neogeo_last_port_output_addr(void) {
    return g_ng_neogeo_last_port_output_addr;
}

uint8_t ng_neogeo_sound_command(void) {
    return g_ng_neogeo_sound_command;
}

uint32_t ng_neogeo_last_sound_pc(void) {
    return g_ng_neogeo_last_sound_pc;
}

uint32_t ng_neogeo_last_sound_addr(void) {
    return g_ng_neogeo_last_sound_addr;
}

uint8_t ng_neogeo_sound_reply(void) {
    return g_ng_neogeo_sound_reply;
}

void ng_neogeo_set_sound_reply(uint8_t reply) {
    g_ng_neogeo_sound_reply = reply;
}

uint32_t ng_neogeo_sound_command_events_available(void) {
    return g_ng_neogeo_sound_command_queue_count;
}

uint32_t ng_neogeo_sound_command_event_overflows(void) {
    return g_ng_neogeo_sound_command_queue_overflows;
}

int ng_neogeo_pop_sound_command_event(NgNeoSoundCommandEvent *out) {
    if (g_ng_neogeo_sound_command_queue_count == 0u) {
        return 0;
    }
    if (out) {
        *out = g_ng_neogeo_sound_command_queue[g_ng_neogeo_sound_command_queue_tail];
    }
    g_ng_neogeo_sound_command_queue_tail =
        (g_ng_neogeo_sound_command_queue_tail + 1u) %
        NG_NEO_SOUND_COMMAND_QUEUE_CAPACITY;
    --g_ng_neogeo_sound_command_queue_count;
    return 1;
}

uint8_t ng_neogeo_shadow_enabled(void) {
    return g_ng_neogeo_shadow_enabled;
}

uint8_t ng_neogeo_bios_vectors_enabled(void) {
    return g_ng_neogeo_bios_vectors_enabled;
}

uint8_t ng_neogeo_palette_bank(void) {
    return g_ng_neogeo_palette_bank;
}

uint32_t ng_neogeo_system_latch_writes(void) {
    return g_ng_neogeo_system_latch_writes;
}

uint32_t ng_neogeo_last_system_latch_pc(void) {
    return g_ng_neogeo_last_system_latch_pc;
}

uint32_t ng_neogeo_last_system_latch_addr(void) {
    return g_ng_neogeo_last_system_latch_addr;
}

uint8_t ng_neogeo_last_system_latch_value(void) {
    return g_ng_neogeo_last_system_latch_value;
}

uint32_t ng_neogeo_bios_vector_enable_writes(void) {
    return g_ng_neogeo_bios_vector_enable_writes;
}

uint32_t ng_neogeo_bios_vector_disable_writes(void) {
    return g_ng_neogeo_bios_vector_disable_writes;
}

uint32_t ng_neogeo_last_bios_vector_pc(void) {
    return g_ng_neogeo_last_bios_vector_pc;
}

uint32_t ng_neogeo_last_bios_vector_addr(void) {
    return g_ng_neogeo_last_bios_vector_addr;
}

uint8_t ng_neogeo_last_bios_vector_value(void) {
    return g_ng_neogeo_last_bios_vector_value;
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

uint32_t ng_neogeo_last_lspc_write_pc(void) {
    return g_ng_neogeo_last_lspc_write_pc;
}

uint32_t ng_neogeo_last_lspc_write_addr(void) {
    return g_ng_neogeo_last_lspc_write_addr;
}

uint16_t ng_neogeo_last_lspc_write_value(void) {
    return g_ng_neogeo_last_lspc_write_value;
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

uint32_t ng_neogeo_last_irq_ack_pc(void) {
    return g_ng_neogeo_last_irq_ack_pc;
}

uint16_t ng_neogeo_last_irq_ack_value(void) {
    return g_ng_neogeo_last_irq_ack_value;
}

uint16_t ng_neogeo_irq_pending(void) {
    return g_ng_neogeo_irq_pending;
}

uint32_t ng_neogeo_last_interrupt_return_pc(void) {
    return g_ng_neogeo_last_interrupt_return_pc;
}

uint8_t ng_neogeo_last_interrupt_level(void) {
    return g_ng_neogeo_last_interrupt_level;
}

uint8_t ng_neogeo_last_interrupt_vector(void) {
    return g_ng_neogeo_last_interrupt_vector;
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

uint32_t ng_neogeo_palette_ram_nonzero_bytes(void) {
    return g_ng_neogeo_palette_nonzero_bytes;
}

uint32_t ng_neogeo_palette_ram_checksum(void) {
    return g_ng_neogeo_palette_checksum;
}

uint32_t ng_neogeo_palette_write_count(void) {
    return g_ng_neogeo_palette_write_count;
}

uint32_t ng_neogeo_palette_nonzero_write_count(void) {
    return g_ng_neogeo_palette_nonzero_write_count;
}

uint32_t ng_neogeo_palette_last_addr(void) {
    return g_ng_neogeo_palette_last_addr;
}

uint16_t ng_neogeo_palette_last_value(void) {
    return g_ng_neogeo_palette_last_value;
}

uint8_t ng_neogeo_palette_last_bank(void) {
    return g_ng_neogeo_palette_last_bank;
}

uint32_t ng_neogeo_palette_peak_nonzero_bytes(void) {
    return g_ng_neogeo_palette_peak_nonzero_bytes;
}

uint32_t ng_neogeo_palette_peak_checksum(void) {
    return g_ng_neogeo_palette_peak_checksum;
}

uint8_t ng_neogeo_backup_ram_unlocked(void) {
    return g_ng_neogeo_backup_ram_unlocked;
}

uint32_t ng_neogeo_backup_ram_write_count(void) {
    return g_ng_neogeo_backup_ram_write_count;
}

uint32_t ng_neogeo_backup_ram_last_addr(void) {
    return g_ng_neogeo_backup_ram_last_addr;
}

uint8_t ng_neogeo_backup_ram_last_value(void) {
    return g_ng_neogeo_backup_ram_last_value;
}

uint32_t ng_neogeo_backup_ram_nonzero_bytes(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < sizeof(g_ng_neogeo_backup_ram); ++i) {
        if (g_ng_neogeo_backup_ram[i] != 0u) {
            ++count;
        }
    }
    return count;
}

uint32_t ng_neogeo_backup_ram_checksum(void) {
    uint32_t sum = 0;
    for (uint32_t i = 0; i < sizeof(g_ng_neogeo_backup_ram); ++i) {
        sum += g_ng_neogeo_backup_ram[i];
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

int ng_neogeo_copy_work_ram(uint8_t *out, uint32_t out_size) {
    if (!out || out_size < (uint32_t)sizeof(g_ng_neogeo_work_ram)) {
        return 0;
    }
    memcpy(out, g_ng_neogeo_work_ram, sizeof(g_ng_neogeo_work_ram));
    return 1;
}

int ng_neogeo_copy_palette_ram(uint8_t *out, uint32_t out_size) {
    if (!out || out_size < (uint32_t)sizeof(g_ng_neogeo_palette_ram)) {
        return 0;
    }
    memcpy(out, g_ng_neogeo_palette_ram, sizeof(g_ng_neogeo_palette_ram));
    return 1;
}

int ng_neogeo_copy_backup_ram(uint8_t *out, uint32_t out_size) {
    if (!out || out_size < (uint32_t)sizeof(g_ng_neogeo_backup_ram)) {
        return 0;
    }
    memcpy(out, g_ng_neogeo_backup_ram, sizeof(g_ng_neogeo_backup_ram));
    return 1;
}

int ng_neogeo_copy_vram(uint16_t *out_words, uint32_t out_word_count) {
    uint32_t word_count = (uint32_t)(sizeof(g_ng_neogeo_vram) /
                                    sizeof(g_ng_neogeo_vram[0]));
    if (!out_words || out_word_count < word_count) {
        return 0;
    }
    memcpy(out_words, g_ng_neogeo_vram, sizeof(g_ng_neogeo_vram));
    return 1;
}

int ng_m68k_take_reset(uint32_t *pc, uint32_t *ssp) {
    if (!g_ng_neogeo_watchdog_reset_pending) {
        return 0;
    }

    g_ng_neogeo_watchdog_reset_pending = 0u;
    ++g_ng_neogeo_watchdog_resets;
    g_ng_neogeo_watchdog_last_kick_poll = g_ng_neogeo_interrupt_polls;
    g_ng_neogeo_watchdog_last_kick_cycle = g_ng_neogeo_cpu_cycles;

    if (pc) {
        *pc = g_ng_neogeo_watchdog_reset_pc != 0u ?
            g_ng_neogeo_watchdog_reset_pc :
            (ng68k_read32(4u) & 0x00FFFFFFu);
    }
    if (ssp) {
        *ssp = g_ng_neogeo_watchdog_reset_ssp != 0u ?
            g_ng_neogeo_watchdog_reset_ssp :
            ng68k_read32(0u);
    }

    g_ng_neogeo_irq_pending = 0;
    g_ng_neogeo_bios_vectors_enabled = 0u;
    g_ng_neogeo_backup_ram_unlocked = 0u;
    ng_m68k_clear_interrupt_level();
    return 1;
}

int ng_m68k_take_interrupt(uint8_t current_mask, uint8_t *level, uint8_t *vector) {
    ++g_ng_neogeo_interrupt_polls;
    if (!g_ng_neogeo_cycle_timing_active &&
        g_ng_neogeo_auto_scanline_interval != 0u &&
        (g_ng_neogeo_interrupt_polls % g_ng_neogeo_auto_scanline_interval) == 0u) {
        ng_neogeo_advance_scanline();
    }
    if (!g_ng_neogeo_cycle_timing_active &&
        g_ng_neogeo_auto_vblank_interval != 0u &&
        (g_ng_neogeo_interrupt_polls % g_ng_neogeo_auto_vblank_interval) == 0u) {
        ng_neogeo_request_vblank_interrupt();
    }
    ng_neogeo_update_watchdog_timeout();

    uint8_t pending_level = (uint8_t)(g_ng_m68k_interrupt_level & 7u);

    if (!level || !vector || pending_level == 0u) {
        return 0;
    }

    if (pending_level == 7u && g_ng_m68k_level7_edge) {
        g_ng_m68k_level7_edge = 0;
        *level = pending_level;
        *vector = g_ng_m68k_interrupt_vector;
        g_ng_neogeo_last_interrupt_return_pc = g_ng_m68k.pc & 0x00FFFFFFu;
        g_ng_neogeo_last_interrupt_level = pending_level;
        g_ng_neogeo_last_interrupt_vector = g_ng_m68k_interrupt_vector;
        return 1;
    }

    if (pending_level <= (current_mask & 7u)) {
        return 0;
    }

    *level = pending_level;
    *vector = g_ng_m68k_interrupt_vector;
    g_ng_neogeo_last_interrupt_return_pc = g_ng_m68k.pc & 0x00FFFFFFu;
    g_ng_neogeo_last_interrupt_level = pending_level;
    g_ng_neogeo_last_interrupt_vector = g_ng_m68k_interrupt_vector;
    return 1;
}
