#include "ngrecomp/neogeo_runtime.h"
#include "p_rom.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NG_GENERATED_SMOKE_RECENT_DISPATCHES 256u
#define NG_GENERATED_SMOKE_LOOP_MAX_PERIOD 64u
#define NG_GENERATED_SMOKE_HOT_SLOTS 32768u
#define NG_GENERATED_SMOKE_HOT_TOP 5u

#ifdef NG_GENERATED_SMOKE_HAS_BIOS
void ng_bios_generated_call(uint32_t addr);

static int g_ng_generated_smoke_bios_dispatch_depth;

static void ng_generated_smoke_call_bios(uint32_t addr) {
    ++g_ng_generated_smoke_bios_dispatch_depth;
    ng_bios_generated_call(addr);
    --g_ng_generated_smoke_bios_dispatch_depth;
}
#endif

#ifdef NG_GENERATED_SMOKE_COMBINED_DISPATCH
void ng_cart_generated_call(uint32_t addr);

static int g_ng_generated_smoke_dispatch_active;
static int g_ng_generated_smoke_dispatch_pending;
static uint32_t g_ng_generated_smoke_dispatch_addr;
static uint64_t g_ng_generated_smoke_dispatch_budget;
static uint64_t g_ng_generated_smoke_dispatch_count;
static uint64_t g_ng_generated_smoke_cart_dispatch_count;
static uint64_t g_ng_generated_smoke_bios_dispatch_count;
static uint32_t g_ng_generated_smoke_last_dispatch_addr;
static uint32_t g_ng_generated_smoke_recent_dispatches[NG_GENERATED_SMOKE_RECENT_DISPATCHES];
static uint32_t g_ng_generated_smoke_hot_addr[NG_GENERATED_SMOKE_HOT_SLOTS];
static uint64_t g_ng_generated_smoke_hot_count[NG_GENERATED_SMOKE_HOT_SLOTS];
static uint32_t g_ng_generated_smoke_unique_dispatch_count;
static int g_ng_generated_smoke_hot_overflow;
static uint32_t g_ng_generated_smoke_budget_stop_addr;
static int g_ng_generated_smoke_budget_hit;

void ng_generated_smoke_reset_dispatch_stats(void) {
    g_ng_generated_smoke_dispatch_count = 0;
    g_ng_generated_smoke_cart_dispatch_count = 0;
    g_ng_generated_smoke_bios_dispatch_count = 0;
    g_ng_generated_smoke_last_dispatch_addr = 0;
    memset(g_ng_generated_smoke_recent_dispatches,
           0,
           sizeof(g_ng_generated_smoke_recent_dispatches));
    memset(g_ng_generated_smoke_hot_addr,
           0,
           sizeof(g_ng_generated_smoke_hot_addr));
    memset(g_ng_generated_smoke_hot_count,
           0,
           sizeof(g_ng_generated_smoke_hot_count));
    g_ng_generated_smoke_unique_dispatch_count = 0;
    g_ng_generated_smoke_hot_overflow = 0;
    g_ng_generated_smoke_budget_stop_addr = 0;
    g_ng_generated_smoke_budget_hit = 0;
}

void ng_generated_smoke_set_dispatch_budget(uint64_t max_dispatches) {
    g_ng_generated_smoke_dispatch_budget = max_dispatches;
    g_ng_generated_smoke_budget_hit = 0;
}

uint64_t ng_generated_smoke_dispatch_count(void) {
    return g_ng_generated_smoke_dispatch_count;
}

uint64_t ng_generated_smoke_cart_dispatch_count(void) {
    return g_ng_generated_smoke_cart_dispatch_count;
}

uint64_t ng_generated_smoke_bios_dispatch_count(void) {
    return g_ng_generated_smoke_bios_dispatch_count;
}

uint32_t ng_generated_smoke_unique_dispatch_count(void) {
    return g_ng_generated_smoke_unique_dispatch_count;
}

int ng_generated_smoke_dispatch_hot_overflow(void) {
    return g_ng_generated_smoke_hot_overflow;
}

int ng_generated_smoke_dispatch_budget_hit(void) {
    return g_ng_generated_smoke_budget_hit;
}

uint32_t ng_generated_smoke_dispatch_budget_stop_addr(void) {
    return g_ng_generated_smoke_budget_stop_addr;
}

uint32_t ng_generated_smoke_last_dispatch_addr(void) {
    return g_ng_generated_smoke_last_dispatch_addr;
}

