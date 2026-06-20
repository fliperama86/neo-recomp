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

#define NG_GENERATED_SMOKE_DISPATCH_WATCHES(X) \
    X(NG_GENERATED_SMOKE_WATCH_CART_HEADER, 0x000122u, "cart_header") \
    X(NG_GENERATED_SMOKE_WATCH_CART_MODE_SELECT, 0x0007CCu, "cart_mode_select") \
    X(NG_GENERATED_SMOKE_WATCH_CART_MODE0_COLD, 0x00080Cu, "cart_mode0_cold") \
    X(NG_GENERATED_SMOKE_WATCH_CART_MODE1_START, 0x000832u, "cart_mode1_start") \
    X(NG_GENERATED_SMOKE_WATCH_CART_MODE2_START, 0x000836u, "cart_mode2_start") \
    X(NG_GENERATED_SMOKE_WATCH_CART_MODE3_START, 0x000840u, "cart_mode3_start") \
    X(NG_GENERATED_SMOKE_WATCH_CART_GAME_ENTRY, 0x000656u, "cart_game_entry") \
    X(NG_GENERATED_SMOKE_WATCH_CART_CLEAR, 0x000868u, "cart_clear") \
    X(NG_GENERATED_SMOKE_WATCH_CART_COLD_INIT, 0x024E38u, "cart_cold_init") \
    X(NG_GENERATED_SMOKE_WATCH_CART_MODE23_INIT, 0x00097Au, "cart_mode23_init") \
    X(NG_GENERATED_SMOKE_WATCH_CART_VBLANK_SETUP, 0x001F84u, "cart_vblank_setup") \
    X(NG_GENERATED_SMOKE_WATCH_CART_MAIN_WAIT, 0x001FE2u, "cart_main_wait") \
    X(NG_GENERATED_SMOKE_WATCH_CART_MAIN_WAIT_BRANCH, 0x00200Au, "cart_main_wait_branch") \
    X(NG_GENERATED_SMOKE_WATCH_CART_MAIN_JSR_INPUT, 0x002012u, "cart_main_jsr_input") \
    X(NG_GENERATED_SMOKE_WATCH_CART_VBLANK_READY, 0x001EECu, "cart_vblank_ready") \
    X(NG_GENERATED_SMOKE_WATCH_CART_VBLANK_JSR_VIDEO, 0x001EE6u, "cart_vblank_jsr_video") \
    X(NG_GENERATED_SMOKE_WATCH_CART_VBLANK_JSR_TASKS, 0x001E84u, "cart_vblank_jsr_tasks") \
    X(NG_GENERATED_SMOKE_WATCH_CART_MAIN_UPDATE, 0x002018u, "cart_main_update") \
    X(NG_GENERATED_SMOKE_WATCH_CART_POST_INPUT, 0x013C3Cu, "cart_post_input") \
    X(NG_GENERATED_SMOKE_WATCH_CART_INPUT_UPDATE, 0x05CC1Au, "cart_input_update") \
    X(NG_GENERATED_SMOKE_WATCH_CART_INPUT_COPY, 0x025066u, "cart_input_copy") \
    X(NG_GENERATED_SMOKE_WATCH_CART_VIDEO_GATE, 0x05C9D6u, "cart_video_gate") \
    X(NG_GENERATED_SMOKE_WATCH_CART_VIDEO_ACTIVE, 0x05C9E0u, "cart_video_active") \
    X(NG_GENERATED_SMOKE_WATCH_CART_VIDEO_JSR_RENDER, 0x05CA02u, "cart_video_jsr_render") \
    X(NG_GENERATED_SMOKE_WATCH_CART_VIDEO_SKIP_RETURN, 0x05CA28u, "cart_video_skip_return") \
    X(NG_GENERATED_SMOKE_WATCH_CART_RENDER_WORKER, 0x05B400u, "cart_render_worker") \
    X(NG_GENERATED_SMOKE_WATCH_CART_IRQ_VECTOR, 0x0008F6u, "cart_irq_vector") \
    X(NG_GENERATED_SMOKE_WATCH_CART_IRQ_HANDLER, 0x000906u, "cart_irq_handler") \
    X(NG_GENERATED_SMOKE_WATCH_CART_IRQ_BIOS_TAIL, 0x000934u, "cart_irq_bios_tail") \
    X(NG_GENERATED_SMOKE_WATCH_BIOS_VBLANK_FALLBACK, 0xC00438u, "bios_vblank_fallback") \
    X(NG_GENERATED_SMOKE_WATCH_BIOS_IRQ_TAIL, 0xC0044Au, "bios_irq_tail") \
    X(NG_GENERATED_SMOKE_WATCH_BIOS_IRQ_RETURN, 0xC004CEu, "bios_irq_return") \
    X(NG_GENERATED_SMOKE_WATCH_BIOS_SERVICE_SEED, 0xC11142u, "bios_service_seed") \
    X(NG_GENERATED_SMOKE_WATCH_BIOS_SERVICE_A, 0xC116A4u, "bios_service_a") \
    X(NG_GENERATED_SMOKE_WATCH_BIOS_SERVICE_B, 0xC1183Cu, "bios_service_b")

