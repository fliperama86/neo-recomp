#include <SDL.h>

#include "ngrecomp/neogeo_video.h"
#include "p_rom.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NG_VIEW_WORK_RAM_BYTES 0x10000u
#define NG_VIEW_PALETTE_BYTES 0x4000u
#define NG_VIEW_PALETTE_WORDS (NG_VIEW_PALETTE_BYTES / 2u)
#define NG_VIEW_VRAM_BYTES 0x20000u
#define NG_VIEW_VRAM_WORDS (NG_VIEW_VRAM_BYTES / 2u)
#define NG_VIEW_DIAG_PIXELS 256u
#define NG_VIEW_MAX_WIDTH NG_NEO_FIX_FRAME_WIDTH
#define NG_VIEW_MAX_HEIGHT NG_NEO_FIX_FRAME_HEIGHT
#define NG_VIEW_PALETTE_BANK_AUTO UINT32_MAX

typedef enum NgViewMode {
    NG_VIEW_VRAM_HASH = 0,
    NG_VIEW_VRAM_NONZERO = 1,
    NG_VIEW_WORK_RAM = 2,
    NG_VIEW_PALETTE = 3,
    NG_VIEW_FIX_LAYER = 4,
    NG_VIEW_SPRITES = 5,
    NG_VIEW_FRAME = 6,
    NG_VIEW_MODE_COUNT = 7
} NgViewMode;

