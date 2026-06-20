#include <SDL.h>

#include "ngrecomp/neogeo_video.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NG_VIEW_WORK_RAM_BYTES 0x10000u
#define NG_VIEW_PALETTE_BYTES 0x4000u
#define NG_VIEW_VRAM_BYTES 0x20000u
#define NG_VIEW_PIXELS 256u

typedef enum NgViewMode {
    NG_VIEW_VRAM_HASH = 0,
    NG_VIEW_VRAM_NONZERO = 1,
    NG_VIEW_WORK_RAM = 2,
    NG_VIEW_PALETTE = 3,
    NG_VIEW_MODE_COUNT = 4
} NgViewMode;

typedef struct NgSnapshotView {
    uint8_t work_ram[NG_VIEW_WORK_RAM_BYTES];
    uint8_t palette_ram[NG_VIEW_PALETTE_BYTES];
    uint8_t vram_be[NG_VIEW_VRAM_BYTES];
    uint32_t pixels[NG_VIEW_PIXELS * NG_VIEW_PIXELS];
    char snapshot_dir[1024];
} NgSnapshotView;

static int make_path(char *out, size_t out_size, const char *dir, const char *name) {
    size_t len = strlen(dir);
    const char *sep = len != 0u && (dir[len - 1u] == '/' || dir[len - 1u] == '\\') ? "" : "/";
    int written = snprintf(out, out_size, "%s%s%s", dir, sep, name);
    return written >= 0 && (size_t)written < out_size;
}

static int read_exact(const char *path, void *out, size_t expected_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "failed to open %s\n", path);
        return 0;
    }
    size_t got = fread(out, 1, expected_size, f);
    int extra = fgetc(f);
    if (fclose(f) != 0) {
        fprintf(stderr, "failed to close %s\n", path);
        return 0;
    }
    if (got != expected_size || extra != EOF) {
        fprintf(stderr,
                "%s has unexpected size (read %llu, expected %llu)\n",
                path,
                (unsigned long long)got,
                (unsigned long long)expected_size);
        return 0;
    }
    return 1;
}

static int load_snapshot(NgSnapshotView *view, const char *dir) {
    char path[1200];
    if (!view || !dir || !*dir) {
        return 0;
    }
    int written = snprintf(view->snapshot_dir, sizeof(view->snapshot_dir), "%s", dir);
    if (written < 0 || (size_t)written >= sizeof(view->snapshot_dir)) {
        fprintf(stderr, "snapshot directory path is too long\n");
        return 0;
    }

    if (!make_path(path, sizeof(path), dir, "work_ram.bin") ||
        !read_exact(path, view->work_ram, sizeof(view->work_ram))) {
        return 0;
    }
    if (!make_path(path, sizeof(path), dir, "palette_ram.bin") ||
        !read_exact(path, view->palette_ram, sizeof(view->palette_ram))) {
        return 0;
    }
    if (!make_path(path, sizeof(path), dir, "vram_be.bin") ||
        !read_exact(path, view->vram_be, sizeof(view->vram_be))) {
        return 0;
    }
    return 1;
}

static uint32_t argb(uint8_t r, uint8_t g, uint8_t b) {
    return 0xFF000000u | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
}

static uint8_t scale5(uint16_t value) {
    return (uint8_t)((value & 0x1Fu) * 255u / 31u);
}

static uint32_t raw_word_color(uint16_t word) {
    if (word == 0u) {
        return argb(0, 0, 0);
    }
    return argb(scale5((uint16_t)(word >> 10)),
                scale5((uint16_t)(word >> 5)),
                scale5(word));
}

static uint32_t nonzero_word_color(uint16_t word) {
    if (word == 0u) {
        return argb(0, 0, 0);
    }
    return argb((uint8_t)(128u + ((word >> 8) & 0x7Fu)),
                (uint8_t)(128u + ((word >> 4) & 0x7Fu)),
                (uint8_t)(128u + (word & 0x7Fu)));
}

static uint32_t palette_word_color(uint16_t word) {
    return ng_neogeo_video_palette_word_to_argb(word);
}

static uint16_t read_be_word(const uint8_t *data, uint32_t word_index) {
    uint32_t off = word_index * 2u;
    return (uint16_t)(((uint16_t)data[off] << 8) | (uint16_t)data[off + 1u]);
}