typedef struct NgGeneratedSmokeDispatchWatch {
    uint32_t addr;
    const char *label;
} NgGeneratedSmokeDispatchWatch;

enum {
#define NG_GENERATED_SMOKE_WATCH_ENUM(id, addr, label) id,
    NG_GENERATED_SMOKE_DISPATCH_WATCHES(NG_GENERATED_SMOKE_WATCH_ENUM)
#undef NG_GENERATED_SMOKE_WATCH_ENUM
    NG_GENERATED_SMOKE_WATCH_COUNT
};

static const NgGeneratedSmokeDispatchWatch g_ng_generated_smoke_dispatch_watch[
    NG_GENERATED_SMOKE_WATCH_COUNT] = {
#define NG_GENERATED_SMOKE_WATCH_INIT(id, addr, label) [id] = {addr, label},
    NG_GENERATED_SMOKE_DISPATCH_WATCHES(NG_GENERATED_SMOKE_WATCH_INIT)
#undef NG_GENERATED_SMOKE_WATCH_INIT
};

static uint32_t g_ng_generated_smoke_scanline_poll_interval = 1u;
static uint64_t
    g_ng_generated_smoke_instruction_watch_count[NG_GENERATED_SMOKE_WATCH_COUNT];

static void ng_generated_smoke_note_instruction_watch(uint32_t addr) {
    switch (addr & 0x00FFFFFFu) {
#define NG_GENERATED_SMOKE_INSTRUCTION_WATCH_CASE(id, watch_addr, label) \
    case watch_addr: \
        ++g_ng_generated_smoke_instruction_watch_count[id]; \
        break;
        NG_GENERATED_SMOKE_DISPATCH_WATCHES(
            NG_GENERATED_SMOKE_INSTRUCTION_WATCH_CASE)
#undef NG_GENERATED_SMOKE_INSTRUCTION_WATCH_CASE
    default:
        break;
    }
}

void ng_generated_instruction_hook(uint32_t addr) {
    ng_generated_smoke_note_instruction_watch(addr);
}

uint64_t ng_generated_smoke_instruction_watch_hits(uint32_t index) {
    return index < NG_GENERATED_SMOKE_WATCH_COUNT ?
        g_ng_generated_smoke_instruction_watch_count[index] : 0u;
}

