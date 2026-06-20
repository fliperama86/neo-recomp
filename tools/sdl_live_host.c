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
#define NG_LIVE_DEFAULT_DISPATCHES_PER_REFRESH 2000u
#define NG_LIVE_DEFAULT_FAST_FORWARD 0u
#define NG_LIVE_DEFAULT_SCALE 3u

void ng_generated_call(uint32_t addr);
void ng_generated_smoke_reset_dispatch_stats(void);
void ng_generated_smoke_set_dispatch_budget(uint64_t max_dispatches);
void ng_generated_smoke_set_scanline_poll_interval(uint32_t interval);
uint64_t ng_generated_smoke_dispatch_count(void);
int ng_generated_smoke_dispatch_budget_hit(void);
uint32_t ng_generated_smoke_dispatch_budget_stop_addr(void);

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

static int parse_u64(const char *text, uint64_t *out) {
    char *end = NULL;
    unsigned long long value = strtoull(text, &end, 0);
    if (!text || !*text || !end || *end != '\0') {
        return 0;
    }
    *out = (uint64_t)value;
    return 1;
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s [options] <game.neo> <bios.rom> [lo-rom]\n"
            "\n"
            "Options:\n"
            "  --fast-forward <n>             dispatches before opening the loop (default: 0)\n"
            "  --dispatches-per-refresh <n>   dispatches per rendered refresh (default: %u)\n"
            "  --scanline-poll-interval <n>   runtime scanline poll interval (default: 64)\n"
            "  --max-refreshes <n>            exit after n presented frames\n"
            "  --status-interval <n>          log status every n presented frames\n"
            "  --stall-refreshes <n>          log if scanline/frame stalls for n refreshes (default: 180, 0 disables)\n"
            "  --no-throttle                  do not sleep to ~60Hz after each refresh\n"
            "  --scale <n>                    integer window scale (default: %u)\n",
            argv0,
            NG_LIVE_DEFAULT_DISPATCHES_PER_REFRESH,
            NG_LIVE_DEFAULT_SCALE);
}

int main(int argc, char **argv) {
    uint64_t fast_forward = NG_LIVE_DEFAULT_FAST_FORWARD;
    uint64_t dispatches_per_refresh = NG_LIVE_DEFAULT_DISPATCHES_PER_REFRESH;
    uint64_t max_refreshes = 0;
    uint64_t status_interval = 0;
    uint64_t stall_refreshes = 180u;
    uint64_t no_throttle = 0;
    uint64_t scanline_poll_interval = 64u;
    uint64_t scale = NG_LIVE_DEFAULT_SCALE;

    int argi = 1;
    while (argi < argc && strncmp(argv[argi], "--", 2) == 0) {
        const char *option = argv[argi++];
        uint64_t *target = NULL;
        if (strcmp(option, "--fast-forward") == 0) {
            target = &fast_forward;
        } else if (strcmp(option, "--dispatches-per-refresh") == 0) {
            target = &dispatches_per_refresh;
        } else if (strcmp(option, "--scanline-poll-interval") == 0) {
            target = &scanline_poll_interval;
        } else if (strcmp(option, "--max-refreshes") == 0) {
            target = &max_refreshes;
        } else if (strcmp(option, "--status-interval") == 0) {
            target = &status_interval;
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

    ng_neogeo_reset_runtime();
    ng_neogeo_set_program_rom(image.p.data, image.p.size);
    ng_neogeo_set_system_rom(bios_data, bios_size);
    ng_neogeo_set_external_dispatch(live_external_dispatch);
    ng_neogeo_set_auto_scanline_interval((uint32_t)scanline_poll_interval);
    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    g_ng_m68k.ssp = ng_program_rom_initial_ssp(&rom);
    g_ng_m68k.a[7] = g_ng_m68k.ssp;
    g_ng_m68k.sr = 0x2700u;
    ng_generated_smoke_reset_dispatch_stats();
    ng_generated_smoke_set_scanline_poll_interval((uint32_t)scanline_poll_interval);

    if (!run_fast_forward(fast_forward, cart_entry)) {
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
            "live host started: entry=$%06X dispatches=%llu frame=%u scanline=%u\n",
            cart_entry & 0x00FFFFFFu,
            (unsigned long long)ng_generated_smoke_dispatch_count(),
            ng_neogeo_frame_count(),
            ng_neogeo_current_scanline());
    fprintf(stderr, "keys: q/Escape quit, Space pause/resume, +/- adjust dispatches per refresh\n");

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

        if (!paused && !run_dispatch_slice(dispatches_per_refresh, cart_entry)) {
            quit = 1;
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
                print_live_status("live scanline stall",
                                  refreshes,
                                  dispatches_per_refresh);
                stall_reported = 1;
            }
        }

        if ((refreshes % 30u) == 0u) {
            char title[256];
            snprintf(title,
                     sizeof(title),
                     "neo-recomp live - dispatches=%llu frame=%u scanline=%u dpf=%llu%s",
                     (unsigned long long)ng_generated_smoke_dispatch_count(),
                     ng_neogeo_frame_count(),
                     ng_neogeo_current_scanline(),
                     (unsigned long long)dispatches_per_refresh,
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
