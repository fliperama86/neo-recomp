#include <SDL.h>

#include "ngrecomp/neogeo_runtime.h"
#include "ngrecomp/neogeo_video.h"
#include "p_rom.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NG_LIVE_PALETTE_WORDS (NG_NEO_PALETTE_RAM_BYTES / 2u)
#define NG_LIVE_PALETTE_BANK_AUTO UINT32_MAX
#define NG_LIVE_DEFAULT_DISPATCHES_PER_REFRESH 5000u
#define NG_LIVE_DEFAULT_FAST_FORWARD 0u
#define NG_LIVE_DEFAULT_SCALE 3u
#define NG_LIVE_DEFAULT_WATCHDOG_TIMEOUT_POLLS 250000u
#define NG_LIVE_FRAME_SYNC_DISPATCH_CHUNK 16u

void ng_generated_call(uint32_t addr);
void ng_generated_smoke_reset_dispatch_stats(void);
void ng_generated_smoke_set_dispatch_budget(uint64_t max_dispatches);
void ng_generated_smoke_set_scanline_poll_interval(uint32_t interval);
uint64_t ng_generated_smoke_dispatch_count(void);
uint64_t ng_generated_smoke_cart_dispatch_count(void);
uint64_t ng_generated_smoke_bios_dispatch_count(void);
uint32_t ng_generated_smoke_unique_dispatch_count(void);
int ng_generated_smoke_dispatch_hot_overflow(void);
int ng_generated_smoke_dispatch_budget_hit(void);
uint32_t ng_generated_smoke_dispatch_budget_stop_addr(void);
uint32_t ng_generated_smoke_last_dispatch_addr(void);
uint32_t ng_generated_smoke_last_cart_dispatch_addr(void);
uint32_t ng_generated_smoke_last_bios_dispatch_addr(void);
uint32_t ng_generated_smoke_recent_loop_period(void);
uint32_t ng_generated_smoke_recent_dispatch(uint32_t offset);
uint32_t ng_generated_smoke_dispatch_watch_count(void);
uint32_t ng_generated_smoke_dispatch_watch_addr(uint32_t index);
const char *ng_generated_smoke_dispatch_watch_label(uint32_t index);
uint64_t ng_generated_smoke_dispatch_watch_hits(uint32_t index);
uint64_t ng_generated_smoke_instruction_watch_hits(uint32_t index);

static int read_file(const char *path, uint8_t **out_data, uint32_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
        return 0;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fprintf(stderr, "failed to seek %s: %s\n", path, strerror(errno));
        fclose(f);
        return 0;
    }
    long size = ftell(f);
    if (size < 0 || (unsigned long)size > UINT32_MAX) {
        fprintf(stderr, "failed to size %s\n", path);
        fclose(f);
        return 0;
    }
    rewind(f);

    uint8_t *data = (uint8_t *)malloc((size_t)size ? (size_t)size : 1u);
    if (!data) {
        fprintf(stderr, "failed to allocate %ld-byte file buffer\n", size);
        fclose(f);
        return 0;
    }
    size_t got = fread(data, 1, (size_t)size, f);
    if (fclose(f) != 0 || got != (size_t)size) {
        fprintf(stderr, "failed to read %s\n", path);
        free(data);
        return 0;
    }

    *out_data = data;
    *out_size = (uint32_t)size;
    return 1;
}

static void byteswap_words(uint8_t *data, uint32_t size) {
    for (uint32_t i = 0; i + 1u < size; i += 2u) {
        uint8_t tmp = data[i];
        data[i] = data[i + 1u];
        data[i + 1u] = tmp;
    }
}

static int load_zoom_rom_file(const char *path,
                              uint8_t **out_data,
                              uint32_t *out_size,
                              int report_errors) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        if (report_errors) {
            fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
        }
        return 0;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        if (report_errors) {
            fprintf(stderr, "failed to seek %s: %s\n", path, strerror(errno));
        }
        fclose(f);
        return 0;
    }
    long size = ftell(f);
    if (size < 0) {
        if (report_errors) {
            fprintf(stderr, "failed to size %s: %s\n", path, strerror(errno));
        }
        fclose(f);
        return 0;
    }
    rewind(f);
    if (size != (long)NG_NEO_ZOOM_ROM_BYTES &&
        size != (long)(NG_NEO_ZOOM_ROM_BYTES * 2u)) {
        if (report_errors) {
            fprintf(stderr,
                    "%s has unexpected LO ROM size %ld (expected %u or %u)\n",
                    path,
                    size,
                    NG_NEO_ZOOM_ROM_BYTES,
                    NG_NEO_ZOOM_ROM_BYTES * 2u);
        }
        fclose(f);
        return 0;
    }

    uint8_t *data = (uint8_t *)malloc((size_t)size);
    if (!data) {
        if (report_errors) {
            fprintf(stderr, "failed to allocate LO ROM buffer\n");
        }
        fclose(f);
        return 0;
    }
    size_t got = fread(data, 1, (size_t)size, f);
    if (fclose(f) != 0 || got != (size_t)size) {
        if (report_errors) {
            fprintf(stderr, "failed to read %s\n", path);
        }
        free(data);
        return 0;
    }
    *out_data = data;
    *out_size = (uint32_t)size;
    return 1;
}