typedef struct NgSnapshotView {
    uint8_t work_ram[NG_VIEW_WORK_RAM_BYTES];
    uint8_t palette_ram[NG_VIEW_PALETTE_BYTES];
    uint8_t vram_be[NG_VIEW_VRAM_BYTES];
    uint16_t palette_words[NG_VIEW_PALETTE_WORDS];
    uint16_t vram_words[NG_VIEW_VRAM_WORDS];
    uint32_t pixels[NG_VIEW_MAX_WIDTH * NG_VIEW_MAX_HEIGHT];
    uint32_t width;
    uint32_t height;
    char snapshot_dir[1024];
    NgNeoRomImage image;
    int has_neo;
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

static uint16_t read_be_word(const uint8_t *data, uint32_t word_index) {
    uint32_t off = word_index * 2u;
    return (uint16_t)(((uint16_t)data[off] << 8) | (uint16_t)data[off + 1u]);
}

static void refresh_snapshot_words(NgSnapshotView *view) {
    for (uint32_t i = 0; i < NG_VIEW_PALETTE_WORDS; ++i) {
        view->palette_words[i] = read_be_word(view->palette_ram, i);
    }
    for (uint32_t i = 0; i < NG_VIEW_VRAM_WORDS; ++i) {
        view->vram_words[i] = read_be_word(view->vram_be, i);
    }
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
    refresh_snapshot_words(view);
    return 1;
}

static int load_neo_image(NgSnapshotView *view, const char *neo_path) {
    if (!neo_path) {
        return 1;
    }
    if (!ng_neo_rom_image_load(&view->image, neo_path)) {
        return 0;
    }
    view->has_neo = 1;
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

static uint32_t choose_palette_bank(const uint16_t *palette_words) {
    uint32_t best_bank = 0;
    uint32_t best_score = 0;
    for (uint32_t bank = 0; bank < 2u; ++bank) {
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

static uint32_t *clear_pixels(NgSnapshotView *view, uint32_t width, uint32_t height) {
    view->width = width;
    view->height = height;
    for (uint32_t y = 0; y < height; ++y) {
        for (uint32_t x = 0; x < width; ++x) {
            view->pixels[y * width + x] = argb(0, 0, 0);
        }
    }
    return view->pixels;
}

static int render_diagnostic_snapshot(NgSnapshotView *view, NgViewMode mode) {
    uint32_t *pixels = clear_pixels(view, NG_VIEW_DIAG_PIXELS, NG_VIEW_DIAG_PIXELS);
    for (uint32_t y = 0; y < NG_VIEW_DIAG_PIXELS; ++y) {
        for (uint32_t x = 0; x < NG_VIEW_DIAG_PIXELS; ++x) {
            uint32_t idx = y * NG_VIEW_DIAG_PIXELS + x;
            switch (mode) {
            case NG_VIEW_VRAM_HASH: {
                uint16_t word = view->vram_words[idx];
                pixels[idx] = raw_word_color(word);
                break;
            }
            case NG_VIEW_VRAM_NONZERO: {
                uint16_t word = view->vram_words[idx];
                pixels[idx] = nonzero_word_color(word);
                break;
            }
            case NG_VIEW_WORK_RAM: {
                uint8_t byte = view->work_ram[idx];
                pixels[idx] = argb(byte, byte, byte);
                break;
            }
            case NG_VIEW_PALETTE: {
                uint32_t pal_col = x / 2u;
                uint32_t pal_row = y / 4u;
                uint32_t word_index = pal_row * 128u + pal_col;
                uint16_t word = view->palette_words[word_index];
                pixels[idx] = palette_word_color(word);
                break;
            }
            default:
                break;
            }
        }
    }
    return 1;
}

static int render_game_snapshot(NgSnapshotView *view, NgViewMode mode) {
    if (!view->has_neo) {
        fprintf(stderr, "game-render mode requires a .neo image argument\n");
        return 0;
    }

    uint32_t out_width = NG_NEO_SPRITE_FRAME_WIDTH;
    uint32_t out_height = NG_NEO_SPRITE_FRAME_HEIGHT;
    if (mode == NG_VIEW_FIX_LAYER) {
        out_width = NG_NEO_FIX_FRAME_WIDTH;
        out_height = NG_NEO_FIX_FRAME_HEIGHT;
    }
    uint32_t *pixels = clear_pixels(view, out_width, out_height);
    uint32_t palette_bank = choose_palette_bank(view->palette_words);
    const uint16_t *palette_words =
        view->palette_words + palette_bank * NG_NEO_PALETTE_COLORS_PER_BANK;

    switch (mode) {
    case NG_VIEW_FIX_LAYER:
        return ng_neogeo_video_render_fix_layer_argb(view->image.s.data,
                                                     view->image.s.size,
                                                     view->vram_words,
                                                     NG_VIEW_VRAM_WORDS,
                                                     palette_words,
                                                     NG_NEO_PALETTE_COLORS_PER_BANK,
                                                     pixels,
                                                     out_width,
                                                     out_height,
                                                     out_width);
    case NG_VIEW_SPRITES:
        return ng_neogeo_video_render_sprite_frame_argb(view->image.c.data,
                                                        view->image.c.size,
                                                        view->vram_words,
                                                        NG_VIEW_VRAM_WORDS,
                                                        palette_words,
                                                        NG_NEO_PALETTE_COLORS_PER_BANK,
                                                        pixels,
                                                        out_width,
                                                        out_height,
                                                        out_width);
    case NG_VIEW_FRAME:
        return ng_neogeo_video_render_frame_argb(view->image.s.data,
                                                 view->image.s.size,
                                                 view->image.c.data,
                                                 view->image.c.size,
                                                 view->vram_words,
                                                 NG_VIEW_VRAM_WORDS,
                                                 palette_words,
                                                 NG_NEO_PALETTE_COLORS_PER_BANK,
                                                 pixels,
                                                 out_width,
                                                 out_height,
                                                 out_width);
    default:
        return 0;
    }
}

static int render_snapshot(NgSnapshotView *view, NgViewMode mode) {
    if (mode <= NG_VIEW_PALETTE) {
        return render_diagnostic_snapshot(view, mode);
    }
    return render_game_snapshot(view, mode);
}

static const char *mode_name(NgViewMode mode) {
    switch (mode) {
    case NG_VIEW_VRAM_HASH: return "VRAM raw word color hash";
    case NG_VIEW_VRAM_NONZERO: return "VRAM nonzero mask/tint";
    case NG_VIEW_WORK_RAM: return "Work RAM bytes";
    case NG_VIEW_PALETTE: return "Palette swatches";
    case NG_VIEW_FIX_LAYER: return "Fix layer render";
    case NG_VIEW_SPRITES: return "Positioned sprites render";
    case NG_VIEW_FRAME: return "Sprite+fix game frame";
    default: return "Unknown";
    }
}

static void update_title(SDL_Window *window, NgViewMode mode, const NgSnapshotView *view) {
    char title[1200];
    snprintf(title,
             sizeof(title),
             "neo-recomp snapshot viewer - %s - %ux%u - %s",
             mode_name(mode),
             view->width,
             view->height,
             view->snapshot_dir);
    SDL_SetWindowTitle(window, title);
}

static void print_help(int has_neo) {
    printf("neo-snapshot-viewer controls:\n");
    printf("  1: VRAM raw word color hash\n");
    printf("  2: VRAM nonzero mask/tint\n");
    printf("  3: Work RAM bytes\n");
    printf("  4: Palette swatches\n");
    printf("  5: fix-layer render%s\n", has_neo ? "" : " (.neo required)");
    printf("  6: positioned sprite render%s\n", has_neo ? "" : " (.neo required)");
    printf("  7: sprite+fix game frame%s\n", has_neo ? "" : " (.neo required)");
    printf("  r: reload snapshot files\n");
    printf("  q/Escape: quit\n");
}

static SDL_Texture *create_texture(SDL_Renderer *renderer,
                                   uint32_t width,
                                   uint32_t height) {
    return SDL_CreateTexture(renderer,
                             SDL_PIXELFORMAT_ARGB8888,
                             SDL_TEXTUREACCESS_STREAMING,
                             (int)width,
                             (int)height);
}

static int update_texture_for_view(SDL_Renderer *renderer,
                                   SDL_Texture **texture,
                                   uint32_t *texture_width,
                                   uint32_t *texture_height,
                                   const NgSnapshotView *view) {
    if (!*texture || *texture_width != view->width || *texture_height != view->height) {
        if (*texture) {
            SDL_DestroyTexture(*texture);
            *texture = NULL;
        }
        *texture = create_texture(renderer, view->width, view->height);
        if (!*texture) {
            fprintf(stderr, "SDL_CreateTexture failed: %s\n", SDL_GetError());
            return 0;
        }
        *texture_width = view->width;
        *texture_height = view->height;
    }

    if (SDL_UpdateTexture(*texture,
                          NULL,
                          view->pixels,
                          (int)(view->width * sizeof(uint32_t))) != 0) {
        fprintf(stderr, "SDL_UpdateTexture failed: %s\n", SDL_GetError());
        return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    if (argc != 2 && argc != 3) {
        fprintf(stderr, "usage: %s <snapshot-dir> [game.neo]\n", argv[0]);
        return 2;
    }

    NgSnapshotView view;
    memset(&view, 0, sizeof(view));
    if (!load_snapshot(&view, argv[1]) || !load_neo_image(&view, argc == 3 ? argv[2] : NULL)) {
        ng_neo_rom_image_free(&view.image);
        return 1;
    }

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL_Init failed: %s\n", SDL_GetError());
        ng_neo_rom_image_free(&view.image);
        return 1;
    }

    SDL_Window *window = SDL_CreateWindow("neo-recomp snapshot viewer",
                                          SDL_WINDOWPOS_CENTERED,
                                          SDL_WINDOWPOS_CENTERED,
                                          view.has_neo ? 960 : 768,
                                          view.has_neo ? 672 : 768,
                                          SDL_WINDOW_RESIZABLE);
    if (!window) {
        fprintf(stderr, "SDL_CreateWindow failed: %s\n", SDL_GetError());
        SDL_Quit();
        ng_neo_rom_image_free(&view.image);
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
        ng_neo_rom_image_free(&view.image);
        return 1;
    }
    SDL_RenderSetIntegerScale(renderer, SDL_FALSE);

    SDL_Texture *texture = NULL;
    uint32_t texture_width = 0;
    uint32_t texture_height = 0;

    print_help(view.has_neo);
    NgViewMode mode = view.has_neo ? NG_VIEW_FRAME : NG_VIEW_VRAM_HASH;
    int redraw = 1;
    int running = 1;

    while (running) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) {
                running = 0;
            } else if (event.type == SDL_KEYDOWN) {
                NgViewMode requested_mode = mode;
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                case SDLK_q:
                    running = 0;
                    break;
                case SDLK_1:
                    requested_mode = NG_VIEW_VRAM_HASH;
                    break;
                case SDLK_2:
                    requested_mode = NG_VIEW_VRAM_NONZERO;
                    break;
                case SDLK_3:
                    requested_mode = NG_VIEW_WORK_RAM;
                    break;
                case SDLK_4:
                    requested_mode = NG_VIEW_PALETTE;
                    break;
                case SDLK_5:
                    requested_mode = NG_VIEW_FIX_LAYER;
                    break;
                case SDLK_6:
                    requested_mode = NG_VIEW_SPRITES;
                    break;
                case SDLK_7:
                    requested_mode = NG_VIEW_FRAME;
                    break;
                case SDLK_r:
                    if (load_snapshot(&view, argv[1])) {
                        redraw = 1;
                    }
                    break;
                default:
                    break;
                }

                if (requested_mode != mode) {
                    if (requested_mode >= NG_VIEW_FIX_LAYER && !view.has_neo) {
                        fprintf(stderr, "game-render modes require a .neo image argument\n");
                    } else {
                        mode = requested_mode;
                        redraw = 1;
                    }
                }
            } else if (event.type == SDL_WINDOWEVENT &&
                       event.window.event == SDL_WINDOWEVENT_EXPOSED) {
                redraw = 1;
            }
        }

        if (redraw) {
            if (render_snapshot(&view, mode) &&
                update_texture_for_view(renderer,
                                        &texture,
                                        &texture_width,
                                        &texture_height,
                                        &view)) {
                SDL_RenderSetLogicalSize(renderer, (int)view.width, (int)view.height);
                SDL_RenderClear(renderer);
                SDL_RenderCopy(renderer, texture, NULL, NULL);
                SDL_RenderPresent(renderer);
                update_title(window, mode, &view);
            }
            redraw = 0;
        }
        SDL_Delay(10);
    }

    if (texture) {
        SDL_DestroyTexture(texture);
    }
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    ng_neo_rom_image_free(&view.image);
    return 0;
}