static uint32_t ng_generated_smoke_recent_from_end(uint32_t offset) {
    if (offset >= NG_GENERATED_SMOKE_RECENT_DISPATCHES ||
        (uint64_t)offset >= g_ng_generated_smoke_dispatch_count) {
        return 0;
    }
    uint64_t idx = (g_ng_generated_smoke_dispatch_count - 1u - offset) %
                   NG_GENERATED_SMOKE_RECENT_DISPATCHES;
    return g_ng_generated_smoke_recent_dispatches[idx];
}

uint32_t ng_generated_smoke_recent_loop_period(void) {
    uint64_t have = g_ng_generated_smoke_dispatch_count;
    if (have > NG_GENERATED_SMOKE_RECENT_DISPATCHES) {
        have = NG_GENERATED_SMOKE_RECENT_DISPATCHES;
    }
    for (uint32_t period = 1; period <= NG_GENERATED_SMOKE_LOOP_MAX_PERIOD; ++period) {
        if (have < (uint64_t)period * 2u) {
            break;
        }
        int matches = 1;
        for (uint32_t i = 0; i < period; ++i) {
            if (ng_generated_smoke_recent_from_end(i) !=
                ng_generated_smoke_recent_from_end(i + period)) {
                matches = 0;
                break;
            }
        }
        if (matches) {
            return period;
        }
    }
    return 0;
}

static uint32_t ng_generated_smoke_hot_slot(uint32_t addr) {
    addr &= 0x00FFFFFFu;
    addr ^= addr >> 12;
    addr *= 2654435761u;
    return addr & (NG_GENERATED_SMOKE_HOT_SLOTS - 1u);
}

static void ng_generated_smoke_note_dispatch(uint32_t addr) {
    addr &= 0x00FFFFFFu;
    uint32_t slot = ng_generated_smoke_hot_slot(addr);
    for (uint32_t probe = 0; probe < NG_GENERATED_SMOKE_HOT_SLOTS; ++probe) {
        uint32_t idx = (slot + probe) & (NG_GENERATED_SMOKE_HOT_SLOTS - 1u);
        if (g_ng_generated_smoke_hot_count[idx] == 0u) {
            g_ng_generated_smoke_hot_addr[idx] = addr;
            g_ng_generated_smoke_hot_count[idx] = 1u;
            ++g_ng_generated_smoke_unique_dispatch_count;
            return;
        }
        if (g_ng_generated_smoke_hot_addr[idx] == addr) {
            ++g_ng_generated_smoke_hot_count[idx];
            return;
        }
    }
    g_ng_generated_smoke_hot_overflow = 1;
}

static void ng_generated_smoke_dispatch_one(uint32_t addr) {
    addr &= 0x00FFFFFFu;
    if (addr >= 0x00C00000u && addr <= 0x00CFFFFFu) {
        uint32_t bios_addr =
            0x00C00000u + ((addr - 0x00C00000u) % NG_NEO_SYSTEM_ROM_BYTES);
        ++g_ng_generated_smoke_bios_dispatch_count;
        ng_generated_smoke_call_bios(bios_addr);
        return;
    }
    ++g_ng_generated_smoke_cart_dispatch_count;
    ng_cart_generated_call(addr);
}

void ng_generated_call(uint32_t addr) {
    addr &= 0x00FFFFFFu;
    if (g_ng_generated_smoke_dispatch_active) {
        g_ng_generated_smoke_dispatch_addr = addr;
        g_ng_generated_smoke_dispatch_pending = 1;
        return;
    }
    g_ng_generated_smoke_dispatch_active = 1;
    g_ng_generated_smoke_dispatch_addr = addr;
    g_ng_generated_smoke_dispatch_pending = 1;
    while (g_ng_generated_smoke_dispatch_pending) {
        uint32_t next_addr = g_ng_generated_smoke_dispatch_addr;
        g_ng_generated_smoke_dispatch_pending = 0;
        if (g_ng_generated_smoke_dispatch_budget != 0 &&
            g_ng_generated_smoke_dispatch_count >=
                g_ng_generated_smoke_dispatch_budget) {
            g_ng_generated_smoke_budget_hit = 1;
            g_ng_generated_smoke_budget_stop_addr = next_addr & 0x00FFFFFFu;
            g_ng_m68k.pc = g_ng_generated_smoke_budget_stop_addr;
            break;
        }
        ++g_ng_generated_smoke_dispatch_count;
        g_ng_generated_smoke_last_dispatch_addr = next_addr & 0x00FFFFFFu;
        g_ng_generated_smoke_recent_dispatches[
            (g_ng_generated_smoke_dispatch_count - 1u) %
            NG_GENERATED_SMOKE_RECENT_DISPATCHES] =
            g_ng_generated_smoke_last_dispatch_addr;
        ng_generated_smoke_note_dispatch(g_ng_generated_smoke_last_dispatch_addr);
        ng_generated_smoke_dispatch_one(next_addr);
    }
    g_ng_generated_smoke_dispatch_active = 0;
}
#else
void ng_generated_call(uint32_t addr);