static int make_sibling_path(char *out,
                             size_t out_size,
                             const char *path,
                             const char *name) {
    const char *slash = strrchr(path, '/');
    const char *backslash = strrchr(path, '\\');
    const char *sep = slash;
    if (backslash && (!sep || backslash > sep)) {
        sep = backslash;
    }
    if (!sep) {
        int written = snprintf(out, out_size, "%s", name);
        return written >= 0 && (size_t)written < out_size;
    }

    size_t dir_len = (size_t)(sep - path) + 1u;
    if (dir_len >= out_size) {
        return 0;
    }
    memcpy(out, path, dir_len);
    int written = snprintf(out + dir_len, out_size - dir_len, "%s", name);
    return written >= 0 && (size_t)written < out_size - dir_len;
}

static int try_load_default_zoom_rom(const char *neo_path,
                                     uint8_t **out_data,
                                     uint32_t *out_size) {
    char path[1200];
    if (make_sibling_path(path, sizeof(path), neo_path, "000-lo.lo") &&
        load_zoom_rom_file(path, out_data, out_size, 0)) {
        return 1;
    }
    if (make_sibling_path(path, sizeof(path), neo_path, "ng-lo.rom") &&
        load_zoom_rom_file(path, out_data, out_size, 0)) {
        return 1;
    }
    return 0;
}

static uint16_t read_be_word(const uint8_t *data, uint32_t word_index) {
    uint32_t off = word_index * 2u;
    return (uint16_t)(((uint16_t)data[off] << 8) | (uint16_t)data[off + 1u]);
}

static uint32_t choose_palette_bank(const uint16_t *palette_words) {
    uint32_t best_bank = 0;
    uint32_t best_score = 0;
    for (uint32_t bank = 0; bank < NG_NEO_PALETTE_BANKS; ++bank) {
        uint32_t score = 0;
        const uint16_t *bank_words =
            palette_words + bank * NG_NEO_PALETTE_COLORS_PER_BANK;
        for (uint32_t i = 0; i < NG_NEO_PALETTE_COLORS_PER_BANK; ++i) {
            uint16_t word = bank_words[i];
            if (word != 0u && word != 0x7FFFu) {
                ++score;
            }
        }
        if (score > best_score) {
            best_score = score;
            best_bank = bank;
        }
    }
    return best_bank;
}

static uint8_t snapshot_auto_animation_counter(uint32_t frame_count, uint16_t lspc) {
    uint32_t speed = (uint32_t)(lspc >> 8);
    return (uint8_t)((frame_count + speed) / (speed + 1u));
}

static int live_external_dispatch(uint32_t addr) {
    addr &= 0x00FFFFFFu;
    if (addr < 0x00100000u ||
        (addr >= 0x00200000u && addr < 0x00300000u) ||
        (addr >= 0x00C00000u && addr <= 0x00CFFFFFu)) {
        ng_generated_call(addr);
        return 1;
    }
    return 0;
}

static int run_until_dispatch_count(uint64_t target_dispatches, uint32_t entry_addr) {
    if (ng_generated_smoke_dispatch_count() >= target_dispatches) {
        return 1;
    }
    ng_generated_smoke_set_dispatch_budget(target_dispatches);
    ng_generated_call(entry_addr);
    if (!ng_generated_smoke_dispatch_budget_hit() &&
        ng_generated_smoke_dispatch_count() < target_dispatches) {
        fprintf(stderr,
                "generated dispatch returned before target: dispatches=%llu target=%llu pc=$%06X\n",
                (unsigned long long)ng_generated_smoke_dispatch_count(),
                (unsigned long long)target_dispatches,
                g_ng_m68k.pc & 0x00FFFFFFu);
        return 0;
    }
    return 1;
}

static int run_dispatch_slice(uint64_t dispatches, uint32_t fallback_entry) {
    uint64_t current = ng_generated_smoke_dispatch_count();
    uint32_t addr = current == 0u ? fallback_entry : (g_ng_m68k.pc & 0x00FFFFFFu);
    return run_until_dispatch_count(current + dispatches, addr);
}

static int run_frame_synced_slice(uint64_t max_dispatches, uint32_t fallback_entry) {
    uint32_t start_frame = ng_neogeo_frame_count();
    uint64_t start = ng_generated_smoke_dispatch_count();
    uint64_t limit = UINT64_MAX;
    if (max_dispatches < UINT64_MAX - start) {
        limit = start + max_dispatches;
    }

    while (ng_generated_smoke_dispatch_count() < limit) {
        uint64_t current = ng_generated_smoke_dispatch_count();
        uint64_t remaining = limit - current;
        uint64_t step = remaining < NG_LIVE_FRAME_SYNC_DISPATCH_CHUNK ?
            remaining :
            NG_LIVE_FRAME_SYNC_DISPATCH_CHUNK;
        uint32_t addr = current == 0u ?
            fallback_entry :
            (g_ng_m68k.pc & 0x00FFFFFFu);
        if (step == 0u || !run_until_dispatch_count(current + step, addr)) {
            return 0;
        }
        if (ng_neogeo_frame_count() != start_frame) {
            return 1;
        }
    }
    return 1;
}

static int run_fast_forward(uint64_t target_dispatches, uint32_t entry_addr) {
    if (target_dispatches == 0u) {
        return 1;
    }

    uint64_t current = ng_generated_smoke_dispatch_count();
    if (current >= target_dispatches) {
        return 1;
    }

    uint64_t step = target_dispatches / 10u;
    if (step < 10000u) {
        step = target_dispatches;
    } else if (step > 100000u) {
        step = 100000u;
    }

    fprintf(stderr, "fast-forwarding %llu dispatches before opening SDL...\n",
            (unsigned long long)target_dispatches);
    while (current < target_dispatches) {
        uint64_t next = current + step;
        if (next < current || next > target_dispatches) {
            next = target_dispatches;
        }
        uint32_t addr = current == 0u ? entry_addr : (g_ng_m68k.pc & 0x00FFFFFFu);
        if (!run_until_dispatch_count(next, addr)) {
            return 0;
        }
        current = ng_generated_smoke_dispatch_count();
        fprintf(stderr,
                "  fast-forward %llu/%llu dispatches (frame=%u scanline=%u)\n",
                (unsigned long long)current,
                (unsigned long long)target_dispatches,
                ng_neogeo_frame_count(),
                ng_neogeo_current_scanline());
    }

    return 1;
}