void ng_generated_smoke_set_scanline_poll_interval(uint32_t interval) {
    g_ng_generated_smoke_scanline_poll_interval = interval;
}

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
static uint32_t g_ng_generated_smoke_last_cart_dispatch_addr;
static uint32_t g_ng_generated_smoke_last_bios_dispatch_addr;
static uint32_t g_ng_generated_smoke_recent_dispatches[NG_GENERATED_SMOKE_RECENT_DISPATCHES];
static uint32_t g_ng_generated_smoke_hot_addr[NG_GENERATED_SMOKE_HOT_SLOTS];
static uint64_t g_ng_generated_smoke_hot_count[NG_GENERATED_SMOKE_HOT_SLOTS];
static uint64_t
    g_ng_generated_smoke_dispatch_watch_count[NG_GENERATED_SMOKE_WATCH_COUNT];
static uint32_t g_ng_generated_smoke_unique_dispatch_count;
static int g_ng_generated_smoke_hot_overflow;
static uint32_t g_ng_generated_smoke_budget_stop_addr;
static int g_ng_generated_smoke_budget_hit;

void ng_generated_smoke_reset_dispatch_stats(void) {
    g_ng_generated_smoke_dispatch_count = 0;
    g_ng_generated_smoke_cart_dispatch_count = 0;
    g_ng_generated_smoke_bios_dispatch_count = 0;
    g_ng_generated_smoke_last_dispatch_addr = 0;
    g_ng_generated_smoke_last_cart_dispatch_addr = 0;
    g_ng_generated_smoke_last_bios_dispatch_addr = 0;
    memset(g_ng_generated_smoke_recent_dispatches,
           0,
           sizeof(g_ng_generated_smoke_recent_dispatches));
    memset(g_ng_generated_smoke_hot_addr,
           0,
           sizeof(g_ng_generated_smoke_hot_addr));
    memset(g_ng_generated_smoke_hot_count,
           0,
           sizeof(g_ng_generated_smoke_hot_count));
    memset(g_ng_generated_smoke_dispatch_watch_count,
           0,
           sizeof(g_ng_generated_smoke_dispatch_watch_count));
    memset(g_ng_generated_smoke_instruction_watch_count,
           0,
           sizeof(g_ng_generated_smoke_instruction_watch_count));
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

uint32_t ng_generated_smoke_last_cart_dispatch_addr(void) {
    return g_ng_generated_smoke_last_cart_dispatch_addr;
}

uint32_t ng_generated_smoke_last_bios_dispatch_addr(void) {
    return g_ng_generated_smoke_last_bios_dispatch_addr;
}

uint32_t ng_generated_smoke_dispatch_watch_count(void) {
    return NG_GENERATED_SMOKE_WATCH_COUNT;
}

uint32_t ng_generated_smoke_dispatch_watch_addr(uint32_t index) {
    return index < NG_GENERATED_SMOKE_WATCH_COUNT ?
        g_ng_generated_smoke_dispatch_watch[index].addr : 0u;
}

const char *ng_generated_smoke_dispatch_watch_label(uint32_t index) {
    return index < NG_GENERATED_SMOKE_WATCH_COUNT ?
        g_ng_generated_smoke_dispatch_watch[index].label : "";
}

uint64_t ng_generated_smoke_dispatch_watch_hits(uint32_t index) {
    return index < NG_GENERATED_SMOKE_WATCH_COUNT ?
        g_ng_generated_smoke_dispatch_watch_count[index] : 0u;
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

static void ng_generated_smoke_note_dispatch_watch(uint32_t addr) {
    switch (addr & 0x00FFFFFFu) {
#define NG_GENERATED_SMOKE_WATCH_CASE(id, watch_addr, label) \
    case watch_addr: \
        ++g_ng_generated_smoke_dispatch_watch_count[id]; \
        break;
        NG_GENERATED_SMOKE_DISPATCH_WATCHES(NG_GENERATED_SMOKE_WATCH_CASE)
#undef NG_GENERATED_SMOKE_WATCH_CASE
    default:
        break;
    }
}

static void ng_generated_smoke_note_dispatch(uint32_t addr) {
    addr &= 0x00FFFFFFu;
    ng_generated_smoke_note_dispatch_watch(addr);
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
        g_ng_generated_smoke_last_bios_dispatch_addr = bios_addr;
        ng_generated_smoke_call_bios(bios_addr);
        return;
    }
    ++g_ng_generated_smoke_cart_dispatch_count;
    g_ng_generated_smoke_last_cart_dispatch_addr = addr & 0x00FFFFFFu;
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
    memset(g_ng_generated_smoke_instruction_watch_count,
           0,
           sizeof(g_ng_generated_smoke_instruction_watch_count));
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

uint32_t ng_generated_smoke_last_cart_dispatch_addr(void) {
    return 0;
}

uint32_t ng_generated_smoke_last_bios_dispatch_addr(void) {
    return 0;
}

uint32_t ng_generated_smoke_recent_loop_period(void) {
    return 0;
}

uint32_t ng_generated_smoke_dispatch_watch_count(void) {
    return NG_GENERATED_SMOKE_WATCH_COUNT;
}

uint32_t ng_generated_smoke_dispatch_watch_addr(uint32_t index) {
    return index < NG_GENERATED_SMOKE_WATCH_COUNT ?
        g_ng_generated_smoke_dispatch_watch[index].addr : 0u;
}

const char *ng_generated_smoke_dispatch_watch_label(uint32_t index) {
    return index < NG_GENERATED_SMOKE_WATCH_COUNT ?
        g_ng_generated_smoke_dispatch_watch[index].label : "";
}

uint64_t ng_generated_smoke_dispatch_watch_hits(uint32_t index) {
    (void)index;
    return 0u;
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

static int ng_generated_smoke_snapshot_path(char *out,
                                            size_t out_size,
                                            const char *dir,
                                            const char *name) {
    if (!out || out_size == 0u || !dir || !*dir || !name || !*name) {
        return 0;
    }
    size_t dir_len = strlen(dir);
    const char *sep =
        (dir[dir_len - 1u] == '/' || dir[dir_len - 1u] == '\\') ? "" : "/";
    int written = snprintf(out, out_size, "%s%s%s", dir, sep, name);
    return written >= 0 && (size_t)written < out_size;
}

static int ng_generated_smoke_write_snapshot_bytes(const char *dir,
                                                   const char *name,
                                                   const void *data,
                                                   size_t size) {
    char path[4096];
    if (!data || !ng_generated_smoke_snapshot_path(path, sizeof(path), dir, name)) {
        return 0;
    }
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "failed to open snapshot file: %s\n", path);
        return 0;
    }
    if (size != 0u && fwrite(data, 1, size, f) != size) {
        fprintf(stderr, "failed to write snapshot file: %s\n", path);
        fclose(f);
        return 0;
    }
    if (fclose(f) != 0) {
        fprintf(stderr, "failed to close snapshot file: %s\n", path);
        return 0;
    }
    fprintf(stderr, "snapshot wrote %s (%llu bytes)\n",
            path,
            (unsigned long long)size);
    return 1;
}

static uint64_t ng_generated_smoke_mslug_sync_flags(void) {
    uint64_t packed = 0;
    for (uint32_t addr = 0x00106ED8u; addr <= 0x00106EDEu; ++addr) {
        packed = (packed << 8) | ng68k_read8(addr);
    }
    return packed;
}

static uint64_t ng_generated_smoke_mslug_sync_counters(void) {
    uint64_t packed = 0;
    packed = ((uint64_t)ng68k_read16(0x00106EE0u) << 32) |
             ((uint64_t)ng68k_read16(0x00106EE2u) << 16) |
             (uint64_t)ng68k_read16(0x00106EE4u);
    return packed;
}

static uint16_t ng_generated_smoke_mslug_vblank_selector(void) {
    return (uint16_t)(((uint16_t)ng68k_read8(0x00106F26u) << 8) |
                      (uint16_t)ng68k_read8(0x00106F27u));
}

static uint16_t ng_generated_smoke_mslug_bios_flags(void) {
    return (uint16_t)(((uint16_t)ng68k_read8(0x0010FD80u) << 8) |
                      (uint16_t)ng68k_read8(0x0010FDAEu));
}

static int ng_generated_smoke_write_snapshot_summary(const char *dir) {
    char path[4096];
    if (!ng_generated_smoke_snapshot_path(path, sizeof(path), dir, "summary.txt")) {
        return 0;
    }
    FILE *f = fopen(path, "w");
    if (!f) {
        fprintf(stderr, "failed to open snapshot file: %s\n", path);
        return 0;
    }

    fprintf(f, "dispatches=%llu\n",
            (unsigned long long)ng_generated_smoke_dispatch_count());
    fprintf(f, "cart=%llu\n",
            (unsigned long long)ng_generated_smoke_cart_dispatch_count());
    fprintf(f, "bios=%llu\n",
            (unsigned long long)ng_generated_smoke_bios_dispatch_count());
    fprintf(f, "unique=%u\n", ng_generated_smoke_unique_dispatch_count());
    fprintf(f, "hot_overflow=%u\n", ng_generated_smoke_dispatch_hot_overflow());
    fprintf(f, "last=$%06X\n", ng_generated_smoke_last_dispatch_addr() & 0x00FFFFFFu);
    fprintf(f, "last_cart=$%06X\n",
            ng_generated_smoke_last_cart_dispatch_addr() & 0x00FFFFFFu);
    fprintf(f, "last_bios=$%06X\n",
            ng_generated_smoke_last_bios_dispatch_addr() & 0x00FFFFFFu);
    fprintf(f, "pc=$%06X\n", g_ng_m68k.pc & 0x00FFFFFFu);
    fprintf(f, "sr=$%04X\n", g_ng_m68k.sr);
    fprintf(f, "sp=$%08X\n", g_ng_m68k.a[7]);
    fprintf(f, "polls=%u\n", ng_neogeo_interrupt_polls());
    fprintf(f, "watchdog=%u\n", ng_neogeo_watchdog_kicks());
    fprintf(f, "vblank=%u\n", ng_neogeo_vblank_interrupts());
    fprintf(f, "frame=%u\n", ng_neogeo_frame_count());
    fprintf(f, "timer_irq=%u\n", ng_neogeo_timer_interrupts());
    fprintf(f, "irqack=%u\n", ng_neogeo_irq_ack_writes());
    fprintf(f, "irq_pending=$%04X\n", ng_neogeo_irq_pending());
    fprintf(f, "last_irq_pc=$%06X\n",
            ng_neogeo_last_interrupt_return_pc() & 0x00FFFFFFu);
    fprintf(f, "last_irq_level=%u\n", ng_neogeo_last_interrupt_level());
    fprintf(f, "last_irq_vector=%u\n", ng_neogeo_last_interrupt_vector());
    fprintf(f, "scanline=%u\n", ng_neogeo_current_scanline());
    fprintf(f, "lspc=$%04X\n", ng_neogeo_lspc_mode());
    fprintf(f, "vram_addr=$%04X\n", ng_neogeo_vram_addr());
    fprintf(f, "vram_mod=$%04X\n", ng_neogeo_vram_mod());
    fprintf(f, "mslug_sync=$%014llX\n",
            (unsigned long long)ng_generated_smoke_mslug_sync_flags());
    fprintf(f, "mslug_counters=$%012llX\n",
            (unsigned long long)ng_generated_smoke_mslug_sync_counters());
    fprintf(f, "mslug_vblank=$%04X\n",
            ng_generated_smoke_mslug_vblank_selector());
    fprintf(f, "mslug_bios_flags=$%04X\n",
            ng_generated_smoke_mslug_bios_flags());
    fprintf(f, "sound=$%02X\n", ng_neogeo_sound_command());
    fprintf(f, "port=$%02X\n", ng_neogeo_port_output());
    fprintf(f, "wram_nonzero=%u\n", ng_neogeo_work_ram_nonzero_bytes());
    fprintf(f, "wram_sum=$%08X\n", ng_neogeo_work_ram_checksum());
    fprintf(f, "palette_nonzero=%u\n", ng_neogeo_palette_ram_nonzero_bytes());
    fprintf(f, "palette_sum=$%08X\n", ng_neogeo_palette_ram_checksum());
    fprintf(f, "palette_writes=%u\n", ng_neogeo_palette_write_count());
    fprintf(f, "palette_nonzero_writes=%u\n",
            ng_neogeo_palette_nonzero_write_count());
    fprintf(f, "palette_last_addr=$%06X\n",
            ng_neogeo_palette_last_addr() & 0x00FFFFFFu);
    fprintf(f, "palette_last_value=$%04X\n",
            ng_neogeo_palette_last_value());
    fprintf(f, "palette_last_bank=%u\n", ng_neogeo_palette_last_bank());
    fprintf(f, "palette_peak_nonzero=%u\n",
            ng_neogeo_palette_peak_nonzero_bytes());
    fprintf(f, "palette_peak_sum=$%08X\n",
            ng_neogeo_palette_peak_checksum());
    fprintf(f, "vram_nonzero=%u\n", ng_neogeo_vram_nonzero_words());
    fprintf(f, "vram_sum=$%08X\n", ng_neogeo_vram_checksum());
    fprintf(f, "recent_loop=%u\n", ng_generated_smoke_recent_loop_period());
    for (uint32_t i = 0; i < NG_GENERATED_SMOKE_HOT_TOP; ++i) {
        uint32_t addr = 0;
        uint64_t count = 0;
        ng_generated_smoke_hot_rank(i, &addr, &count);
        if (count == 0u) {
            break;
        }
        fprintf(f, "hot%u=$%06X:%llu\n",
                i,
                addr & 0x00FFFFFFu,
                (unsigned long long)count);
    }
    for (uint32_t i = 0; i < ng_generated_smoke_dispatch_watch_count(); ++i) {
        fprintf(f,
                "watch_%06X_%s=%llu\n",
                ng_generated_smoke_dispatch_watch_addr(i) & 0x00FFFFFFu,
                ng_generated_smoke_dispatch_watch_label(i),
                (unsigned long long)ng_generated_smoke_dispatch_watch_hits(i));
        fprintf(f,
                "instr_%06X_%s=%llu\n",
                ng_generated_smoke_dispatch_watch_addr(i) & 0x00FFFFFFu,
                ng_generated_smoke_dispatch_watch_label(i),
                (unsigned long long)ng_generated_smoke_instruction_watch_hits(i));
    }

    if (fclose(f) != 0) {
        fprintf(stderr, "failed to close snapshot file: %s\n", path);
        return 0;
    }
    fprintf(stderr, "snapshot wrote %s\n", path);
    return 1;
}

static int ng_generated_smoke_write_snapshot(const char *dir) {
    if (!dir || !*dir) {
        return 1;
    }

    uint8_t *work_ram = (uint8_t *)malloc(NG_NEO_WORK_RAM_BYTES);
    uint8_t *palette_ram = (uint8_t *)malloc(NG_NEO_PALETTE_RAM_BYTES);
    uint16_t *vram = (uint16_t *)malloc((size_t)NG_NEO_VRAM_WORDS *
                                        sizeof(*vram));
    uint8_t *vram_be = (uint8_t *)malloc(NG_NEO_VRAM_BYTES);
    if (!work_ram || !palette_ram || !vram || !vram_be) {
        fprintf(stderr, "failed to allocate snapshot buffers\n");
        free(work_ram);
        free(palette_ram);
        free(vram);
        free(vram_be);
        return 0;
    }

    int ok = 1;
    if (!ng_neogeo_copy_work_ram(work_ram, NG_NEO_WORK_RAM_BYTES) ||
        !ng_neogeo_copy_palette_ram(palette_ram, NG_NEO_PALETTE_RAM_BYTES) ||
        !ng_neogeo_copy_vram(vram, NG_NEO_VRAM_WORDS)) {
        ok = 0;
    }
    for (uint32_t i = 0; i < NG_NEO_VRAM_WORDS; ++i) {
        vram_be[i * 2u] = (uint8_t)(vram[i] >> 8);
        vram_be[i * 2u + 1u] = (uint8_t)vram[i];
    }

    if (ok) {
        ok = ng_generated_smoke_write_snapshot_bytes(
                 dir, "work_ram.bin", work_ram, NG_NEO_WORK_RAM_BYTES) &&
             ng_generated_smoke_write_snapshot_bytes(
                 dir, "palette_ram.bin", palette_ram, NG_NEO_PALETTE_RAM_BYTES) &&
             ng_generated_smoke_write_snapshot_bytes(
                 dir, "vram_be.bin", vram_be, NG_NEO_VRAM_BYTES) &&
             ng_generated_smoke_write_snapshot_summary(dir);
    }

    free(work_ram);
    free(palette_ram);
    free(vram);
    free(vram_be);
    return ok;
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
            "unique=%u hot_overflow=%u last=$%06X last_cart=$%06X "
            "last_bios=$%06X pc=$%06X sr=$%04X "
            "sp=$%08X polls=%u watchdog=%u vblank=%u frame=%u timer_irq=%u "
            "irqack=%u irq_pending=$%04X last_irq_pc=$%06X "
            "last_irq_level=%u last_irq_vector=%u "
            "scanline=%u lspc=$%04X vram_addr=$%04X vram_mod=$%04X "
            "mslug_sync=$%014llX mslug_counters=$%012llX "
            "mslug_vblank=$%04X mslug_bios_flags=$%04X sound=$%02X "
            "port=$%02X wram_nonzero=%u "
            "wram_sum=$%08X palette_nonzero=%u palette_sum=$%08X "
            "palette_writes=%u palette_nonzero_writes=%u "
            "palette_last_addr=$%06X palette_last_value=$%04X "
            "palette_last_bank=%u palette_peak_nonzero=%u "
            "palette_peak_sum=$%08X "
            "vram_nonzero=%u vram_sum=$%08X recent_loop=%u\n",
            (unsigned long long)ng_generated_smoke_dispatch_count(),
            (unsigned long long)ng_generated_smoke_cart_dispatch_count(),
            (unsigned long long)ng_generated_smoke_bios_dispatch_count(),
            ng_generated_smoke_unique_dispatch_count(),
            ng_generated_smoke_dispatch_hot_overflow(),
            ng_generated_smoke_last_dispatch_addr() & 0x00FFFFFFu,
            ng_generated_smoke_last_cart_dispatch_addr() & 0x00FFFFFFu,
            ng_generated_smoke_last_bios_dispatch_addr() & 0x00FFFFFFu,
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
            ng_neogeo_last_interrupt_return_pc() & 0x00FFFFFFu,
            ng_neogeo_last_interrupt_level(),
            ng_neogeo_last_interrupt_vector(),
            ng_neogeo_current_scanline(),
            ng_neogeo_lspc_mode(),
            ng_neogeo_vram_addr(),
            ng_neogeo_vram_mod(),
            (unsigned long long)ng_generated_smoke_mslug_sync_flags(),
            (unsigned long long)ng_generated_smoke_mslug_sync_counters(),
            ng_generated_smoke_mslug_vblank_selector(),
            ng_generated_smoke_mslug_bios_flags(),
            ng_neogeo_sound_command(),
            ng_neogeo_port_output(),
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
    fprintf(stderr, "dispatch watch:");
    for (uint32_t i = 0; i < ng_generated_smoke_dispatch_watch_count(); ++i) {
        fprintf(stderr,
                " $%06X:%s=%llu",
                ng_generated_smoke_dispatch_watch_addr(i) & 0x00FFFFFFu,
                ng_generated_smoke_dispatch_watch_label(i),
                (unsigned long long)ng_generated_smoke_dispatch_watch_hits(i));
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "instruction watch:");
    for (uint32_t i = 0; i < ng_generated_smoke_dispatch_watch_count(); ++i) {
        fprintf(stderr,
                " $%06X:%s=%llu",
                ng_generated_smoke_dispatch_watch_addr(i) & 0x00FFFFFFu,
                ng_generated_smoke_dispatch_watch_label(i),
                (unsigned long long)ng_generated_smoke_instruction_watch_hits(i));
    }
    fprintf(stderr, "\n");
    if (ng_generated_smoke_dispatch_budget_hit()) {
        fprintf(stderr,
                "smoke budget reached at $%06X after %llu dispatches\n",
                ng_generated_smoke_dispatch_budget_stop_addr() & 0x00FFFFFFu,
                (unsigned long long)ng_generated_smoke_dispatch_count());
    }
}

int ng_generated_smoke_run_with_bios_snapshot(const char *neo_path,
                                              const char *bios_path,
                                              const char *snapshot_dir) {
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
    ng_neogeo_set_auto_scanline_interval(
        bios_path ? g_ng_generated_smoke_scanline_poll_interval : 0u);
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
    int ok = ng_generated_smoke_write_snapshot(snapshot_dir);

    ng_neogeo_set_external_dispatch(NULL);
    ng_neogeo_set_auto_scanline_interval(0);
    ng_neogeo_set_auto_vblank_interval(0);
    ng_neogeo_set_system_rom(NULL, 0);
    ng_neogeo_set_program_rom(NULL, 0);
    free(bios_data);
    ng_program_rom_free(&rom);
    return ok ? 0 : 1;
}

int ng_generated_smoke_run_with_bios(const char *neo_path,
                                     const char *bios_path) {
    return ng_generated_smoke_run_with_bios_snapshot(neo_path, bios_path, NULL);
}

int ng_generated_smoke_run(const char *neo_path) {
    return ng_generated_smoke_run_with_bios(neo_path, NULL);
}

#ifndef NG_GENERATED_SMOKE_NO_MAIN
static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s [--bios <bios.rom>] [--max-dispatches <n>] "
            "[--scanline-poll-interval <n>] [--snapshot-dir <dir>] "
            "<game.neo>\n",
            argv0);
}