static void render_snapshot(NgSnapshotView *view, NgViewMode mode) {
    for (uint32_t y = 0; y < NG_VIEW_PIXELS; ++y) {
        for (uint32_t x = 0; x < NG_VIEW_PIXELS; ++x) {
            uint32_t idx = y * NG_VIEW_PIXELS + x;
            switch (mode) {
            case NG_VIEW_VRAM_HASH: {
                uint16_t word = read_be_word(view->vram_be, idx);
                view->pixels[idx] = raw_word_color(word);
                break;
            }
            case NG_VIEW_VRAM_NONZERO: {
                uint16_t word = read_be_word(view->vram_be, idx);
                view->pixels[idx] = nonzero_word_color(word);
                break;
            }
            case NG_VIEW_WORK_RAM: {
                uint8_t byte = view->work_ram[idx];
                view->pixels[idx] = argb(byte, byte, byte);
                break;
            }
            case NG_VIEW_PALETTE: {
                uint32_t pal_col = x / 2u;
                uint32_t pal_row = y / 4u;
                uint32_t word_index = pal_row * 128u + pal_col;
                uint16_t word = read_be_word(view->palette_ram, word_index);
                view->pixels[idx] = palette_word_color(word);
                break;
            }
            default:
                view->pixels[idx] = argb(0, 0, 0);
                break;
            }
        }
    }
}

static const char *mode_name(NgViewMode mode) {
    switch (mode) {
    case NG_VIEW_VRAM_HASH: return "VRAM raw word color hash";
    case NG_VIEW_VRAM_NONZERO: return "VRAM nonzero mask/tint";
    case NG_VIEW_WORK_RAM: return "Work RAM bytes";
    case NG_VIEW_PALETTE: return "Palette swatches";
    default: return "Unknown";
    }
}

static void update_title(SDL_Window *window, NgViewMode mode, const char *dir) {
    char title[1200];
    snprintf(title,
             sizeof(title),
             "neo-recomp snapshot viewer - %s - %s",
             mode_name(mode),
             dir);
    SDL_SetWindowTitle(window, title);
}

static void print_help(void) {
    printf("neo-snapshot-viewer controls:\n");
    printf("  1: VRAM raw word color hash\n");
    printf("  2: VRAM nonzero mask/tint\n");
    printf("  3: Work RAM bytes\n");
    printf("  4: Palette swatches\n");
    printf("  r: reload snapshot files\n");
    printf("  q/Escape: quit\n");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <snapshot-dir>\n", argv[0]);
        return 2;
    }

    NgSnapshotView view;
    memset(&view, 0, sizeof(view));
    if (!load_snapshot(&view, argv[1])) {
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("neo-recomp snapshot viewer",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          768,
                                          768,
                                          SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    SDL_Renderer *renderer = SDL_CreateRenderer(window,
                                                -1,
                                                SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer) {
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    }
    if (!renderer) {
        fprintf(stderr, "SDL_CreateRenderer failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    SDL_Texture *texture = SDL_CreateTexture(renderer,
                                             SDL_PIXELFORMAT_ARGB8888,
                                             SDL_TEXTUREACCESS_STREAMING,
                                             NG_VIEW_PIXELS,
                                             NG_VIEW_PIXELS);
    if (!texture) {
        fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return 1;
    }

    print_help();
    NgViewMode mode = NG_VIEW_VRAM_HASH;
    int redraw = 1;
    int running = 1;
    update_title(window, mode, view.snapshot_dir);

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                case SDLK_q:
                    running = 0;
                    break;
                case SDLK_1:
                    mode = NG_VIEW_VRAM_HASH;
                    redraw = 1;
                    break;
                case SDLK_2:
                    mode = NG_VIEW_VRAM_NONZERO;
                    redraw = 1;
                    break;
                case SDLK_3:
                    mode = NG_VIEW_WORK_RAM;
                    redraw = 1;
                    break;
                case SDLK_4:
                    mode = NG_VIEW_PALETTE;
                    redraw = 1;
                    break;
                case SDLK_r:
                    if (load_snapshot(&view, argv[1])) {
                        redraw = 1;
                    }
                    break;
                default:
                    break;
                }
            } else if (event.type == SDL_WINDOWEVENT &&
                       event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                redraw = 1;
            }
        }

        if (redraw) {
            render_snapshot(&view, mode);
            SDL_UpdateTexture(texture, NULL, view.pixels, (int)(NG_VIEW_PIXELS * sizeof(uint32_t)));
            SDL_RenderClear(renderer);
            SDL_RenderCopy(renderer, texture, NULL, NULL);
            SDL_RenderPresent(renderer);
            update_title(window, mode, view.snapshot_dir);
            redraw = 0;
        }
        SDL_Delay(10);
    }

    SDL_DestroyTexture(texture);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