static int render_live_frame(const NgNeoRomImage *image,
                             const uint8_t *zoom_rom,
                             uint32_t zoom_rom_size,
                             uint32_t *pixels,
                             uint32_t width,
                             uint32_t height) {
    uint8_t palette_ram[NG_NEO_PALETTE_RAM_BYTES];
    uint16_t palette_words[NG_LIVE_PALETTE_WORDS];
    uint16_t vram_words[NG_NEO_VRAM_WORDS];
    if (!ng_neogeo_copy_palette_ram(palette_ram, sizeof(palette_ram)) ||
        !ng_neogeo_copy_vram(vram_words, NG_NEO_VRAM_WORDS)) {
        fprintf(stderr, "failed to copy live video state\n");
        return 0;
    }
    for (uint32_t i = 0; i < NG_LIVE_PALETTE_WORDS; ++i) {
        palette_words[i] = read_be_word(palette_ram, i);
    }

    uint32_t palette_bank = choose_palette_bank(palette_words);
    NgNeoVideoRenderOptions options;
    memset(&options, 0, sizeof(options));
    options.zoom_rom = zoom_rom;
    options.zoom_rom_size = zoom_rom_size;
    options.auto_animation_counter =
        snapshot_auto_animation_counter(ng_neogeo_frame_count(),
                                        ng_neogeo_lspc_mode());
    options.auto_animation_disabled =
        (uint8_t)((ng_neogeo_lspc_mode() >> 3) & 1u);

    return ng_neogeo_video_render_frame_argb_with_options(
        image->s.data,
        image->s.size,
        image->c.data,
        image->c.size,
        &options,
        vram_words,
        NG_NEO_VRAM_WORDS,
        palette_words + palette_bank * NG_NEO_PALETTE_COLORS_PER_BANK,
        NG_NEO_PALETTE_COLORS_PER_BANK,
        pixels,
        width,
        height,
        width);
}

typedef struct NgLivePixelStats {
    uint64_t hash;
    uint32_t nonzero;
    uint32_t nonblack;
} NgLivePixelStats;

typedef enum NgLivePresentMode {
    NG_LIVE_PRESENT_FRAME,
    NG_LIVE_PRESENT_SLICE
} NgLivePresentMode;

static const char *present_mode_name(NgLivePresentMode mode) {
    return mode == NG_LIVE_PRESENT_SLICE ? "slice" : "frame";
}

static uint64_t live_fnv1a64_u32(uint64_t hash, uint32_t value) {
    for (uint32_t shift = 0; shift < 32u; shift += 8u) {
        hash ^= (uint8_t)(value >> shift);
        hash *= 1099511628211ull;
    }
    return hash;
}

static NgLivePixelStats analyze_live_pixels(const uint32_t *pixels,
                                            uint32_t width,
                                            uint32_t height) {
    NgLivePixelStats stats;
    memset(&stats, 0, sizeof(stats));
    stats.hash = 1469598103934665603ull;
    if (!pixels) {
        return stats;
    }
    uint32_t count = width * height;
    for (uint32_t i = 0; i < count; ++i) {
        uint32_t pixel = pixels[i];
        stats.hash = live_fnv1a64_u32(stats.hash, pixel);
        if (pixel != 0u) {
            ++stats.nonzero;
        }
        if (pixel != 0u && pixel != 0xFF000000u) {
            ++stats.nonblack;
        }
    }
    return stats;
}

static uint32_t current_live_palette_bank(void) {
    uint8_t palette_ram[NG_NEO_PALETTE_RAM_BYTES];
    uint16_t palette_words[NG_LIVE_PALETTE_WORDS];
    if (!ng_neogeo_copy_palette_ram(palette_ram, sizeof(palette_ram))) {
        return 0;
    }
    for (uint32_t i = 0; i < NG_LIVE_PALETTE_WORDS; ++i) {
        palette_words[i] = read_be_word(palette_ram, i);
    }
    return choose_palette_bank(palette_words);
}

static uint64_t live_mslug_sync_flags(void) {
    uint64_t packed = 0;
    for (uint32_t addr = 0x00106ED8u; addr <= 0x00106EDEu; ++addr) {
        packed = (packed << 8) | ng68k_read8(addr);
    }
    return packed;
}

static uint64_t live_mslug_sync_counters(void) {
    return ((uint64_t)ng68k_read16(0x00106EE0u) << 32) |
           ((uint64_t)ng68k_read16(0x00106EE2u) << 16) |
           (uint64_t)ng68k_read16(0x00106EE4u);
}

static uint16_t live_mslug_vblank_selector(void) {
    return (uint16_t)(((uint16_t)ng68k_read8(0x00106F26u) << 8) |
                      (uint16_t)ng68k_read8(0x00106F27u));
}

static uint16_t live_mslug_bios_flags(void) {
    return (uint16_t)(((uint16_t)ng68k_read8(0x0010FD80u) << 8) |
                      (uint16_t)ng68k_read8(0x0010FDAEu));
}