void ng_generated_smoke_reset_dispatch_stats(void) {
}

void ng_generated_smoke_set_dispatch_budget(uint64_t max_dispatches) {
    (void)max_dispatches;
}

uint64_t ng_generated_smoke_dispatch_count(void) {
    return 0;
}

uint64_t ng_generated_smoke_cart_dispatch_count(void) {
    return 0;
}

uint64_t ng_generated_smoke_bios_dispatch_count(void) {
    return 0;
}

uint32_t ng_generated_smoke_unique_dispatch_count(void) {
    return 0;
}

int ng_generated_smoke_dispatch_hot_overflow(void) {
    return 0;
}

int ng_generated_smoke_dispatch_budget_hit(void) {
    return 0;
}

uint32_t ng_generated_smoke_dispatch_budget_stop_addr(void) {
    return 0;
}

uint32_t ng_generated_smoke_last_dispatch_addr(void) {
    return 0;
}

uint32_t ng_generated_smoke_recent_loop_period(void) {
    return 0;
}
#endif

#ifdef NG_GENERATED_SMOKE_COMBINED_DISPATCH
static void ng_generated_smoke_hot_rank(uint32_t rank,
                                        uint32_t *out_addr,
                                        uint64_t *out_count) {
    uint32_t top_addr[NG_GENERATED_SMOKE_HOT_TOP] = {0};
    uint64_t top_count[NG_GENERATED_SMOKE_HOT_TOP] = {0};

    for (uint32_t i = 0; i < NG_GENERATED_SMOKE_HOT_SLOTS; ++i) {
        uint64_t count = g_ng_generated_smoke_hot_count[i];
        if (count == 0u) {
            continue;
        }
        for (uint32_t j = 0; j < NG_GENERATED_SMOKE_HOT_TOP; ++j) {
            if (count > top_count[j]) {
                for (uint32_t k = NG_GENERATED_SMOKE_HOT_TOP - 1u; k > j; --k) {
                    top_count[k] = top_count[k - 1u];
                    top_addr[k] = top_addr[k - 1u];
                }
                top_count[j] = count;
                top_addr[j] = g_ng_generated_smoke_hot_addr[i];
                break;
            }
        }
    }

    if (out_addr) {
        *out_addr = rank < NG_GENERATED_SMOKE_HOT_TOP ? top_addr[rank] : 0u;
    }
    if (out_count) {
        *out_count = rank < NG_GENERATED_SMOKE_HOT_TOP ? top_count[rank] : 0u;
    }
}
#else
static void ng_generated_smoke_hot_rank(uint32_t rank,
                                        uint32_t *out_addr,
                                        uint64_t *out_count) {
    (void)rank;
    if (out_addr) {
        *out_addr = 0u;
    }
    if (out_count) {
        *out_count = 0u;
    }
}
#endif

#ifdef NG_GENERATED_SMOKE_HAS_BIOS
static int ng_generated_smoke_bios_dispatch(uint32_t addr) {
    addr &= 0x00FFFFFFu;
    if (addr >= 0x00C00000u && addr <= 0x00CFFFFFu) {
        if (g_ng_generated_smoke_bios_dispatch_depth) {
            return 0;
        }
        ng_generated_smoke_call_bios(addr);
        return 1;
    }
    return 0;
}
#endif

static void ng_generated_smoke_byteswap_words(uint8_t *data, uint32_t size) {
    for (uint32_t i = 0; i + 1u < size; i += 2u) {
        uint8_t tmp = data[i];
        data[i] = data[i + 1u];
        data[i + 1u] = tmp;
    }
}