int main(int argc, char **argv) {
    const char *bios_path = NULL;
    const char *neo_path = NULL;
    const char *snapshot_dir = NULL;
    uint64_t max_dispatches = 0;
    uint64_t scanline_poll_interval = g_ng_generated_smoke_scanline_poll_interval;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--bios") == 0) {
            if (++i >= argc) {
                usage(argv[0]);
                return 2;
            }
            bios_path = argv[i];
        } else if (strcmp(argv[i], "--snapshot-dir") == 0) {
            if (++i >= argc) {
                usage(argv[0]);
                return 2;
            }
            snapshot_dir = argv[i];
        } else if (strcmp(argv[i], "--max-dispatches") == 0) {
            if (++i >= argc ||
                !ng_generated_smoke_parse_u64(argv[i], &max_dispatches)) {
                usage(argv[0]);
                return 2;
            }
        } else if (strcmp(argv[i], "--scanline-poll-interval") == 0) {
            if (++i >= argc ||
                !ng_generated_smoke_parse_u64(argv[i], &scanline_poll_interval) ||
                scanline_poll_interval > UINT32_MAX) {
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
    ng_generated_smoke_set_scanline_poll_interval(
        (uint32_t)scanline_poll_interval);
    return ng_generated_smoke_run_with_bios_snapshot(neo_path,
                                                    bios_path,
                                                    snapshot_dir);
}
#endif