typedef struct NgLiveBackupTableStats {
    uint32_t nonzero_slots;
    uint32_t first_nonzero_slot;
    uint16_t first_nonzero_value;
} NgLiveBackupTableStats;

static NgLiveBackupTableStats live_backup_table_stats(void) {
    NgLiveBackupTableStats stats;
    memset(&stats, 0, sizeof(stats));
    stats.first_nonzero_slot = UINT32_MAX;
    for (uint32_t i = 0; i < 256u; ++i) {
        uint16_t value = ng68k_read16(0x00D00124u + i * 4u);
        if (value == 0u) {
            continue;
        }
        if (stats.first_nonzero_slot == UINT32_MAX) {
            stats.first_nonzero_slot = i;
            stats.first_nonzero_value = value;
        }
        ++stats.nonzero_slots;
    }
    return stats;
}

static void print_live_status(const char *label,
                              uint64_t refreshes,
                              uint64_t dispatches_per_refresh) {
    fprintf(stderr,
            "%s refresh=%llu dispatches=%llu frame=%u scanline=%u "
            "pc=$%06X sr=$%04X dpf=%llu irq_pending=$%04X polls=%u "
            "last_irq_pc=$%06X last_irq_level=%u last_irq_vector=%u "
            "budget_stop=$%06X\n",
            label,
            (unsigned long long)refreshes,
            (unsigned long long)ng_generated_smoke_dispatch_count(),
            ng_neogeo_frame_count(),
            ng_neogeo_current_scanline(),
            g_ng_m68k.pc & 0x00FFFFFFu,
            g_ng_m68k.sr & 0xFFFFu,
            (unsigned long long)dispatches_per_refresh,
            ng_neogeo_irq_pending(),
            ng_neogeo_interrupt_polls(),
            ng_neogeo_last_interrupt_return_pc() & 0x00FFFFFFu,
            ng_neogeo_last_interrupt_level(),
            ng_neogeo_last_interrupt_vector(),
            ng_generated_smoke_dispatch_budget_stop_addr() & 0x00FFFFFFu);
}

static void print_nonzero_watch_counts(const char *label, int instruction) {
    uint32_t count = ng_generated_smoke_dispatch_watch_count();
    int printed = 0;
    fprintf(stderr, "%s:", label);
    for (uint32_t i = 0; i < count; ++i) {
        uint64_t hits = instruction ?
            ng_generated_smoke_instruction_watch_hits(i) :
            ng_generated_smoke_dispatch_watch_hits(i);
        if (hits == 0u) {
            continue;
        }
        fprintf(stderr,
                " $%06X:%s=%llu",
                ng_generated_smoke_dispatch_watch_addr(i) & 0x00FFFFFFu,
                ng_generated_smoke_dispatch_watch_label(i),
                (unsigned long long)hits);
        printed = 1;
    }
    if (!printed) {
        fprintf(stderr, " none");
    }
    fprintf(stderr, "\n");
}

static void print_recent_dispatches(uint32_t count) {
    for (uint32_t i = 0; i < count; ++i) {
        if ((i % 16u) == 0u) {
            fprintf(stderr, "%srecent dispatches newest[%02u..%02u]:",
                    i == 0u ? "" : "\n",
                    i,
                    i + 15u);
        }
        fprintf(stderr,
                " $%06X",
                ng_generated_smoke_recent_dispatch(i) & 0x00FFFFFFu);
    }
    fprintf(stderr, "\n");
}