static int ng_generated_smoke_read_file(const char *path,
                                        uint8_t **out_data,
                                        uint32_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "failed to open file: %s\n", path);
        return 0;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return 0;
    }
    if ((unsigned long)size > 0xFFFFFFFFul) {
        fprintf(stderr, "file too large: %s\n", path);
        fclose(f);
        return 0;
    }
    rewind(f);

    uint8_t *data = (uint8_t *)malloc((size_t)size ? (size_t)size : 1u);
    if (!data) {
        fclose(f);
        return 0;
    }
    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return 0;
    }
    fclose(f);

    *out_data = data;
    *out_size = (uint32_t)size;
    return 1;
}

#ifndef NG_GENERATED_SMOKE_NO_MAIN
static int ng_generated_smoke_parse_u64(const char *text, uint64_t *out) {
    uint64_t value = 0;
    if (!text || !*text || !out) {
        return 0;
    }
    for (const char *p = text; *p; ++p) {
        if (*p < '0' || *p > '9') {
            return 0;
        }
        uint64_t digit = (uint64_t)(*p - '0');
        if (value > (UINT64_MAX - digit) / 10u) {
            return 0;
        }
        value = value * 10u + digit;
    }
    *out = value;
    return 1;
}
#endif

static void ng_generated_smoke_print_summary(void) {
    uint32_t recent_loop_period = ng_generated_smoke_recent_loop_period();
    fprintf(stderr,
            "smoke summary: dispatches=%llu cart=%llu bios=%llu "
            "unique=%u hot_overflow=%u last=$%06X pc=$%06X sr=$%04X "
            "sp=$%08X polls=%u watchdog=%u vblank=%u frame=%u timer_irq=%u "
            "irqack=%u irq_pending=$%04X scanline=%u sound=$%02X "
            "port=$%02X wram_nonzero=%u "
            "wram_sum=$%08X vram_nonzero=%u vram_sum=$%08X recent_loop=%u\n",
            (unsigned long long)ng_generated_smoke_dispatch_count(),
            (unsigned long long)ng_generated_smoke_cart_dispatch_count(),
            (unsigned long long)ng_generated_smoke_bios_dispatch_count(),
            ng_generated_smoke_unique_dispatch_count(),
            ng_generated_smoke_dispatch_hot_overflow(),
            ng_generated_smoke_last_dispatch_addr() & 0x00FFFFFFu,
            g_ng_m68k.pc & 0x00FFFFFFu,
            g_ng_m68k.sr,
            g_ng_m68k.a[7],
            ng_neogeo_interrupt_polls(),
            ng_neogeo_watchdog_kicks(),
            ng_neogeo_vblank_interrupts(),
            ng_neogeo_frame_count(),
            ng_neogeo_timer_interrupts(),
            ng_neogeo_irq_ack_writes(),
            ng_neogeo_irq_pending(),
            ng_neogeo_current_scanline(),
            ng_neogeo_sound_command(),
            ng_neogeo_port_output(),
            ng_neogeo_work_ram_nonzero_bytes(),
            ng_neogeo_work_ram_checksum(),
            ng_neogeo_vram_nonzero_words(),
            ng_neogeo_vram_checksum(),
            recent_loop_period);
    fprintf(stderr,
            "dispatch hot: unique=%u overflow=%u",
            ng_generated_smoke_unique_dispatch_count(),
            ng_generated_smoke_dispatch_hot_overflow());
    for (uint32_t i = 0; i < NG_GENERATED_SMOKE_HOT_TOP; ++i) {
        uint32_t addr = 0;
        uint64_t count = 0;
        ng_generated_smoke_hot_rank(i, &addr, &count);
        if (count == 0u) {
            break;
        }
        fprintf(stderr,
                " top%u=$%06X:%llu",
                i,
                addr & 0x00FFFFFFu,
                (unsigned long long)count);
    }
    fprintf(stderr, "\n");
    if (ng_generated_smoke_dispatch_budget_hit()) {
        fprintf(stderr,
                "smoke budget reached at $%06X after %llu dispatches\n",
                ng_generated_smoke_dispatch_budget_stop_addr() & 0x00FFFFFFu,
                (unsigned long long)ng_generated_smoke_dispatch_count());
    }
}

