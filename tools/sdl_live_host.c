#include <SDL.h>

#include "ngrecomp/neogeo_audio.h"
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
#define NG_LIVE_PALETTE_BANK_ACTIVE (UINT32_MAX - 1u)
#define NG_LIVE_DEFAULT_DISPATCHES_PER_REFRESH 50000u
#define NG_LIVE_DEFAULT_FAST_FORWARD 0u
#define NG_LIVE_DEFAULT_SCALE 3u
#define NG_LIVE_DEFAULT_WATCHDOG_TIMEOUT_CYCLES NG_NEO_WATCHDOG_TIMEOUT_CPU_CYCLES
#define NG_LIVE_DEFAULT_VIDEO_SETTLE_DISPATCHES 16u
#define NG_LIVE_DEFAULT_FRAME_HOLD 1u
#define NG_LIVE_MAME_MASTER_CLOCK_HZ 24000000ull
#define NG_LIVE_MAME_PIXEL_CLOCK_HZ (NG_LIVE_MAME_MASTER_CLOCK_HZ / 4u)
#define NG_LIVE_MAME_HTOTAL 0x180u
#define NG_LIVE_MAME_VTOTAL 0x108u
#define NG_LIVE_MAX_CATCHUP_FRAMES 5u
#define NG_LIVE_MSLUG_VIDEO_PRESENT_ADDR 0x0005CA28u
#define NG_LIVE_AUDIO_SAMPLE_RATE 48000u
#define NG_LIVE_AUDIO_CHUNK_FRAMES 1024u
#define NG_LIVE_AUDIO_MAX_QUEUE_MS 250u
#define NG_LIVE_AUDIO_SYNC_CYCLES (NG_NEO_CPU_CYCLES_PER_SCANLINE / 8u)
#define NG_LIVE_AUDIO_COMMAND_SERVICE_MAX_US 50u
#define NG_LIVE_AUDIO_DIAGNOSTIC_COMMAND 0xD5u
#define NG_LIVE_AUDIO_NO_TEST_COMMAND UINT64_MAX
#define NG_LIVE_INPUT_NO_AUTO_FRAME UINT64_MAX
#define NG_LIVE_AUTO_COIN_HOLD_REFRESHES 6u
#define NG_LIVE_AUTO_START_HOLD_REFRESHES 30u
#define NG_LIVE_AUTO_BUTTON_HOLD_REFRESHES 30u

typedef struct NgLivePerfWindow {
    uint64_t cpu_ticks;
    uint64_t render_ticks;
    uint64_t sdl_ticks;
    uint64_t throttle_ticks;
    uint64_t refreshes;
} NgLivePerfWindow;

typedef struct NgLiveAudio {
    NgNeoAudio *audio;
    SDL_AudioDeviceID device;
    SDL_AudioSpec spec;
    uint64_t last_cpu_cycles;
    uint64_t z80_cycle_remainder;
    uint64_t sample_remainder;
    uint64_t generated_audio_frames;
    uint64_t nonzero_audio_samples;
    uint64_t sound_commands;
    uint64_t queue_wait_ms;
    uint32_t queue_clears;
    uint32_t max_queued_audio_bytes;
    int32_t peak_audio_sample;
    uint8_t last_sound_command;
    uint8_t recent_sound_commands[16];
    uint32_t recent_sound_command_head;
    int16_t output_buffer[NG_LIVE_AUDIO_CHUNK_FRAMES * 2u];
    uint32_t output_buffer_frames;
    int output_enabled;
    int device_open;
    int realtime_pacing;
} NgLiveAudio;

enum {
    NG_LIVE_P1_UP = 0x01u,
    NG_LIVE_P1_DOWN = 0x02u,
    NG_LIVE_P1_LEFT = 0x04u,
    NG_LIVE_P1_RIGHT = 0x08u,
    NG_LIVE_P1_A = 0x10u,
    NG_LIVE_P1_B = 0x20u,
    NG_LIVE_P1_C = 0x40u,
    NG_LIVE_P1_D = 0x80u,
    NG_LIVE_STATUS_P1_START = 0x01u,
    NG_LIVE_STATUS_P1_SELECT = 0x02u,
    NG_LIVE_AUDIO_COIN1 = 0x01u,
    NG_LIVE_AUDIO_SERVICE = 0x04u
};

typedef struct NgLiveInput {
    uint8_t p1_buttons;
    uint8_t p2_buttons;
    uint8_t status_a;
    uint8_t status_b;
    uint8_t dipswitch;
} NgLiveInput;

void ng_generated_call(uint32_t addr);
void ng_generated_smoke_reset_dispatch_stats(void);
void ng_generated_smoke_set_dispatch_budget(uint64_t max_dispatches);
void ng_generated_smoke_set_scanline_poll_interval(uint32_t interval);
void ng_generated_smoke_set_cycle_observer(void (*observer)(uint32_t addr,
                                                            uint32_t cycles));
void ng_generated_smoke_clear_instruction_yield(void);
void ng_generated_smoke_set_instruction_yield(uint32_t addr,
                                              uint32_t frame_must_differ);
void ng_generated_smoke_set_instruction_yield_frame_stop(uint32_t frame);
int ng_generated_smoke_instruction_yield_hit(void);
uint32_t ng_generated_smoke_instruction_yield_hit_addr(void);
uint32_t ng_generated_smoke_instruction_yield_hit_frame(void);
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

static int live_mslug_video_update_pending(void);

static uint64_t live_perf_delta(uint64_t start, uint64_t end) {
    return end >= start ? end - start : 0u;
}

static double live_perf_average_ms(uint64_t ticks,
                                   uint64_t refreshes,
                                   uint64_t perf_frequency) {
    if (ticks == 0u || refreshes == 0u || perf_frequency == 0u) {
        return 0.0;
    }
    return ((double)ticks * 1000.0) /
           ((double)perf_frequency * (double)refreshes);
}

static void live_perf_reset(NgLivePerfWindow *window) {
    if (window) {
        memset(window, 0, sizeof(*window));
    }
}

static void live_input_reset(NgLiveInput *input) {
    if (!input) {
        return;
    }
    input->p1_buttons = 0xFFu;
    input->p2_buttons = 0xFFu;
    input->status_a = 0xBFu;
    input->status_b = 0xFFu;
    input->dipswitch = 0xFFu;
}

static void live_input_apply(const NgLiveInput *input) {
    if (!input) {
        return;
    }
    ng_neogeo_set_p1_input(input->p1_buttons);
    ng_neogeo_set_p2_input(input->p2_buttons);
    ng_neogeo_set_status_a_input(input->status_a);
    ng_neogeo_set_status_b_input(input->status_b);
    ng_neogeo_set_dipswitch_input(input->dipswitch);
}

static void live_input_set_active_low(uint8_t *value, uint8_t mask, int pressed) {
    if (!value) {
        return;
    }
    if (pressed) {
        *value = (uint8_t)(*value & (uint8_t)~mask);
    } else {
        *value = (uint8_t)(*value | mask);
    }
}

static int live_input_handle_key(NgLiveInput *input, SDL_Keycode key, int pressed) {
    if (!input) {
        return 0;
    }
    switch (key) {
    case SDLK_UP:
        live_input_set_active_low(&input->p1_buttons, NG_LIVE_P1_UP, pressed);
        return 1;
    case SDLK_DOWN:
        live_input_set_active_low(&input->p1_buttons, NG_LIVE_P1_DOWN, pressed);
        return 1;
    case SDLK_LEFT:
        live_input_set_active_low(&input->p1_buttons, NG_LIVE_P1_LEFT, pressed);
        return 1;
    case SDLK_RIGHT:
        live_input_set_active_low(&input->p1_buttons, NG_LIVE_P1_RIGHT, pressed);
        return 1;
    case SDLK_z:
        live_input_set_active_low(&input->p1_buttons, NG_LIVE_P1_A, pressed);
        return 1;
    case SDLK_x:
        live_input_set_active_low(&input->p1_buttons, NG_LIVE_P1_B, pressed);
        return 1;
    case SDLK_c:
        live_input_set_active_low(&input->p1_buttons, NG_LIVE_P1_C, pressed);
        return 1;
    case SDLK_v:
        live_input_set_active_low(&input->p1_buttons, NG_LIVE_P1_D, pressed);
        return 1;
    case SDLK_RETURN:
        live_input_set_active_low(&input->status_b,
                                  NG_LIVE_STATUS_P1_START,
                                  pressed);
        return 1;
    case SDLK_RSHIFT:
    case SDLK_LSHIFT:
        live_input_set_active_low(&input->status_b,
                                  NG_LIVE_STATUS_P1_SELECT,
                                  pressed);
        return 1;
    case SDLK_5:
        live_input_set_active_low(&input->status_a, NG_LIVE_AUDIO_COIN1, pressed);
        return 1;
    case SDLK_6:
        live_input_set_active_low(&input->status_a,
                                  NG_LIVE_AUDIO_SERVICE,
                                  pressed);
        return 1;
    default:
        return 0;
    }
}