static void print_live_diagnostics(const char *label,
                                   uint64_t refreshes,
                                   uint64_t dispatches_per_refresh,
                                   const uint32_t *pixels,
                                   uint32_t width,
                                   uint32_t height) {
    NgLivePixelStats pixel_stats = analyze_live_pixels(pixels, width, height);
    NgLiveBackupTableStats backup_table = live_backup_table_stats();
    print_live_status(label, refreshes, dispatches_per_refresh);
    fprintf(stderr,
            "live dispatch diag: cart=%llu bios=%llu unique=%u hot_overflow=%u "
            "last=$%06X last_cart=$%06X last_bios=$%06X recent_loop=%u\n",
            (unsigned long long)ng_generated_smoke_cart_dispatch_count(),
            (unsigned long long)ng_generated_smoke_bios_dispatch_count(),
            ng_generated_smoke_unique_dispatch_count(),
            ng_generated_smoke_dispatch_hot_overflow(),
            ng_generated_smoke_last_dispatch_addr() & 0x00FFFFFFu,
            ng_generated_smoke_last_cart_dispatch_addr() & 0x00FFFFFFu,
            ng_generated_smoke_last_bios_dispatch_addr() & 0x00FFFFFFu,
            ng_generated_smoke_recent_loop_period());
    fprintf(stderr,
            "live runtime diag: watchdog=%u vblank=%u timer_irq=%u irqack=%u "
            "wd_timeout=%u wd_gap=%u wd_max_gap=%u wd_resets=%u "
            "wd_pending=%u wd_reset=$%06X@%u "
            "lspc=$%04X timer_reload=$%08X timer_counter=$%08X timer_stop=$%04X "
            "vram_addr=$%04X vram_mod=$%04X shadow=%u bios_vectors=%u fix=%u "
            "sound=$%02X port=$%02X\n",
            ng_neogeo_watchdog_kicks(),
            ng_neogeo_vblank_interrupts(),
            ng_neogeo_timer_interrupts(),
            ng_neogeo_irq_ack_writes(),
            ng_neogeo_watchdog_timeout_polls(),
            ng_neogeo_interrupt_polls() - ng_neogeo_watchdog_last_kick_poll(),
            ng_neogeo_watchdog_max_gap_polls(),
            ng_neogeo_watchdog_resets(),
            ng_neogeo_watchdog_reset_pending(),
            ng_neogeo_watchdog_last_reset_pc() & 0x00FFFFFFu,
            ng_neogeo_watchdog_last_reset_poll(),
            ng_neogeo_lspc_mode(),
            ng_neogeo_timer_reload(),
            ng_neogeo_timer_counter(),
            ng_neogeo_timer_stop(),
            ng_neogeo_vram_addr(),
            ng_neogeo_vram_mod(),
            ng_neogeo_shadow_enabled(),
            ng_neogeo_bios_vectors_enabled(),
            ng_neogeo_board_fix_enabled(),
            ng_neogeo_sound_command(),
            ng_neogeo_port_output());
    fprintf(stderr,
            "live write diag: watchdog=$%06X:$%02X@%06X "
            "latch_writes=%u latch=$%06X:$%02X@%06X "
            "biosvec_set=%u biosvec_clear=%u biosvec=$%06X:$%02X@%06X "
            "lspc=$%06X:$%04X@%06X irqack=$%04X@%06X "
            "port=$%06X:$%02X@%06X sound=$%06X:$%02X@%06X\n",
            ng_neogeo_last_watchdog_addr() & 0x00FFFFFFu,
            ng_neogeo_last_watchdog_value(),
            ng_neogeo_last_watchdog_pc() & 0x00FFFFFFu,
            ng_neogeo_system_latch_writes(),
            ng_neogeo_last_system_latch_addr() & 0x00FFFFFFu,
            ng_neogeo_last_system_latch_value(),
            ng_neogeo_last_system_latch_pc() & 0x00FFFFFFu,
            ng_neogeo_bios_vector_enable_writes(),
            ng_neogeo_bios_vector_disable_writes(),
            ng_neogeo_last_bios_vector_addr() & 0x00FFFFFFu,
            ng_neogeo_last_bios_vector_value(),
            ng_neogeo_last_bios_vector_pc() & 0x00FFFFFFu,
            ng_neogeo_last_lspc_write_addr() & 0x00FFFFFFu,
            ng_neogeo_last_lspc_write_value(),
            ng_neogeo_last_lspc_write_pc() & 0x00FFFFFFu,
            ng_neogeo_last_irq_ack_value(),
            ng_neogeo_last_irq_ack_pc() & 0x00FFFFFFu,
            ng_neogeo_last_port_output_addr() & 0x00FFFFFFu,
            ng_neogeo_port_output(),
            ng_neogeo_last_port_output_pc() & 0x00FFFFFFu,
            ng_neogeo_last_sound_addr() & 0x00FFFFFFu,
            ng_neogeo_sound_command(),
            ng_neogeo_last_sound_pc() & 0x00FFFFFFu);
    fprintf(stderr,
            "live memory/video diag: wram_nonzero=%u wram_sum=$%08X "
            "palette_nonzero=%u palette_sum=$%08X palette_writes=%u "
            "palette_nonzero_writes=%u palette_last=$%06X:$%04X:bank%u "
            "palette_peak=%u/$%08X palette_bank=%u vram_nonzero=%u "
            "vram_sum=$%08X pixels_nonzero=%u pixels_nonblack=%u "
            "pixels_hash=$%016llX\n",
            ng_neogeo_work_ram_nonzero_bytes(),
            ng_neogeo_work_ram_checksum(),
            ng_neogeo_palette_ram_nonzero_bytes(),
            ng_neogeo_palette_ram_checksum(),
            ng_neogeo_palette_write_count(),
            ng_neogeo_palette_nonzero_write_count(),
            ng_neogeo_palette_last_addr() & 0x00FFFFFFu,
            ng_neogeo_palette_last_value(),
            ng_neogeo_palette_last_bank(),
            ng_neogeo_palette_peak_nonzero_bytes(),
            ng_neogeo_palette_peak_checksum(),
            current_live_palette_bank(),
            ng_neogeo_vram_nonzero_words(),
            ng_neogeo_vram_checksum(),
            pixel_stats.nonzero,
            pixel_stats.nonblack,
            (unsigned long long)pixel_stats.hash);
    fprintf(stderr,
            "live backup diag: unlocked=%u writes=%u last=$%06X:$%02X "
            "nonzero=%u sum=$%08X d00047=$%02X d08108=$%02X d08109=$%02X "
            "table_nonzero=%u table_first=%s",
            ng_neogeo_backup_ram_unlocked(),
            ng_neogeo_backup_ram_write_count(),
            ng_neogeo_backup_ram_last_addr() & 0x00FFFFFFu,
            ng_neogeo_backup_ram_last_value(),
            ng_neogeo_backup_ram_nonzero_bytes(),
            ng_neogeo_backup_ram_checksum(),
            ng68k_read8(0x00D00047u),
            ng68k_read8(0x00D08108u),
            ng68k_read8(0x00D08109u),
            backup_table.nonzero_slots,
            backup_table.first_nonzero_slot == UINT32_MAX ? "none" : "$");
    if (backup_table.first_nonzero_slot != UINT32_MAX) {
        fprintf(stderr,
                "%02X:$%04X",
                backup_table.first_nonzero_slot & 0xFFu,
                backup_table.first_nonzero_value);
    }
    fprintf(stderr, "\n");
    fprintf(stderr,
            "live mslug diag: sync=$%014llX counters=$%012llX "
            "vblank_selector=$%04X bios_flags=$%04X\n",
            (unsigned long long)live_mslug_sync_flags(),
            (unsigned long long)live_mslug_sync_counters(),
            live_mslug_vblank_selector(),
            live_mslug_bios_flags());
    fprintf(stderr,
            "live regs d: d0=$%08X d1=$%08X d2=$%08X d3=$%08X "
            "d4=$%08X d5=$%08X d6=$%08X d7=$%08X\n",
            g_ng_m68k.d[0], g_ng_m68k.d[1], g_ng_m68k.d[2], g_ng_m68k.d[3],
            g_ng_m68k.d[4], g_ng_m68k.d[5], g_ng_m68k.d[6], g_ng_m68k.d[7]);
    fprintf(stderr,
            "live regs a: a0=$%08X a1=$%08X a2=$%08X a3=$%08X "
            "a4=$%08X a5=$%08X a6=$%08X a7=$%08X usp=$%08X ssp=$%08X\n",
            g_ng_m68k.a[0], g_ng_m68k.a[1], g_ng_m68k.a[2], g_ng_m68k.a[3],
            g_ng_m68k.a[4], g_ng_m68k.a[5], g_ng_m68k.a[6], g_ng_m68k.a[7],
            g_ng_m68k.usp, g_ng_m68k.ssp);
    print_recent_dispatches(32u);
    print_nonzero_watch_counts("live dispatch watch", 0);
    print_nonzero_watch_counts("live instruction watch", 1);
}