int ng_generated_smoke_run_with_bios(const char *neo_path,
                                     const char *bios_path) {
    if (!neo_path || !*neo_path) {
        fprintf(stderr, "missing .neo path\n");
        return 2;
    }
    if (bios_path && !*bios_path) {
        fprintf(stderr, "missing BIOS path\n");
        return 2;
    }
    ng_neogeo_set_external_dispatch(NULL);
    ng_neogeo_set_system_rom(NULL, 0);

    NgProgramRom rom;
    if (!ng_program_rom_load_neo(&rom, neo_path)) {
        fprintf(stderr, "failed to load neo image: %s\n", neo_path);
        return 1;
    }

    uint32_t cart_entry = 0;
    if (!ng_program_rom_cart_entry(&rom, &cart_entry)) {
        fprintf(stderr, "failed to locate cartridge entry in %s\n", neo_path);
        ng_program_rom_free(&rom);
        return 1;
    }

    uint8_t *bios_data = NULL;
    uint32_t bios_size = 0;
    if (bios_path) {
        if (!ng_generated_smoke_read_file(bios_path, &bios_data, &bios_size)) {
            fprintf(stderr, "failed to load BIOS image: %s\n", bios_path);
            ng_program_rom_free(&rom);
            return 1;
        }
        ng_generated_smoke_byteswap_words(bios_data, bios_size);
    }

    ng_neogeo_reset_runtime();
    ng_neogeo_set_auto_vblank_interval(0);
    ng_neogeo_set_auto_scanline_interval(bios_path ? 1u : 0u);
    ng_neogeo_set_program_rom(rom.data, rom.size);
    ng_neogeo_set_system_rom(bios_data, bios_size);
#ifdef NG_GENERATED_SMOKE_HAS_BIOS
    ng_neogeo_set_external_dispatch(bios_path ?
        ng_generated_smoke_bios_dispatch : NULL);
#else
    ng_neogeo_set_external_dispatch(NULL);
    if (bios_path) {
        fprintf(stderr,
                "BIOS image loaded for bus reads; rebuild harness with "
                "NG_GENERATED_SMOKE_HAS_BIOS to execute BIOS code\n");
    }
#endif
    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    g_ng_m68k.ssp = ng_program_rom_initial_ssp(&rom);
    g_ng_m68k.usp = 0;
    g_ng_m68k.a[7] = g_ng_m68k.ssp;
    g_ng_m68k.sr = 0x2700u;

    fprintf(stderr,
            "starting cart entry $%06X ssp=$%08X\n",
            cart_entry & 0x00FFFFFFu,
            g_ng_m68k.ssp);
    ng_generated_smoke_reset_dispatch_stats();
    ng_generated_call(cart_entry);
    fprintf(stderr,
            "returned pc=$%06X sr=$%04X sp=$%08X\n",
            g_ng_m68k.pc & 0x00FFFFFFu,
            g_ng_m68k.sr,
            g_ng_m68k.a[7]);
    ng_generated_smoke_print_summary();

    ng_neogeo_set_external_dispatch(NULL);
    ng_neogeo_set_auto_scanline_interval(0);
    ng_neogeo_set_auto_vblank_interval(0);
    ng_neogeo_set_system_rom(NULL, 0);
    ng_neogeo_set_program_rom(NULL, 0);
    free(bios_data);
    ng_program_rom_free(&rom);
    return 0;
}

int ng_generated_smoke_run(const char *neo_path) {
    return ng_generated_smoke_run_with_bios(neo_path, NULL);
}

#ifndef NG_GENERATED_SMOKE_NO_MAIN
static void usage(const char *argv0) {
    fprintf(stderr, "usage: %s [--bios <bios.rom>] [--max-dispatches <n>] <game.neo>\n", argv0);
}

int main(int argc, char **argv) {
    const char *bios_path = NULL;
    const char *neo_path = NULL;
    uint64_t max_dispatches = 0;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--bios") == 0) {
            if (++i >= argc) {
                usage(argv[0]);
                return 2;
            }
            bios_path = argv[i];
        } else if (strcmp(argv[i], "--max-dispatches") == 0) {
            if (++i >= argc ||
                !ng_generated_smoke_parse_u64(argv[i], &max_dispatches)) {
                usage(argv[0]);
                return 2;
            }
        } else if (strcmp(argv[i], "--help") == 0 ||
                   strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else if (!neo_path) {
            neo_path = argv[i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (!neo_path) {
        usage(argv[0]);
        return 2;
    }
    ng_generated_smoke_set_dispatch_budget(max_dispatches);
    return ng_generated_smoke_run_with_bios(neo_path, bios_path);
}
#endif