static uint64_t live_neo_frame_perf_ticks(uint64_t perf_frequency) {
    /* MAME Neo Geo timing:
       NEOGEO_MASTER_CLOCK = 24 MHz, NEOGEO_PIXEL_CLOCK = master / 4,
       screen().set_raw(pixel_clock, NEOGEO_HTOTAL=0x180, ...,
       NEOGEO_VTOTAL=0x108, ...).  That yields ~59.1856 Hz. */
    uint64_t frame_pixels =
        (uint64_t)NG_LIVE_MAME_HTOTAL * (uint64_t)NG_LIVE_MAME_VTOTAL;
    uint64_t ticks =
        (perf_frequency * frame_pixels + NG_LIVE_MAME_PIXEL_CLOCK_HZ / 2u) /
        NG_LIVE_MAME_PIXEL_CLOCK_HZ;
    return ticks != 0u ? ticks : 1u;
}

static void throttle_to_next_neo_frame(uint64_t *next_tick,
                                       uint64_t perf_frequency,
                                       uint64_t frame_ticks,
                                       int no_throttle) {
    if (no_throttle || !next_tick || perf_frequency == 0u ||
        frame_ticks == 0u) {
        return;
    }

    uint64_t now = SDL_GetPerformanceCounter();
    if (*next_tick == 0u) {
        *next_tick = now + frame_ticks;
    }

    while (now < *next_tick) {
        uint64_t remaining = *next_tick - now;
        uint64_t remaining_us = (remaining * 1000000u) / perf_frequency;
        (void)remaining_us;
        /* SDL_Delay(0/1) can overshoot by several milliseconds on some hosts.
           Use a short spin so MAME's 59.19 Hz cadence does not degrade into a
           visibly slower rate. */
        now = SDL_GetPerformanceCounter();
    }

    now = SDL_GetPerformanceCounter();
    if (now > *next_tick &&
        now - *next_tick > frame_ticks * NG_LIVE_MAX_CATCHUP_FRAMES) {
        /* If the host is massively late, reset the phase to avoid a long
           burst.  Smaller misses are caught up by skipping sleep only; we still
           render each emulated frame instead of intentionally dropping frames. */
        *next_tick = now + frame_ticks;
    } else {
        *next_tick += frame_ticks;
    }
}

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

static int make_child_path(char *out,
                           size_t out_size,
                           const char *dir,
                           const char *name) {
    if (!out || out_size == 0u || !dir || !*dir || !name || !*name) {
        return 0;
    }
    size_t dir_len = strlen(dir);
    const char *sep = (dir[dir_len - 1u] == '/' || dir[dir_len - 1u] == '\\') ?
        "" : "/";
    int written = snprintf(out, out_size, "%s%s%s", dir, sep, name);
    return written >= 0 && (size_t)written < out_size;
}

static int write_file_bytes(const char *path,
                            const uint8_t *data,
                            uint32_t size) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "failed to open %s for writing: %s\n", path, strerror(errno));
        return 0;
    }
    int ok = fwrite(data, 1, size, f) == size;
    if (fclose(f) != 0) {
        ok = 0;
    }
    if (!ok) {
        fprintf(stderr, "failed to write %s\n", path);
    }
    return ok;
}