static int parse_u64(const char *text, uint64_t *out) {
    char *end = NULL;
    unsigned long long value = strtoull(text, &end, 0);
    if (!text || !*text || !end || *end != '\0') {
        return 0;
    }
    *out = (uint64_t)value;
    return 1;
}

static int parse_present_mode(const char *text, NgLivePresentMode *out) {
    if (strcmp(text, "frame") == 0 || strcmp(text, "sync") == 0) {
        *out = NG_LIVE_PRESENT_FRAME;
        return 1;
    }
    if (strcmp(text, "slice") == 0 || strcmp(text, "fixed") == 0) {
        *out = NG_LIVE_PRESENT_SLICE;
        return 1;
    }
    return 0;
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s [options] <game.neo> <bios.rom> [lo-rom]\n"
            "\n"
            "Options:\n"
            "  --fast-forward <n>             dispatches before opening the loop (default: 0)\n"
            "  --dispatches-per-refresh <n>   dispatch cap per rendered refresh (default: %u)\n"
            "  --present-mode <frame|slice>   frame-boundary or fixed-slice presentation (default: frame)\n"
            "  --scanline-poll-interval <n>   runtime scanline poll interval (default: 64)\n"
            "  --watchdog-timeout-polls <n>   reset after n interrupt polls without watchdog kick (default: %u, 0 disables)\n"
            "  --start-bios                   enter through the BIOS reset vector instead of cart header\n"
            "  --max-refreshes <n>            exit after n presented frames\n"
            "  --status-interval <n>          log status every n presented frames\n"
            "  --diagnostics-interval <n>     log detailed diagnostics every n frames\n"
            "  --stall-refreshes <n>          log if scanline/frame stalls for n refreshes (default: 180, 0 disables)\n"
            "  --no-throttle                  do not sleep to ~60Hz after each refresh\n"
            "  --scale <n>                    integer window scale (default: %u)\n",
            argv0,
            NG_LIVE_DEFAULT_DISPATCHES_PER_REFRESH,
            NG_LIVE_DEFAULT_WATCHDOG_TIMEOUT_POLLS,
            NG_LIVE_DEFAULT_SCALE);
}