static int dump_live_state(const char *dir) {
    if (!dir || !*dir) {
        return 1;
    }

    uint8_t *work_ram = (uint8_t *)malloc(NG_NEO_WORK_RAM_BYTES);
    uint8_t *palette_ram = (uint8_t *)malloc(NG_NEO_PALETTE_RAM_BYTES);
    uint8_t *backup_ram = (uint8_t *)malloc(NG_NEO_BACKUP_RAM_BYTES);
    uint16_t *vram = (uint16_t *)malloc((size_t)NG_NEO_VRAM_WORDS *
                                        sizeof(*vram));
    uint8_t *vram_be = (uint8_t *)malloc(NG_NEO_VRAM_BYTES);
    if (!work_ram || !palette_ram || !backup_ram || !vram || !vram_be) {
        fprintf(stderr, "failed to allocate live state dump buffers\n");
        free(work_ram);
        free(palette_ram);
        free(backup_ram);
        free(vram);
        free(vram_be);
        return 0;
    }

    int ok = ng_neogeo_copy_work_ram(work_ram, NG_NEO_WORK_RAM_BYTES) &&
             ng_neogeo_copy_palette_ram(palette_ram, NG_NEO_PALETTE_RAM_BYTES) &&
             ng_neogeo_copy_backup_ram(backup_ram, NG_NEO_BACKUP_RAM_BYTES) &&
             ng_neogeo_copy_vram(vram, NG_NEO_VRAM_WORDS);
    for (uint32_t i = 0; i < NG_NEO_VRAM_WORDS; ++i) {
        vram_be[i * 2u] = (uint8_t)(vram[i] >> 8);
        vram_be[i * 2u + 1u] = (uint8_t)vram[i];
    }

    char path[4096];
    if (ok && make_child_path(path, sizeof(path), dir, "work_ram.bin")) {
        ok = write_file_bytes(path, work_ram, NG_NEO_WORK_RAM_BYTES);
    }
    if (ok && make_child_path(path, sizeof(path), dir, "palette_ram.bin")) {
        ok = write_file_bytes(path, palette_ram, NG_NEO_PALETTE_RAM_BYTES);
    }
    if (ok && make_child_path(path, sizeof(path), dir, "backup_ram.bin")) {
        ok = write_file_bytes(path, backup_ram, NG_NEO_BACKUP_RAM_BYTES);
    }
    if (ok && make_child_path(path, sizeof(path), dir, "vram_be.bin")) {
        ok = write_file_bytes(path, vram_be, NG_NEO_VRAM_BYTES);
    }
    if (ok && make_child_path(path, sizeof(path), dir, "summary.txt")) {
        FILE *f = fopen(path, "w");
        if (!f) {
            fprintf(stderr, "failed to open %s for writing: %s\n", path, strerror(errno));
            ok = 0;
        } else {
            fprintf(f, "dispatches=%llu\n",
                    (unsigned long long)ng_generated_smoke_dispatch_count());
            fprintf(f, "frame=%u\n", ng_neogeo_frame_count());
            fprintf(f, "scanline=%u\n", ng_neogeo_current_scanline());
            fprintf(f, "pc=$%06X\n", g_ng_m68k.pc & 0x00FFFFFFu);
            fprintf(f, "lspc=$%04X\n", ng_neogeo_lspc_mode());
            fprintf(f, "palette_bank=%u\n", ng_neogeo_palette_bank());
            fprintf(f, "vram_addr=$%04X\n", ng_neogeo_vram_addr());
            fprintf(f, "vram_mod=$%04X\n", ng_neogeo_vram_mod());
            if (fclose(f) != 0) {
                ok = 0;
            }
        }
    }
    if (ok) {
        fprintf(stderr, "live state dumped to %s\n", dir);
    }

    free(work_ram);
    free(palette_ram);
    free(backup_ram);
    free(vram);
    free(vram_be);
    return ok;
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

static void live_audio_flush_output(NgLiveAudio *ctx);
static NgLiveAudio *g_live_audio_cycle_context;

static void live_audio_close(NgLiveAudio *ctx) {
    if (!ctx) {
        return;
    }
    if (g_live_audio_cycle_context == ctx) {
        ng_generated_smoke_set_cycle_observer(NULL);
        g_live_audio_cycle_context = NULL;
    }
    live_audio_flush_output(ctx);
    if (ctx->device_open) {
        SDL_CloseAudioDevice(ctx->device);
        ctx->device_open = 0;
        ctx->device = 0;
    }
    ng_neogeo_audio_destroy(ctx->audio);
    ctx->audio = NULL;
}

static int live_audio_create(NgLiveAudio *ctx,
                             const NgNeoRomImage *image,
                             int output_enabled) {
    if (!ctx) {
        return 0;
    }
    memset(ctx, 0, sizeof(*ctx));
    ctx->output_enabled = output_enabled;
    ctx->last_cpu_cycles = ng_neogeo_cpu_cycles();
    ctx->audio = ng_neogeo_audio_create();
    if (!ctx->audio) {
        fprintf(stderr, "warning: failed to create Neo Geo audio core; continuing silent\n");
        return 0;
    }
    ng_neogeo_audio_set_roms(ctx->audio,
                             image ? image->m.data : NULL,
                             image ? image->m.size : 0u,
                             image ? image->v1.data : NULL,
                             image ? image->v1.size : 0u,
                             image ? image->v2.data : NULL,
                             image ? image->v2.size : 0u);
    ng_neogeo_audio_reset(ctx->audio);
    ng_neogeo_set_sound_reply(ng_neogeo_audio_reply_latch(ctx->audio));
    return 1;
}

static void live_audio_open_device(NgLiveAudio *ctx) {
    if (!ctx || !ctx->audio || !ctx->output_enabled) {
        return;
    }

    SDL_AudioSpec want;
    memset(&want, 0, sizeof(want));
    want.freq = (int)NG_LIVE_AUDIO_SAMPLE_RATE;
    want.format = AUDIO_S16SYS;
    want.channels = 2;
    want.samples = NG_LIVE_AUDIO_CHUNK_FRAMES;
    want.callback = NULL;

    ctx->device = SDL_OpenAudioDevice(NULL,
                                      0,
                                      &want,
                                      &ctx->spec,
                                      SDL_AUDIO_ALLOW_FREQUENCY_CHANGE);
    if (ctx->device == 0) {
        fprintf(stderr,
                "warning: SDL audio device open failed: %s; continuing silent\n",
                SDL_GetError());
        memset(&ctx->spec, 0, sizeof(ctx->spec));
        return;
    }
    if (ctx->spec.format != AUDIO_S16SYS || ctx->spec.channels != 2) {
        fprintf(stderr,
                "warning: SDL audio returned unsupported format/channels; continuing silent\n");
        SDL_CloseAudioDevice(ctx->device);
        ctx->device = 0;
        memset(&ctx->spec, 0, sizeof(ctx->spec));
        return;
    }
    ctx->device_open = 1;
    SDL_PauseAudioDevice(ctx->device, 0);
}

static void live_audio_advance_z80(NgLiveAudio *ctx, uint64_t m68k_cycles) {
    if (!ctx || !ctx->audio || m68k_cycles == 0u) {
        return;
    }

    uint64_t total = ctx->z80_cycle_remainder + m68k_cycles;
    uint64_t z80_cycles = total / 3u;
    ctx->z80_cycle_remainder = total % 3u;
    while (z80_cycles != 0u) {
        uint32_t step = z80_cycles > UINT32_MAX ? UINT32_MAX : (uint32_t)z80_cycles;
        ng_neogeo_audio_advance_z80_cycles(ctx->audio, step);
        z80_cycles -= step;
    }
    ng_neogeo_set_sound_reply(ng_neogeo_audio_reply_latch(ctx->audio));
}

static void live_audio_flush_output(NgLiveAudio *ctx) {
    if (!ctx || !ctx->device_open || ctx->output_buffer_frames == 0u) {
        return;
    }

    uint32_t sample_rate = ctx->spec.freq > 0 ?
        (uint32_t)ctx->spec.freq : NG_LIVE_AUDIO_SAMPLE_RATE;
    uint32_t chunk_bytes =
        ctx->output_buffer_frames * 2u * (uint32_t)sizeof(int16_t);
    uint32_t queued = SDL_GetQueuedAudioSize(ctx->device);
    if (queued > ctx->max_queued_audio_bytes) {
        ctx->max_queued_audio_bytes = queued;
    }
    uint32_t max_queue =
        (sample_rate * 2u * (uint32_t)sizeof(int16_t) *
         NG_LIVE_AUDIO_MAX_QUEUE_MS) / 1000u;

    if (max_queue != 0u && queued + chunk_bytes > max_queue) {
        if (ctx->realtime_pacing) {
            uint32_t waited_ms = 0u;
            while (queued + chunk_bytes > max_queue &&
                   waited_ms < NG_LIVE_AUDIO_MAX_QUEUE_MS) {
                SDL_Delay(1u);
                ++waited_ms;
                queued = SDL_GetQueuedAudioSize(ctx->device);
                if (queued > ctx->max_queued_audio_bytes) {
                    ctx->max_queued_audio_bytes = queued;
                }
            }
            ctx->queue_wait_ms += waited_ms;
        }

        queued = SDL_GetQueuedAudioSize(ctx->device);
        if (queued > ctx->max_queued_audio_bytes) {
            ctx->max_queued_audio_bytes = queued;
        }
        if (!ctx->realtime_pacing && queued > max_queue) {
            SDL_ClearQueuedAudio(ctx->device);
            ++ctx->queue_clears;
        }
    }
    (void)SDL_QueueAudio(ctx->device,
                         ctx->output_buffer,
                         chunk_bytes);
    ctx->output_buffer_frames = 0u;
}

static void live_audio_record_and_buffer_samples(NgLiveAudio *ctx,
                                                 const int16_t *samples,
                                                 uint32_t frames) {
    if (!ctx || !samples || frames == 0u) {
        return;
    }

    ctx->generated_audio_frames += frames;
    for (uint32_t i = 0; i < frames * 2u; ++i) {
        int32_t sample = samples[i];
        int32_t abs_sample = sample < 0 ? -sample : sample;
        if (sample != 0) {
            ++ctx->nonzero_audio_samples;
        }
        if (abs_sample > ctx->peak_audio_sample) {
            ctx->peak_audio_sample = abs_sample;
        }
    }

    if (!ctx->device_open) {
        return;
    }

    uint32_t consumed = 0u;
    while (consumed < frames) {
        uint32_t room = NG_LIVE_AUDIO_CHUNK_FRAMES - ctx->output_buffer_frames;
        if (room == 0u) {
            live_audio_flush_output(ctx);
            room = NG_LIVE_AUDIO_CHUNK_FRAMES;
        }
        uint32_t copy_frames = frames - consumed;
        if (copy_frames > room) {
            copy_frames = room;
        }
        memcpy(&ctx->output_buffer[ctx->output_buffer_frames * 2u],
               &samples[consumed * 2u],
               (size_t)copy_frames * 2u * sizeof(int16_t));
        ctx->output_buffer_frames += copy_frames;
        consumed += copy_frames;
        if (ctx->output_buffer_frames == NG_LIVE_AUDIO_CHUNK_FRAMES) {
            live_audio_flush_output(ctx);
        }
    }
}

static void live_audio_generate_and_queue(NgLiveAudio *ctx, uint64_t m68k_cycles) {
    if (!ctx || !ctx->audio || m68k_cycles == 0u) {
        return;
    }

    uint32_t sample_rate = ctx->device_open && ctx->spec.freq > 0 ?
        (uint32_t)ctx->spec.freq : NG_LIVE_AUDIO_SAMPLE_RATE;
    uint64_t numerator =
        ctx->sample_remainder + m68k_cycles * (uint64_t)sample_rate;
    uint64_t frames = numerator / NG_NEO_MAIN_CPU_CLOCK_HZ;
    ctx->sample_remainder = numerator % NG_NEO_MAIN_CPU_CLOCK_HZ;

    int16_t samples[NG_LIVE_AUDIO_CHUNK_FRAMES * 2u];
    while (frames != 0u) {
        uint32_t chunk = frames > NG_LIVE_AUDIO_CHUNK_FRAMES ?
            NG_LIVE_AUDIO_CHUNK_FRAMES : (uint32_t)frames;
        ng_neogeo_audio_generate(ctx->audio, samples, chunk, sample_rate);
        live_audio_record_and_buffer_samples(ctx, samples, chunk);
        frames -= chunk;
    }
}

static void live_audio_advance_to_cpu_cycle(NgLiveAudio *ctx,
                                            uint64_t target_cpu_cycles) {
    if (!ctx || !ctx->audio) {
        return;
    }
    if (target_cpu_cycles <= ctx->last_cpu_cycles) {
        return;
    }
    while (ctx->last_cpu_cycles < target_cpu_cycles) {
        uint64_t delta = target_cpu_cycles - ctx->last_cpu_cycles;
        uint64_t step = delta > NG_NEO_CPU_CYCLES_PER_SCANLINE ?
            NG_NEO_CPU_CYCLES_PER_SCANLINE : delta;
        live_audio_advance_z80(ctx, step);
        live_audio_generate_and_queue(ctx, step);
        ctx->last_cpu_cycles += step;
    }
}

static void live_audio_send_command(NgLiveAudio *ctx, uint8_t command) {
    if (!ctx || !ctx->audio) {
        return;
    }
    uint32_t initial_clear_count =
        ng_neogeo_audio_command_clear_count(ctx->audio);
    ng_neogeo_audio_write_command(ctx->audio, command);
    ++ctx->sound_commands;
    ctx->last_sound_command = command;
    ctx->recent_sound_commands[ctx->recent_sound_command_head++ & 15u] =
        command;

    /* MAME and MiSTer both model a single command latch with NMI asserted on
       the 68000 sound-write edge and cleared when the Z80 reads/clears the
       latch.  The live host sees generated 68000 writes in coarse callback
       batches instead of true parallel CPU time, so service the Z80 through the
       Z80-side clear/write when the M1 program performs one, bounded by the
       existing 50 us safety cap.  Metal Slug's M1 commonly only reads the
       latch (MAME's generic latch treats the read as acknowledge), so the cap is
       still required to let the command handler run far enough to write YM/ADPCM
       registers after the read. */
    uint32_t command_service_z80_cycles =
        (uint32_t)(((uint64_t)NG_NEO_AUDIO_CPU_CLOCK_HZ *
                        (uint64_t)NG_LIVE_AUDIO_COMMAND_SERVICE_MAX_US +
                    999999ull) /
                   1000000ull);
    if (command_service_z80_cycles == 0u) {
        command_service_z80_cycles = 1u;
    }

    uint64_t before_z80 = ng_neogeo_audio_z80_cycles(ctx->audio);
    uint32_t advanced_z80 =
        ng_neogeo_audio_advance_z80_cycles_until_command_clear(
            ctx->audio,
            command_service_z80_cycles,
            initial_clear_count);
    uint64_t after_z80 = ng_neogeo_audio_z80_cycles(ctx->audio);
    if (after_z80 >= before_z80) {
        advanced_z80 = (uint32_t)(after_z80 - before_z80);
    }
    if (advanced_z80 == 0u) {
        return;
    }

    uint64_t command_service_cycles = (uint64_t)advanced_z80 * 3ull;
    live_audio_generate_and_queue(ctx, command_service_cycles);
    ctx->last_cpu_cycles += command_service_cycles;
    ng_neogeo_set_sound_reply(ng_neogeo_audio_reply_latch(ctx->audio));
}

static void live_audio_sync_to_runtime(NgLiveAudio *ctx) {
    if (!ctx || !ctx->audio) {
        return;
    }

    uint64_t current_cycles = ng_neogeo_cpu_cycles();
    NgNeoSoundCommandEvent event;
    while (ng_neogeo_pop_sound_command_event(&event)) {
        uint64_t event_cycles = event.cpu_cycles;
        if (event_cycles > current_cycles) {
            event_cycles = current_cycles;
        }
        live_audio_advance_to_cpu_cycle(ctx, event_cycles);
        live_audio_send_command(ctx, event.command);
    }
    live_audio_advance_to_cpu_cycle(ctx, current_cycles);
    ng_neogeo_set_sound_reply(ng_neogeo_audio_reply_latch(ctx->audio));
}

static void live_audio_cycle_observer(uint32_t addr, uint32_t cycles) {
    (void)addr;
    (void)cycles;
    NgLiveAudio *ctx = g_live_audio_cycle_context;
    if (!ctx || !ctx->audio) {
        return;
    }

    uint64_t current_cycles = ng_neogeo_cpu_cycles();
    if (ng_neogeo_sound_command_events_available() != 0u ||
        current_cycles - ctx->last_cpu_cycles >= NG_LIVE_AUDIO_SYNC_CYCLES) {
        live_audio_sync_to_runtime(ctx);
    }
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

static uint32_t resolve_palette_bank(const uint16_t *palette_words,
                                     uint32_t palette_bank_mode) {
    if (palette_bank_mode == NG_LIVE_PALETTE_BANK_AUTO) {
        return choose_palette_bank(palette_words);
    }
    if (palette_bank_mode == NG_LIVE_PALETTE_BANK_ACTIVE) {
        return ng_neogeo_palette_bank() & 1u;
    }
    return palette_bank_mode & 1u;
}

static const char *palette_bank_mode_name(uint32_t palette_bank_mode) {
    if (palette_bank_mode == NG_LIVE_PALETTE_BANK_AUTO) {
        return "auto";
    }
    if (palette_bank_mode == NG_LIVE_PALETTE_BANK_ACTIVE) {
        return "active";
    }
    return palette_bank_mode == 0u ? "0" : "1";
}

static uint8_t snapshot_auto_animation_counter(uint32_t frame_count, uint16_t lspc) {
    uint32_t speed = (uint32_t)(lspc >> 8);
    return (uint8_t)((frame_count + speed) / (speed + 1u));
}

static int live_external_dispatch(uint32_t addr) {
    static int dispatch_depth;
    static uint32_t active_addr;
    addr &= 0x00FFFFFFu;
    if (addr >= 0x00C00000u && addr <= 0x00CFFFFFu) {
        if (dispatch_depth != 0 && active_addr == addr) {
            return 0;
        }
        active_addr = addr;
        ++dispatch_depth;
        ng_generated_call(addr);
        --dispatch_depth;
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

static int run_until_instruction_or_frame_change(uint64_t target_dispatches,
                                                 uint32_t fallback_entry,
                                                 uint32_t yield_addr,
                                                 uint32_t frame_stop,
                                                 int *out_target_hit) {
    uint64_t current = ng_generated_smoke_dispatch_count();
    uint32_t addr = current == 0u ?
        fallback_entry :
        (g_ng_m68k.pc & 0x00FFFFFFu);

    if (out_target_hit) {
        *out_target_hit = 0;
    }
    if (current >= target_dispatches) {
        return 1;
    }

    ng_generated_smoke_clear_instruction_yield();
    ng_generated_smoke_set_instruction_yield(yield_addr, UINT32_MAX);
    ng_generated_smoke_set_instruction_yield_frame_stop(frame_stop);
    ng_generated_smoke_set_dispatch_budget(target_dispatches);
    ng_generated_call(addr);

    int yield_hit = ng_generated_smoke_instruction_yield_hit();
    uint32_t yield_hit_addr =
        ng_generated_smoke_instruction_yield_hit_addr() & 0x00FFFFFFu;
    ng_generated_smoke_clear_instruction_yield();
    if (yield_hit) {
        if (out_target_hit) {
            *out_target_hit =
                yield_addr != UINT32_MAX &&
                yield_hit_addr == (yield_addr & 0x00FFFFFFu);
        }
        return 1;
    }
    if (ng_generated_smoke_dispatch_budget_hit()) {
        return 1;
    }
    if (ng_generated_smoke_dispatch_count() < target_dispatches) {
        fprintf(stderr,
                "generated dispatch returned before yield/budget target: "
                "dispatches=%llu target=%llu pc=$%06X yield=$%06X frame_stop=%u\n",
                (unsigned long long)ng_generated_smoke_dispatch_count(),
                (unsigned long long)target_dispatches,
                g_ng_m68k.pc & 0x00FFFFFFu,
                yield_addr & 0x00FFFFFFu,
                frame_stop);
        return 0;
    }
    return 1;
}

static int run_frame_synced_slice(uint64_t max_dispatches, uint32_t fallback_entry) {
    uint64_t start = ng_generated_smoke_dispatch_count();
    uint64_t limit = UINT64_MAX;
    if (max_dispatches < UINT64_MAX - start) {
        limit = start + max_dispatches;
    }
    return run_until_instruction_or_frame_change(limit,
                                                 fallback_entry,
                                                 UINT32_MAX,
                                                 ng_neogeo_frame_count(),
                                                 NULL);
}

static int run_video_synced_slice(uint64_t max_dispatches,
                                  uint64_t video_settle_dispatches,
                                  uint32_t fallback_entry) {
    uint32_t start_frame = ng_neogeo_frame_count();
    uint64_t start = ng_generated_smoke_dispatch_count();
    uint64_t limit = UINT64_MAX;
    if (max_dispatches < UINT64_MAX - start) {
        limit = start + max_dispatches;
    }

    if (!run_frame_synced_slice(max_dispatches, fallback_entry)) {
        return 0;
    }
    if (ng_neogeo_frame_count() == start_frame ||
        ng_generated_smoke_dispatch_count() >= limit) {
        return 1;
    }
    if (!live_mslug_video_update_pending()) {
        return 1;
    }

    uint64_t settle_limit = limit;
    uint64_t current = ng_generated_smoke_dispatch_count();
    if (video_settle_dispatches < settle_limit - current) {
        settle_limit = current + video_settle_dispatches;
    }

    return run_until_instruction_or_frame_change(
        settle_limit,
        fallback_entry,
        NG_LIVE_MSLUG_VIDEO_PRESENT_ADDR,
        ng_neogeo_frame_count(),
        NULL);
}

static int run_fast_forward(uint64_t target_dispatches,
                            uint32_t entry_addr,
                            NgLiveAudio *audio) {
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
        live_audio_sync_to_runtime(audio);
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
                             uint32_t palette_bank_mode,
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

    uint32_t palette_bank = resolve_palette_bank(palette_words, palette_bank_mode);
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

typedef struct NgLiveSpriteStats {
    uint32_t max_active_per_line;
    uint32_t saturated_lines;
    uint32_t total_active_samples;
} NgLiveSpriteStats;

typedef enum NgLivePresentMode {
    NG_LIVE_PRESENT_FRAME,
    NG_LIVE_PRESENT_VIDEO,
    NG_LIVE_PRESENT_SLICE
} NgLivePresentMode;

static const char *present_mode_name(NgLivePresentMode mode) {
    if (mode == NG_LIVE_PRESENT_SLICE) {
        return "slice";
    }
    if (mode == NG_LIVE_PRESENT_VIDEO) {
        return "video";
    }
    return "frame";
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

static uint16_t live_sprite_y(uint16_t scb3_word) {
    return (uint16_t)((496u - ((scb3_word >> 7) & 0x01FFu)) & 0x01FFu);
}

static int live_sprite_on_scanline(uint32_t scanline,
                                   uint16_t y,
                                   uint8_t rows) {
    return rows >= 0x20u ||
           (((scanline - (uint32_t)y) & 0x01FFu) <
            (uint32_t)rows * NG_NEO_SPRITE_TILE_PIXELS);
}

static NgLiveSpriteStats analyze_live_sprites(void) {
    NgLiveSpriteStats stats;
    uint16_t vram_words[NG_NEO_VRAM_WORDS];
    memset(&stats, 0, sizeof(stats));
    if (!ng_neogeo_copy_vram(vram_words, NG_NEO_VRAM_WORDS)) {
        return stats;
    }

    for (uint32_t scanline = 0;
         scanline < NG_NEO_SPRITE_FRAME_HEIGHT;
         ++scanline) {
        uint16_t y = 0;
        uint8_t rows = 0;
        uint32_t active_count = 0;
        for (uint16_t sprite = 0;
             sprite < NG_NEO_SPRITE_DISPLAY_LIMIT;
             ++sprite) {
            uint16_t scb3 = vram_words[0x8200u + sprite];
            if (((scb3 >> 6) & 1u) == 0u) {
                y = live_sprite_y(scb3);
                rows = (uint8_t)(scb3 & 0x3Fu);
            }

            if (rows == 0u ||
                !live_sprite_on_scanline(scanline, y, rows)) {
                continue;
            }

            ++active_count;
            if (active_count == NG_NEO_SPRITES_PER_SCANLINE) {
                ++stats.saturated_lines;
                break;
            }
        }
        if (active_count > stats.max_active_per_line) {
            stats.max_active_per_line = active_count;
        }
        stats.total_active_samples += active_count;
    }
    return stats;
}

static uint32_t current_live_auto_palette_bank(void) {
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

static uint32_t current_live_selected_palette_bank(uint32_t palette_bank_mode) {
    if (palette_bank_mode == NG_LIVE_PALETTE_BANK_AUTO) {
        return current_live_auto_palette_bank();
    }
    if (palette_bank_mode == NG_LIVE_PALETTE_BANK_ACTIVE) {
        return ng_neogeo_palette_bank() & 1u;
    }
    return palette_bank_mode & 1u;
}

static uint64_t live_mslug_sync_flags(void) {
    uint64_t packed = 0;
    for (uint32_t addr = 0x00106ED8u; addr <= 0x00106EDEu; ++addr) {
        packed = (packed << 8) | ng68k_read8(addr);
    }
    return packed;
}

static int live_mslug_video_update_pending(void) {
    return ng68k_read8(0x0010E1ECu) != 0u;
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
            "cycle=%llu pc=$%06X sr=$%04X cap=%llu irq_pending=$%04X polls=%u "
            "last_irq_pc=$%06X last_irq_level=%u last_irq_vector=%u "
            "budget_stop=$%06X\n",
            label,
            (unsigned long long)refreshes,
            (unsigned long long)ng_generated_smoke_dispatch_count(),
            ng_neogeo_frame_count(),
            ng_neogeo_current_scanline(),
            (unsigned long long)ng_neogeo_cpu_cycles(),
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
                                   uint32_t palette_bank_mode,
                                   const uint32_t *pixels,
                                   uint32_t width,
                                   uint32_t height) {
    NgLivePixelStats pixel_stats = analyze_live_pixels(pixels, width, height);
    NgLiveSpriteStats sprite_stats = analyze_live_sprites();
    NgLiveBackupTableStats backup_table = live_backup_table_stats();
    uint32_t selected_palette_bank =
        current_live_selected_palette_bank(palette_bank_mode);
    uint32_t auto_palette_bank = current_live_auto_palette_bank();
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
            "wd_timeout_cycles=%u wd_gap_cycles=%llu wd_max_gap_cycles=%llu "
            "wd_timeout_polls=%u wd_gap=%u wd_max_gap=%u wd_resets=%u "
            "wd_pending=%u wd_reset=$%06X@%u "
            "lspc=$%04X timer_reload=$%08X timer_counter=$%08X timer_stop=$%04X "
            "vram_addr=$%04X vram_mod=$%04X shadow=%u bios_vectors=%u fix=%u "
            "sound=$%02X port=$%02X\n",
            ng_neogeo_watchdog_kicks(),
            ng_neogeo_vblank_interrupts(),
            ng_neogeo_timer_interrupts(),
            ng_neogeo_irq_ack_writes(),
            ng_neogeo_watchdog_timeout_cycles(),
            (unsigned long long)(ng_neogeo_cpu_cycles() -
                                 ng_neogeo_watchdog_last_kick_cycle()),
            (unsigned long long)ng_neogeo_watchdog_max_gap_cycles(),
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
            "palette_peak=%u/$%08X palette_selected=%u palette_active=%u "
            "palette_auto=%u palette_mode=%s vram_nonzero=%u "
            "vram_sum=$%08X sprite_max_line=%u sprite_sat_lines=%u "
            "sprite_samples=%u pixels_nonzero=%u pixels_nonblack=%u "
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
            selected_palette_bank,
            ng_neogeo_palette_bank() & 1u,
            auto_palette_bank,
            palette_bank_mode_name(palette_bank_mode),
            ng_neogeo_vram_nonzero_words(),
            ng_neogeo_vram_checksum(),
            sprite_stats.max_active_per_line,
            sprite_stats.saturated_lines,
            sprite_stats.total_active_samples,
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
    if (strcmp(text, "video") == 0 || strcmp(text, "vblank-video") == 0) {
        *out = NG_LIVE_PRESENT_VIDEO;
        return 1;
    }
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

static int parse_palette_bank_mode(const char *text, uint32_t *out) {
    if (strcmp(text, "active") == 0 || strcmp(text, "latch") == 0) {
        *out = NG_LIVE_PALETTE_BANK_ACTIVE;
        return 1;
    }
    if (strcmp(text, "auto") == 0) {
        *out = NG_LIVE_PALETTE_BANK_AUTO;
        return 1;
    }
    if (strcmp(text, "0") == 0) {
        *out = 0u;
        return 1;
    }
    if (strcmp(text, "1") == 0) {
        *out = 1u;
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
            "  --dispatches-per-refresh <n>   safety dispatch cap per rendered refresh (default: %u)\n"
            "  --present-mode <frame|video|slice>\n"
            "                                  frame-boundary, video-update-settled, or fixed-slice presentation (default: frame)\n"
            "  --video-settle-dispatches <n>  extra cap for video-update settle mode (default: %u)\n"
            "  --frame-hold <n>               present each emulated frame n times for slow inspection (default: %u)\n"
            "  --palette-bank <active|auto|0|1>\n"
            "                                  displayed palette bank (default: active latch)\n"
            "  --scanline-poll-interval <n>   legacy scanline poll interval when cycle hooks are disabled (default: 0)\n"
            "  --watchdog-timeout-cycles <n>  reset after n 68k cycles without watchdog kick (default: %u, 0 disables)\n"
            "  --watchdog-timeout-polls <n>   legacy reset after n interrupt polls without watchdog kick (default: 0)\n"
            "  --start-bios                   enter through the BIOS reset vector instead of cart header\n"
            "  --max-refreshes <n>            exit after n presented frames\n"
            "  --status-interval <n>          log status every n presented frames\n"
            "  --diagnostics-interval <n>     log detailed diagnostics every n frames\n"
            "  --perf-log                     log rolling CPU/render/SDL frame costs\n"
            "  --dump-state-dir <dir>         write work/backup/palette/VRAM dumps on exit\n"
            "  --stall-refreshes <n>          log if scanline/frame stalls for n refreshes (default: 180, 0 disables)\n"
            "  --no-audio                     run the Z80/YM2610 core but do not open SDL audio output\n"
            "  --audio-test-command <n>       inject one M1 command at launch for audio diagnostics (for mslug, 0xD5 is audible)\n"
            "  --auto-coin-frame <n>          hold coin 1 for a few refreshes starting at n (diagnostic input automation)\n"
            "  --auto-start-frame <n>         hold P1 start for a few refreshes starting at n (diagnostic input automation)\n"
            "  --auto-p1-a-frame <n>          hold P1 A for a few refreshes starting at n (diagnostic input automation)\n"
            "  --no-throttle                  do not sleep to ~60Hz after each refresh\n"
            "  --scale <n>                    integer window scale (default: %u)\n",
            argv0,
            NG_LIVE_DEFAULT_DISPATCHES_PER_REFRESH,
            NG_LIVE_DEFAULT_VIDEO_SETTLE_DISPATCHES,
            NG_LIVE_DEFAULT_FRAME_HOLD,
            NG_LIVE_DEFAULT_WATCHDOG_TIMEOUT_CYCLES,
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
    uint64_t scanline_poll_interval = 0u;
    uint64_t watchdog_timeout_cycles = NG_LIVE_DEFAULT_WATCHDOG_TIMEOUT_CYCLES;
    uint64_t watchdog_timeout_polls = 0u;
    uint64_t video_settle_dispatches = NG_LIVE_DEFAULT_VIDEO_SETTLE_DISPATCHES;
    uint64_t frame_hold = NG_LIVE_DEFAULT_FRAME_HOLD;
    uint64_t scale = NG_LIVE_DEFAULT_SCALE;
    uint64_t audio_test_command = NG_LIVE_AUDIO_NO_TEST_COMMAND;
    uint64_t auto_coin_frame = NG_LIVE_INPUT_NO_AUTO_FRAME;
    uint64_t auto_start_frame = NG_LIVE_INPUT_NO_AUTO_FRAME;
    uint64_t auto_p1_a_frame = NG_LIVE_INPUT_NO_AUTO_FRAME;
    int start_bios = 0;
    int perf_log = 0;
    int audio_output_enabled = 1;
    const char *dump_state_dir = NULL;
    NgLivePresentMode present_mode = NG_LIVE_PRESENT_FRAME;
    uint32_t palette_bank_mode = NG_LIVE_PALETTE_BANK_ACTIVE;

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
                        "%s requires one of: video, frame, slice\n",
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
        } else if (strcmp(option, "--present-video") == 0) {
            present_mode = NG_LIVE_PRESENT_VIDEO;
            continue;
        } else if (strcmp(option, "--palette-bank") == 0) {
            if (argi >= argc ||
                !parse_palette_bank_mode(argv[argi++], &palette_bank_mode)) {
                fprintf(stderr,
                        "%s requires one of: active, auto, 0, 1\n",
                        option);
                usage(argv[0]);
                return 2;
            }
            continue;
        } else if (strcmp(option, "--scanline-poll-interval") == 0) {
            target = &scanline_poll_interval;
        } else if (strcmp(option, "--watchdog-timeout-polls") == 0) {
            target = &watchdog_timeout_polls;
        } else if (strcmp(option, "--watchdog-timeout-cycles") == 0) {
            target = &watchdog_timeout_cycles;
        } else if (strcmp(option, "--video-settle-dispatches") == 0) {
            target = &video_settle_dispatches;
        } else if (strcmp(option, "--frame-hold") == 0 ||
                   strcmp(option, "--hold") == 0 ||
                   strcmp(option, "--slowmo") == 0) {
            target = &frame_hold;
        } else if (strcmp(option, "--start-bios") == 0) {
            start_bios = 1;
            continue;
        } else if (strcmp(option, "--max-refreshes") == 0) {
            target = &max_refreshes;
        } else if (strcmp(option, "--status-interval") == 0) {
            target = &status_interval;
        } else if (strcmp(option, "--diagnostics-interval") == 0) {
            target = &diagnostics_interval;
        } else if (strcmp(option, "--perf-log") == 0) {
            perf_log = 1;
            continue;
        } else if (strcmp(option, "--dump-state-dir") == 0) {
            if (argi >= argc) {
                fprintf(stderr, "%s requires a directory path\n", option);
                usage(argv[0]);
                return 2;
            }
            dump_state_dir = argv[argi++];
            continue;
        } else if (strcmp(option, "--stall-refreshes") == 0) {
            target = &stall_refreshes;
        } else if (strcmp(option, "--scale") == 0) {
            target = &scale;
        } else if (strcmp(option, "--audio-test-command") == 0) {
            target = &audio_test_command;
        } else if (strcmp(option, "--auto-coin-frame") == 0) {
            target = &auto_coin_frame;
        } else if (strcmp(option, "--auto-start-frame") == 0) {
            target = &auto_start_frame;
        } else if (strcmp(option, "--auto-p1-a-frame") == 0) {
            target = &auto_p1_a_frame;
        } else if (strcmp(option, "--no-throttle") == 0) {
            no_throttle = 1u;
            continue;
        } else if (strcmp(option, "--no-audio") == 0) {
            audio_output_enabled = 0;
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
    if (dispatches_per_refresh == 0u || frame_hold == 0u ||
        scanline_poll_interval > UINT32_MAX ||
        watchdog_timeout_cycles > UINT32_MAX ||
        watchdog_timeout_polls > UINT32_MAX ||
        (audio_test_command != NG_LIVE_AUDIO_NO_TEST_COMMAND &&
         audio_test_command > 0xFFu) ||
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
    NgLiveAudio live_audio;
    NgLiveInput live_input;
    memset(&live_audio, 0, sizeof(live_audio));
    live_input_reset(&live_input);

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
    live_input_apply(&live_input);
    if (!start_bios) {
        /* We enter through the cart header for fast iteration, bypassing the
         * cold MVS BIOS save-RAM directory setup.  Seed the same minimal
         * Metal Slug directory that a fresh MAME/MVS boot creates so BIOS
         * backup-RAM load/save services address the game block at $D00320
         * instead of scanning uninitialized directory data. */
        ng_neogeo_seed_mslug_backup_ram();
    }
    ng_neogeo_set_program_rom(image.p.data, image.p.size);
    ng_neogeo_set_system_rom(bios_data, bios_size);
    ng_neogeo_set_external_dispatch(live_external_dispatch);
    ng_neogeo_set_auto_scanline_interval((uint32_t)scanline_poll_interval);
    ng_neogeo_set_watchdog_reset_vector(initial_entry, ng_program_rom_initial_ssp(&rom));
    ng_neogeo_set_watchdog_timeout_cycles((uint32_t)watchdog_timeout_cycles);
    ng_neogeo_set_watchdog_timeout_polls((uint32_t)watchdog_timeout_polls);
    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    g_ng_m68k.ssp = ng_program_rom_initial_ssp(&rom);
    g_ng_m68k.a[7] = g_ng_m68k.ssp;
    g_ng_m68k.sr = 0x2700u;
    ng_generated_smoke_reset_dispatch_stats();
    ng_generated_smoke_set_scanline_poll_interval((uint32_t)scanline_poll_interval);
    (void)live_audio_create(&live_audio, &image, audio_output_enabled);
    live_audio.realtime_pacing = no_throttle == 0u;
    if (live_audio.audio) {
        g_live_audio_cycle_context = &live_audio;
        ng_generated_smoke_set_cycle_observer(live_audio_cycle_observer);
    }

    if (!run_fast_forward(fast_forward, initial_entry, &live_audio)) {
        live_audio_close(&live_audio);
        ng_neo_rom_image_free(&image);
        free(bios_data);
        free(zoom_rom);
        return 1;
    }
    if (audio_test_command != NG_LIVE_AUDIO_NO_TEST_COMMAND) {
        live_audio_send_command(&live_audio, (uint8_t)audio_test_command);
    }

    const uint32_t width = NG_NEO_SPRITE_FRAME_WIDTH;
    const uint32_t height = NG_NEO_SPRITE_FRAME_HEIGHT;
    pixels = (uint32_t *)calloc((size_t)width * (size_t)height, sizeof(uint32_t));
    if (!pixels) {
        fprintf(stderr, "failed to allocate framebuffer\n");
        live_audio_close(&live_audio);
        ng_neo_rom_image_free(&image);
        free(bios_data);
        free(zoom_rom);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_AUDIO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        live_audio_close(&live_audio);
        free(pixels);
        ng_neo_rom_image_free(&image);
        free(bios_data);
        free(zoom_rom);
        return 1;
    }
    live_audio_open_device(&live_audio);
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
        live_audio_close(&live_audio);
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
            "frame=%u scanline=%u present=%s palette=%s watchdog_cycles=%llu "
            "video_settle=%llu frame_hold=%llu audio=%s/%uHz ym=%uHz\n",
            initial_entry & 0x00FFFFFFu,
            cart_entry & 0x00FFFFFFu,
            (unsigned long long)ng_generated_smoke_dispatch_count(),
            ng_neogeo_frame_count(),
            ng_neogeo_current_scanline(),
            present_mode_name(present_mode),
            palette_bank_mode_name(palette_bank_mode),
            (unsigned long long)watchdog_timeout_cycles,
            (unsigned long long)video_settle_dispatches,
            (unsigned long long)frame_hold,
            live_audio.device_open ? "on" :
                (live_audio.output_enabled ? "silent" : "off"),
            live_audio.device_open && live_audio.spec.freq > 0 ?
                (uint32_t)live_audio.spec.freq : NG_LIVE_AUDIO_SAMPLE_RATE,
            ng_neogeo_audio_ym2610_native_sample_rate(live_audio.audio));
    fprintf(stderr,
            "keys: q/Escape quit, Space pause/resume, n/. step one frame, m audio test, +/- cap; P1 arrows+Z/X/C/V, Enter start, 5 coin\n");

    int quit = 0;
    int paused = 0;
    int step_requested = 0;
    int stall_reported = 0;
    uint64_t stagnant_refreshes = 0;
    uint64_t hold_phase = 0;
    uint64_t perf_frequency = SDL_GetPerformanceFrequency();
    uint64_t neo_frame_ticks = live_neo_frame_perf_ticks(perf_frequency);
    uint64_t next_present_tick = SDL_GetPerformanceCounter() + neo_frame_ticks;
    uint64_t fps_last_tick = SDL_GetPerformanceCounter();
    uint64_t fps_last_refreshes = 0;
    uint32_t fps_last_frame = ng_neogeo_frame_count();
    double present_fps = 0.0;
    double emulated_fps = 0.0;
    double perf_cpu_ms = 0.0;
    double perf_render_ms = 0.0;
    double perf_sdl_ms = 0.0;
    double perf_throttle_ms = 0.0;
    NgLivePerfWindow perf_window;
    live_perf_reset(&perf_window);
    uint32_t last_progress_frame = ng_neogeo_frame_count();
    uint16_t last_progress_scanline = ng_neogeo_current_scanline();
    uint64_t refreshes = 0;
    while (!quit) {
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
                } else if (key == SDLK_n || key == SDLK_PERIOD) {
                    step_requested = 1;
                    hold_phase = 0;
                } else if (key == SDLK_m) {
                    live_audio_send_command(&live_audio,
                                            NG_LIVE_AUDIO_DIAGNOSTIC_COMMAND);
                } else if (key == SDLK_EQUALS || key == SDLK_PLUS) {
                    dispatches_per_refresh += dispatches_per_refresh / 4u + 1u;
                } else if (key == SDLK_MINUS && dispatches_per_refresh > 1u) {
                    dispatches_per_refresh -= dispatches_per_refresh / 4u + 1u;
                    if (dispatches_per_refresh == 0u) {
                        dispatches_per_refresh = 1u;
                    }
                } else if (live_input_handle_key(&live_input, key, 1)) {
                    live_input_apply(&live_input);
                }
            } else if (event.type == SDL_KEYUP) {
                SDL_Keycode key = event.key.keysym.sym;
                if (live_input_handle_key(&live_input, key, 0)) {
                    live_input_apply(&live_input);
                }
            }
        }
        if (auto_coin_frame != NG_LIVE_INPUT_NO_AUTO_FRAME) {
            int active = refreshes >= auto_coin_frame &&
                refreshes < auto_coin_frame + NG_LIVE_AUTO_COIN_HOLD_REFRESHES;
            live_input_set_active_low(&live_input.status_a,
                                      NG_LIVE_AUDIO_COIN1,
                                      active);
        }
        if (auto_start_frame != NG_LIVE_INPUT_NO_AUTO_FRAME) {
            int active = refreshes >= auto_start_frame &&
                refreshes < auto_start_frame + NG_LIVE_AUTO_START_HOLD_REFRESHES;
            live_input_set_active_low(&live_input.status_b,
                                      NG_LIVE_STATUS_P1_START,
                                      active);
        }
        if (auto_p1_a_frame != NG_LIVE_INPUT_NO_AUTO_FRAME) {
            int active = refreshes >= auto_p1_a_frame &&
                refreshes < auto_p1_a_frame + NG_LIVE_AUTO_BUTTON_HOLD_REFRESHES;
            live_input_set_active_low(&live_input.p1_buttons,
                                      NG_LIVE_P1_A,
                                      active);
        }
        if (auto_coin_frame != NG_LIVE_INPUT_NO_AUTO_FRAME ||
            auto_start_frame != NG_LIVE_INPUT_NO_AUTO_FRAME ||
            auto_p1_a_frame != NG_LIVE_INPUT_NO_AUTO_FRAME) {
            live_input_apply(&live_input);
        }

        uint64_t perf_start = SDL_GetPerformanceCounter();
        int should_advance = (!paused || step_requested) && hold_phase == 0u;
        if (should_advance) {
            int ok;
            if (present_mode == NG_LIVE_PRESENT_VIDEO) {
                ok = run_video_synced_slice(dispatches_per_refresh,
                                            video_settle_dispatches,
                                            initial_entry);
            } else if (present_mode == NG_LIVE_PRESENT_FRAME) {
                ok = run_frame_synced_slice(dispatches_per_refresh, initial_entry);
            } else {
                ok = run_dispatch_slice(dispatches_per_refresh, initial_entry);
            }
            if (!ok) {
                quit = 1;
            }
            live_audio_sync_to_runtime(&live_audio);
            step_requested = 0;
        }
        uint64_t perf_after_cpu = SDL_GetPerformanceCounter();
        if (!render_live_frame(&image,
                               zoom_rom,
                               zoom_rom_size,
                               palette_bank_mode,
                               pixels,
                               width,
                               height)) {
            quit = 1;
        }
        uint64_t perf_after_render = SDL_GetPerformanceCounter();

        SDL_UpdateTexture(texture, NULL, pixels, (int)(width * sizeof(uint32_t)));
        SDL_RenderClear(renderer);
        SDL_RenderCopy(renderer, texture, NULL, NULL);
        SDL_RenderPresent(renderer);
        uint64_t perf_after_sdl = SDL_GetPerformanceCounter();
        perf_window.cpu_ticks += live_perf_delta(perf_start, perf_after_cpu);
        perf_window.render_ticks += live_perf_delta(perf_after_cpu, perf_after_render);
        perf_window.sdl_ticks += live_perf_delta(perf_after_render, perf_after_sdl);
        ++perf_window.refreshes;

        uint64_t presented_refreshes = refreshes + 1u;
        uint64_t fps_now = SDL_GetPerformanceCounter();
        int title_due = refreshes == 0u || (refreshes % 30u) == 0u;
        if (perf_frequency != 0u &&
            fps_now >= fps_last_tick &&
            fps_now - fps_last_tick >= perf_frequency / 2u) {
            double elapsed =
                (double)(fps_now - fps_last_tick) / (double)perf_frequency;
            uint32_t frame_now = ng_neogeo_frame_count();
            if (elapsed > 0.0) {
                present_fps =
                    (double)(presented_refreshes - fps_last_refreshes) /
                    elapsed;
                emulated_fps =
                    (double)(frame_now - fps_last_frame) / elapsed;
            }
            perf_cpu_ms = live_perf_average_ms(perf_window.cpu_ticks,
                                               perf_window.refreshes,
                                               perf_frequency);
            perf_render_ms = live_perf_average_ms(perf_window.render_ticks,
                                                  perf_window.refreshes,
                                                  perf_frequency);
            perf_sdl_ms = live_perf_average_ms(perf_window.sdl_ticks,
                                               perf_window.refreshes,
                                               perf_frequency);
            perf_throttle_ms = live_perf_average_ms(perf_window.throttle_ticks,
                                                    perf_window.refreshes,
                                                    perf_frequency);
            if (perf_log) {
                fprintf(stderr,
                        "live perf refresh=%llu fps=%.1f emu=%.1f "
                        "cpu=%.3fms render=%.3fms sdl=%.3fms wait=%.3fms\n",
                        (unsigned long long)presented_refreshes,
                        present_fps,
                        emulated_fps,
                        perf_cpu_ms,
                        perf_render_ms,
                        perf_sdl_ms,
                        perf_throttle_ms);
            }
            live_perf_reset(&perf_window);
            fps_last_tick = fps_now;
            fps_last_refreshes = presented_refreshes;
            fps_last_frame = frame_now;
            title_due = 1;
        }

        if (status_interval != 0u && refreshes != 0u &&
            (refreshes % status_interval) == 0u) {
            print_live_status("live status", refreshes, dispatches_per_refresh);
        }
        if (diagnostics_interval != 0u && refreshes != 0u &&
            (refreshes % diagnostics_interval) == 0u) {
            print_live_diagnostics("live diagnostics",
                                   refreshes,
                                   dispatches_per_refresh,
                                   palette_bank_mode,
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
                                       palette_bank_mode,
                                       pixels,
                                       width,
                                       height);
                stall_reported = 1;
            }
        }

        if (title_due) {
            char title[256];
            snprintf(title,
                     sizeof(title),
                     "neo-recomp live - %.1f fps / %.1f emu - cpu %.1f render %.1f sdl %.1f ms - dispatches=%llu frame=%u scanline=%u cap=%llu hold=%llu %s%s",
                     present_fps,
                     emulated_fps,
                     perf_cpu_ms,
                     perf_render_ms,
                     perf_sdl_ms,
                     (unsigned long long)ng_generated_smoke_dispatch_count(),
                     ng_neogeo_frame_count(),
                     ng_neogeo_current_scanline(),
                     (unsigned long long)dispatches_per_refresh,
                     (unsigned long long)frame_hold,
                     present_mode_name(present_mode),
                     paused ? " paused" : "");
            SDL_SetWindowTitle(window, title);
        }

        ++refreshes;
        if (!paused && frame_hold > 1u) {
            hold_phase = (hold_phase + 1u) % frame_hold;
        } else if (paused) {
            hold_phase = 0;
        }
        if (max_refreshes != 0u && refreshes >= max_refreshes) {
            quit = 1;
        }
        live_audio_flush_output(&live_audio);
        uint64_t perf_before_throttle = SDL_GetPerformanceCounter();
        throttle_to_next_neo_frame(&next_present_tick,
                                   perf_frequency,
                                   neo_frame_ticks,
                                   no_throttle != 0u);
        uint64_t perf_after_throttle = SDL_GetPerformanceCounter();
        perf_window.throttle_ticks += live_perf_delta(perf_before_throttle,
                                                      perf_after_throttle);
    }

    uint32_t audio_report_sample_rate = live_audio.device_open && live_audio.spec.freq > 0 ?
        (uint32_t)live_audio.spec.freq : NG_LIVE_AUDIO_SAMPLE_RATE;
    uint32_t audio_queue_max_ms = audio_report_sample_rate != 0u ?
        (uint32_t)(((uint64_t)live_audio.max_queued_audio_bytes * 1000u) /
                   ((uint64_t)audio_report_sample_rate * 2u * sizeof(int16_t))) :
        0u;
    fprintf(stderr,
            "live host stopped: dispatches=%llu frame=%u scanline=%u budget_stop=$%06X "
            "audio_frames=%llu audio_nonzero=%llu audio_peak=%d "
            "audio_qmax_ms=%u audio_qwait_ms=%llu audio_qclears=%u "
            "sound_cmds=%llu nmi=%u cmd_ack=%u cmd_read=%u cmd_clear=%u last_sound=$%02X latch=$%02X reply=$%02X "
            "ym_writes=%u ym_reads=%u\n",
            (unsigned long long)ng_generated_smoke_dispatch_count(),
            ng_neogeo_frame_count(),
            ng_neogeo_current_scanline(),
            ng_generated_smoke_dispatch_budget_stop_addr() & 0x00FFFFFFu,
            (unsigned long long)live_audio.generated_audio_frames,
            (unsigned long long)live_audio.nonzero_audio_samples,
            (int)live_audio.peak_audio_sample,
            audio_queue_max_ms,
            (unsigned long long)live_audio.queue_wait_ms,
            live_audio.queue_clears,
            (unsigned long long)live_audio.sound_commands,
            ng_neogeo_audio_nmi_service_count(live_audio.audio),
            ng_neogeo_audio_command_ack_count(live_audio.audio),
            ng_neogeo_audio_command_read_count(live_audio.audio),
            ng_neogeo_audio_command_clear_count(live_audio.audio),
            live_audio.last_sound_command,
            ng_neogeo_audio_command_latch(live_audio.audio),
            ng_neogeo_audio_reply_latch(live_audio.audio),
            ng_neogeo_audio_ym_write_count(live_audio.audio),
            ng_neogeo_audio_ym_read_count(live_audio.audio));
    if (live_audio.sound_commands != 0u) {
        fprintf(stderr, "live audio recent commands:");
        uint32_t count = live_audio.sound_commands < 16u ?
            (uint32_t)live_audio.sound_commands : 16u;
        uint32_t start = live_audio.recent_sound_command_head - count;
        for (uint32_t i = 0; i < count; ++i) {
            fprintf(stderr,
                    " $%02X",
                    live_audio.recent_sound_commands[(start + i) & 15u]);
        }
        fprintf(stderr, "\n");
    }
    if (live_audio.audio) {
        NgNeoAudioAdpcmAEvent adpcm_a =
            ng_neogeo_audio_last_adpcm_a_event(live_audio.audio);
        if (adpcm_a.keyon_count != 0u || adpcm_a.keyoff_count != 0u) {
            fprintf(stderr,
                    "live audio ADPCM-A: keyons=%u keyoffs=%u last_ch=%u "
                    "start=$%06X end=$%06X level=$%02X total=$%02X pan=%u%u\n",
                    adpcm_a.keyon_count,
                    adpcm_a.keyoff_count,
                    adpcm_a.channel,
                    adpcm_a.start_addr & 0x00FFFFFFu,
                    adpcm_a.end_addr & 0x00FFFFFFu,
                    adpcm_a.level,
                    adpcm_a.total_level,
                    adpcm_a.pan_left,
                    adpcm_a.pan_right);
        }
        NgNeoAudioAdpcmBEvent adpcm_b =
            ng_neogeo_audio_last_adpcm_b_event(live_audio.audio);
        if (adpcm_b.keyon_count != 0u || adpcm_b.reset_count != 0u) {
            fprintf(stderr,
                    "live audio ADPCM-B: keyons=%u resets=%u "
                    "start=$%06X end=$%06X delta=$%04X level=$%02X "
                    "ctrl=$%02X pan=%u%u repeat=%u spoff=%u\n",
                    adpcm_b.keyon_count,
                    adpcm_b.reset_count,
                    adpcm_b.start_addr & 0x00FFFFFFu,
                    adpcm_b.end_addr & 0x00FFFFFFu,
                    adpcm_b.delta_n,
                    adpcm_b.level,
                    adpcm_b.control,
                    adpcm_b.pan_left,
                    adpcm_b.pan_right,
                    adpcm_b.repeat,
                    adpcm_b.speaker_off);
        }

        NgNeoAudioYmWrite recent_writes[16];
        uint32_t write_count =
            ng_neogeo_audio_copy_recent_ym_writes(live_audio.audio,
                                                  recent_writes,
                                                  16u);
        if (write_count != 0u) {
            fprintf(stderr, "live audio recent YM writes:");
            for (uint32_t i = 0; i < write_count; ++i) {
                fprintf(stderr,
                        " p%u[$%02X]=$%02X",
                        recent_writes[i].port,
                        recent_writes[i].address,
                        recent_writes[i].data);
            }
            fprintf(stderr, "\n");
        }
    }

    int dump_ok = dump_live_state(dump_state_dir);

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    live_audio_close(&live_audio);
    SDL_Quit();
    free(pixels);
    free(zoom_rom);
    free(bios_data);
    ng_neo_rom_image_free(&image);
    return dump_ok ? 0 : 1;
}