int main(int argc, char **argv) {
    uint64_t fast_forward = NG_LIVE_DEFAULT_FAST_FORWARD;
    uint64_t dispatches_per_refresh = NG_LIVE_DEFAULT_DISPATCHES_PER_REFRESH;
    uint64_t max_refreshes = 0;
    uint64_t status_interval = 0;
    uint64_t diagnostics_interval = 0;
    uint64_t stall_refreshes = 180u;
    uint64_t no_throttle = 0;
    uint64_t scanline_poll_interval = 64u;
    uint64_t watchdog_timeout_polls = NG_LIVE_DEFAULT_WATCHDOG_TIMEOUT_POLLS;
    uint64_t scale = NG_LIVE_DEFAULT_SCALE;
    int start_bios = 0;
    NgLivePresentMode present_mode = NG_LIVE_PRESENT_FRAME;

    int argi = 1;
    while (argi < argc && strncmp(argv[argi], "--", 2) == 0) {
        const char *option = argv[argi++];
        uint64_t *target = NULL;
        if (strcmp(option, "--fast-forward") == 0) {
            target = &fast_forward;
        } else if (strcmp(option, "--dispatches-per-refresh") == 0) {
            target = &dispatches_per_refresh;
        } else if (strcmp(option, "--present-mode") == 0) {
            if (argi >= argc ||
                !parse_present_mode(argv[argi++], &present_mode)) {
                fprintf(stderr,
                        "%s requires one of: frame, slice\n",
                        option);
                usage(argv[0]);
                return 2;
            }
            continue;
        } else if (strcmp(option, "--present-frame") == 0) {
            present_mode = NG_LIVE_PRESENT_FRAME;
            continue;
        } else if (strcmp(option, "--present-slice") == 0) {
            present_mode = NG_LIVE_PRESENT_SLICE;
            continue;
        } else if (strcmp(option, "--scanline-poll-interval") == 0) {
            target = &scanline_poll_interval;
        } else if (strcmp(option, "--watchdog-timeout-polls") == 0) {
            target = &watchdog_timeout_polls;
        } else if (strcmp(option, "--start-bios") == 0) {
            start_bios = 1;
            continue;
        } else if (strcmp(option, "--max-refreshes") == 0) {
            target = &max_refreshes;
        } else if (strcmp(option, "--status-interval") == 0) {
            target = &status_interval;
        } else if (strcmp(option, "--diagnostics-interval") == 0) {
            target = &diagnostics_interval;
        } else if (strcmp(option, "--stall-refreshes") == 0) {
            target = &stall_refreshes;
        } else if (strcmp(option, "--scale") == 0) {
            target = &scale;
        } else if (strcmp(option, "--no-throttle") == 0) {
            no_throttle = 1u;
            continue;
        } else if (strcmp(option, "--help") == 0 || strcmp(option, "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "unknown option: %s\n", option);
            usage(argv[0]);
            return 2;
        }
        if (argi >= argc || !parse_u64(argv[argi++], target)) {
            fprintf(stderr, "%s requires an integer value\n", option);
            usage(argv[0]);
            return 2;
        }
    }

    if (argc - argi != 2 && argc - argi != 3) {
        usage(argv[0]);
        return 2;
    }
    if (dispatches_per_refresh == 0u || scanline_poll_interval > UINT32_MAX ||
        watchdog_timeout_polls > UINT32_MAX ||
        scale == 0u || scale > 16u) {
        usage(argv[0]);
        return 2;
    }

    const char *neo_path = argv[argi];
    const char *bios_path = argv[argi + 1];
    const char *lo_rom_path = argc - argi == 3 ? argv[argi + 2] : NULL;

    NgNeoRomImage image;
    memset(&image, 0, sizeof(image));
    uint8_t *bios_data = NULL;
    uint32_t bios_size = 0;
    uint8_t *zoom_rom = NULL;
    uint32_t zoom_rom_size = 0;
    uint32_t *pixels = NULL;
    SDL_Window *window = NULL;
    SDL_Renderer *renderer = NULL;
    SDL_Texture *texture = NULL;

    if (!ng_neo_rom_image_load(&image, neo_path) ||
        !read_file(bios_path, &bios_data, &bios_size)) {
        ng_neo_rom_image_free(&image);
        free(bios_data);
        return 1;
    }
    byteswap_words(bios_data, bios_size);
    if (lo_rom_path) {
        if (!load_zoom_rom_file(lo_rom_path, &zoom_rom, &zoom_rom_size, 1)) {
            ng_neo_rom_image_free(&image);
            free(bios_data);
            return 1;
        }
    } else {
        (void)try_load_default_zoom_rom(neo_path, &zoom_rom, &zoom_rom_size);
    }

    NgProgramRom rom;
    memset(&rom, 0, sizeof(rom));
    rom.data = image.p.data;
    rom.size = image.p.size;
    uint32_t cart_entry = 0;
    if (!ng_program_rom_cart_entry(&rom, &cart_entry)) {
        fprintf(stderr, "failed to locate cartridge entry in %s\n", neo_path);
        ng_neo_rom_image_free(&image);
        free(bios_data);
        free(zoom_rom);
        return 1;
    }
    uint32_t initial_entry = start_bios ?
        (ng_program_rom_initial_pc(&rom) & 0x00FFFFFFu) :
        cart_entry;

    ng_neogeo_reset_runtime();
    ng_neogeo_set_program_rom(image.p.data, image.p.size);
    ng_neogeo_set_system_rom(bios_data, bios_size);
    ng_neogeo_set_external_dispatch(live_external_dispatch);
    ng_neogeo_set_auto_scanline_interval((uint32_t)scanline_poll_interval);
    ng_neogeo_set_watchdog_reset_vector(initial_entry, ng_program_rom_initial_ssp(&rom));
    ng_neogeo_set_watchdog_timeout_polls((uint32_t)watchdog_timeout_polls);
    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    g_ng_m68k.ssp = ng_program_rom_initial_ssp(&rom);
    g_ng_m68k.a[7] = g_ng_m68k.ssp;
    g_ng_m68k.sr = 0x2700u;
    ng_generated_smoke_reset_dispatch_stats();
    ng_generated_smoke_set_scanline_poll_interval((uint32_t)scanline_poll_interval);

    if (!run_fast_forward(fast_forward, initial_entry)) {
        ng_neo_rom_image_free(&image);
        free(bios_data);
        free(zoom_rom);
        return 1;
    }

    const uint32_t width = NG_NEO_SPRITE_FRAME_WIDTH;
    const uint32_t height = NG_NEO_SPRITE_FRAME_HEIGHT;
    pixels = (uint32_t *)calloc((size_t)width * (size_t)height, sizeof(uint32_t));
    if (!pixels) {
        fprintf(stderr, "failed to allocate framebuffer\n");
        ng_neo_rom_image_free(&image);
        free(bios_data);
        free(zoom_rom);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        free(pixels);
        ng_neo_rom_image_free(&image);
        free(bios_data);
        free(zoom_rom);
        return 1;
    }
    window = SDL_CreateWindow("neo-recomp live host",
                              SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED,
                              (int)(width * scale),
                              (int)(height * scale),
                              SDL_WINDOW_SHOWN | SDL_WINDOW_RESIZABLE);
    renderer = window ? SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED) : NULL;
    if (!renderer && window) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    texture = renderer ? SDL_CreateTexture(renderer,
                                           SDL_PIXELFORMAT_ARGB8888,
                                           SDL_TEXTUREACCESS_STREAMING,
                                           (int)width,
                                           (int)height) : NULL;
    if (!window || !renderer || !texture) {
        fprintf(stderr, "SDL window/renderer/texture setup failed: %s\n", SDL_GetError());
        SDL_DestroyTexture(texture);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        free(pixels);
        ng_neo_rom_image_free(&image);
        free(bios_data);
        free(zoom_rom);
        return 1;
    }
    SDL_RenderSetLogicalSize(renderer, (int)width, (int)height);

    fprintf(stderr,
            "live host started: entry=$%06X cart_entry=$%06X dispatches=%llu "
            "frame=%u scanline=%u present=%s watchdog_timeout=%llu\n",
            initial_entry & 0x00FFFFFFu,
            cart_entry & 0x00FFFFFFu,
            (unsigned long long)ng_generated_smoke_dispatch_count(),
            ng_neogeo_frame_count(),
            ng_neogeo_current_scanline(),
            present_mode_name(present_mode),
            (unsigned long long)watchdog_timeout_polls);
    fprintf(stderr,
            "keys: q/Escape quit, Space pause/resume, +/- adjust dispatch cap per refresh\n");

    int quit = 0;
    int paused = 0;
    int stall_reported = 0;
    uint64_t stagnant_refreshes = 0;
    uint32_t last_progress_frame = ng_neogeo_frame_count();
    uint16_t last_progress_scanline = ng_neogeo_current_scanline();
    uint64_t refreshes = 0;
    while (!quit) {
        uint32_t tick_start = SDL_GetTicks();
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                quit = 1;
            } else if (event.type == SDL_KEYDOWN) {
                SDL_Keycode key = event.key.keysym.sym;
                if (key == SDLK_ESCAPE || key == SDLK_q) {
                    quit = 1;
                } else if (key == SDLK_SPACE) {
                    paused = !paused;
                } else if (key == SDLK_EQUALS || key == SDLK_PLUS) {
                    dispatches_per_refresh += dispatches_per_refresh / 4u + 1u;
                } else if (key == SDLK_MINUS && dispatches_per_refresh > 1u) {
                    dispatches_per_refresh -= dispatches_per_refresh / 4u + 1u;
                    if (dispatches_per_refresh == 0u) {
                        dispatches_per_refresh = 1u;
                    }
                }
            }
        }

        if (!paused) {
            int ok = present_mode == NG_LIVE_PRESENT_FRAME ?
                run_frame_synced_slice(dispatches_per_refresh, initial_entry) :
                run_dispatch_slice(dispatches_per_refresh, initial_entry);
            if (!ok) {
                quit = 1;
            }
        }
        if (!render_live_frame(&image, zoom_rom, zoom_rom_size, pixels, width, height)) {
            quit = 1;
        }

        SDL_UpdateTexture(texture, NULL, pixels, (int)(width * sizeof(uint32_t)));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);

        if (status_interval != 0u && refreshes != 0u &&
            (refreshes % status_interval) == 0u) {
            print_live_status("live status", refreshes, dispatches_per_refresh);
        }
        if (diagnostics_interval != 0u && refreshes != 0u &&
            (refreshes % diagnostics_interval) == 0u) {
            print_live_diagnostics("live diagnostics",
                                   refreshes,
                                   dispatches_per_refresh,
                                   pixels,
                                   width,
                                   height);
        }

        uint32_t current_frame = ng_neogeo_frame_count();
        uint16_t current_scanline = ng_neogeo_current_scanline();
        if (current_frame != last_progress_frame ||
            current_scanline != last_progress_scanline) {
            last_progress_frame = current_frame;
            last_progress_scanline = current_scanline;
            stagnant_refreshes = 0;
            stall_reported = 0;
        } else if (!paused && stall_refreshes != 0u) {
            ++stagnant_refreshes;
            if (!stall_reported && stagnant_refreshes >= stall_refreshes) {
                print_live_diagnostics("live scanline stall",
                                       refreshes,
                                       dispatches_per_refresh,
                                       pixels,
                                       width,
                                       height);
                stall_reported = 1;
            }
        }

        if ((refreshes % 30u) == 0u) {
            char title[256];
            snprintf(title,
                     sizeof(title),
                     "neo-recomp live - dispatches=%llu frame=%u scanline=%u dpf=%llu %s%s",
                     (unsigned long long)ng_generated_smoke_dispatch_count(),
                     ng_neogeo_frame_count(),
                     ng_neogeo_current_scanline(),
                     (unsigned long long)dispatches_per_refresh,
                     present_mode_name(present_mode),
                     paused ? " paused" : "");
            SDL_SetWindowTitle(window, title);
        }

        ++refreshes;
        if (max_refreshes != 0u && refreshes >= max_refreshes) {
            quit = 1;
        }
        uint32_t elapsed = SDL_GetTicks() - tick_start;
        if (!no_throttle && elapsed < 16u) {
            SDL_Delay(16u - elapsed);
        }
    }

    fprintf(stderr,
            "live host stopped: dispatches=%llu frame=%u scanline=%u budget_stop=$%06X\n",
            (unsigned long long)ng_generated_smoke_dispatch_count(),
            ng_neogeo_frame_count(),
            ng_neogeo_current_scanline(),
            ng_generated_smoke_dispatch_budget_stop_addr() & 0x00FFFFFFu);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    free(pixels);
    free(zoom_rom);
    free(bios_data);
    ng_neo_rom_image_free(&image);
    return 0;
}
