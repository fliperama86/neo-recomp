#include "ngrecomp/neogeo_video.h"
#include "p_rom.h"

#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NG_RENDER_WORK_RAM_BYTES 0x10000u
#define NG_RENDER_PALETTE_BYTES 0x4000u
#define NG_RENDER_PALETTE_WORDS (NG_RENDER_PALETTE_BYTES / 2u)
#define NG_RENDER_VRAM_BYTES 0x20000u
#define NG_RENDER_VRAM_WORDS (NG_RENDER_VRAM_BYTES / 2u)

typedef enum NgRenderMode {
    NG_RENDER_MODE_FIX = 0,
    NG_RENDER_MODE_SPRITE_ATLAS = 1,
    NG_RENDER_MODE_SPRITES = 2,
    NG_RENDER_MODE_FRAME = 3
} NgRenderMode;

#define NG_RENDER_PALETTE_BANK_AUTO UINT32_MAX

static int make_path(char *out, size_t out_size, const char *dir, const char *name) {
    size_t len = strlen(dir);
    const char *sep = len != 0u && (dir[len - 1u] == '/' || dir[len - 1u] == '\\') ? "" : "/";
    int written = snprintf(out, out_size, "%s%s%s", dir, sep, name);
    return written >= 0 && (size_t)written < out_size;
}

static int read_exact(const char *path, uint8_t *out, size_t expected_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
        return 0;
    }
    size_t got = fread(out, 1, expected_size, f);
    int extra = fgetc(f);
    if (fclose(f) != 0) {
        fprintf(stderr, "failed to close %s: %s\n", path, strerror(errno));
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
            fprintf(stderr, "failed to read %s: %s\n", path, strerror(errno));
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

static uint16_t read_be16(const uint8_t *data, uint32_t word_index) {
    uint32_t offset = word_index * 2u;
    return (uint16_t)(((uint16_t)data[offset] << 8) | (uint16_t)data[offset + 1u]);
}

static int load_snapshot_words(const char *snapshot_dir,
                               uint16_t *out_palette_words,
                               uint16_t *out_vram_words) {
    char path[1200];
    uint8_t *palette_be = (uint8_t *)malloc(NG_RENDER_PALETTE_BYTES);
    uint8_t *vram_be = (uint8_t *)malloc(NG_RENDER_VRAM_BYTES);
    if (!palette_be || !vram_be) {
        fprintf(stderr, "failed to allocate snapshot buffers\n");
        free(palette_be);
        free(vram_be);
        return 0;
    }

    int ok = make_path(path, sizeof(path), snapshot_dir, "palette_ram.bin") &&
             read_exact(path, palette_be, NG_RENDER_PALETTE_BYTES) &&
             make_path(path, sizeof(path), snapshot_dir, "vram_be.bin") &&
             read_exact(path, vram_be, NG_RENDER_VRAM_BYTES);
    if (ok) {
        for (uint32_t i = 0; i < NG_RENDER_PALETTE_WORDS; ++i) {
            out_palette_words[i] = read_be16(palette_be, i);
        }
        for (uint32_t i = 0; i < NG_RENDER_VRAM_WORDS; ++i) {
            out_vram_words[i] = read_be16(vram_be, i);
        }
    }

    free(palette_be);
    free(vram_be);
    return ok;
}

static int write_ppm_argb(const char *path,
                          const uint32_t *pixels,
                          uint32_t width,
                          uint32_t height,
                          uint32_t stride_pixels) {
    FILE *f = fopen(path, "wb");
    if (!f) {
        fprintf(stderr, "failed to open %s: %s\n", path, strerror(errno));
        return 0;
    }
    if (fprintf(f, "P6\n%u %u\n255\n", width, height) < 0) {
        fclose(f);
        return 0;
    }
    for (uint32_t y = 0; y < height; ++y) {
        const uint32_t *row = pixels + y * stride_pixels;
        for (uint32_t x = 0; x < width; ++x) {
            uint32_t argb = row[x];
            uint8_t rgb[3] = {
                (uint8_t)(argb >> 16),
                (uint8_t)(argb >> 8),
                (uint8_t)argb,
            };
            if (fwrite(rgb, 1, sizeof(rgb), f) != sizeof(rgb)) {
                fprintf(stderr, "failed to write %s: %s\n", path, strerror(errno));
                fclose(f);
                return 0;
            }
        }
    }
    if (fclose(f) != 0) {
        fprintf(stderr, "failed to close %s: %s\n", path, strerror(errno));
        return 0;
    }
    return 1;
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s [options] <snapshot-dir> <game.neo> <out.ppm>\n"
            "\n"
            "Options:\n"
            "  --mode fix|sprite-atlas|sprites|frame\n"
            "                               render mode (default: fix)\n"
            "  --palette-bank auto|0|1      palette RAM bank (default: auto)\n"
            "  --debug-palette              false-color tile pixels instead of\n"
            "                               using snapshot palette RAM\n"
            "  --sprite-base-word <addr>    atlas VRAM word base (default: 0)\n"
            "  --sprite-cols <n>            atlas columns (default: 32)\n"
            "  --sprite-rows <n>            atlas rows (default: 32)\n"
            "  --lo-rom <path>              optional 000-lo.lo/ng-lo.rom vertical\n"
            "                               zoom ROM (auto-detects next to .neo)\n"
            "\n"
            "fix renders the CPU-visible 40x32 fix tile map from the snapshot\n"
            "and cartridge S region. sprite-atlas renders slow-VRAM sprite-map\n"
            "entries as a diagnostic atlas using the cartridge C region. sprites\n"
            "renders a first-pass positioned sprite frame from SCB1-SCB4. frame\n"
            "renders sprites plus the visible fix layer; vertical shrink falls\n"
            "back to an approximation without LO ROM, and line-buffer priority\n"
            "is not exact yet.\n",
            argv0);
}

static uint16_t palette_word_from_rgb5(uint8_t r5, uint8_t g5, uint8_t b5) {
    uint16_t word = 0;
    r5 &= 0x1Fu;
    g5 &= 0x1Fu;
    b5 &= 0x1Fu;
    word |= (uint16_t)((r5 & 0x1Eu) << 7);
    word |= (uint16_t)((r5 & 0x01u) << 14);
    word |= (uint16_t)((g5 & 0x1Eu) << 3);
    word |= (uint16_t)((g5 & 0x01u) << 13);
    word |= (uint16_t)((b5 & 0x1Eu) >> 1);
    word |= (uint16_t)((b5 & 0x01u) << 12);
    return word;
}

static uint16_t debug_palette_word(uint32_t palette, uint32_t color) {
    static const uint8_t base_rgb5[16][3] = {
        {0, 0, 0},     {31, 31, 31}, {31, 0, 0},   {0, 31, 0},
        {0, 0, 31},    {31, 31, 0},  {31, 0, 31},  {0, 31, 31},
        {18, 18, 18},  {31, 16, 0},  {16, 0, 31},  {0, 16, 31},
        {16, 31, 0},   {31, 0, 16},  {0, 31, 16},  {24, 24, 24},
    };

    color &= 0x0Fu;
    if (color == 0u) {
        return 0;
    }

    uint8_t r = (uint8_t)(base_rgb5[color][0] + ((palette * 5u) & 0x0Fu));
    uint8_t g = (uint8_t)(base_rgb5[color][1] + ((palette * 3u) & 0x0Fu));
    uint8_t b = (uint8_t)(base_rgb5[color][2] + ((palette * 7u) & 0x0Fu));
    if (r > 31u) r = 31u;
    if (g > 31u) g = 31u;
    if (b > 31u) b = 31u;
    return palette_word_from_rgb5(r, g, b);
}

static void fill_debug_palette(uint16_t *palette_words) {
    for (uint32_t palette = 0; palette < 256u; ++palette) {
        for (uint32_t color = 0; color < 16u; ++color) {
            palette_words[palette * 16u + color] =
                debug_palette_word(palette, color);
        }
    }
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

static int parse_summary_u32(const char *line, const char *key, uint32_t *out) {
    size_t key_len = strlen(key);
    if (strncmp(line, key, key_len) != 0 || line[key_len] != '=') {
        return 0;
    }
    const char *value = line + key_len + 1u;
    int base = 10;
    if (*value == '$') {
        ++value;
        base = 16;
    }
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, base);
    if (end == value || parsed > UINT32_MAX) {
        return 0;
    }
    *out = (uint32_t)parsed;
    return 1;
}

static void load_snapshot_render_state(const char *snapshot_dir,
                                       uint32_t *out_frame_count,
                                       uint16_t *out_lspc) {
    char path[1200];
    char line[256];
    uint32_t frame_count = 0;
    uint32_t lspc = 0;
    if (!make_path(path, sizeof(path), snapshot_dir, "summary.txt")) {
        *out_frame_count = frame_count;
        *out_lspc = (uint16_t)lspc;
        return;
    }

    FILE *f = fopen(path, "r");
    if (!f) {
        *out_frame_count = frame_count;
        *out_lspc = (uint16_t)lspc;
        return;
    }
    while (fgets(line, sizeof(line), f)) {
        uint32_t value = 0;
        if (parse_summary_u32(line, "frame", &value)) {
            frame_count = value;
        } else if (parse_summary_u32(line, "lspc", &value)) {
            lspc = value & 0xFFFFu;
        }
    }
    fclose(f);

    *out_frame_count = frame_count;
    *out_lspc = (uint16_t)lspc;
}

static uint8_t snapshot_auto_animation_counter(uint32_t frame_count, uint16_t lspc) {
    uint32_t speed = (uint32_t)(lspc >> 8);
    return (uint8_t)((frame_count + speed) / (speed + 1u));
}

static int parse_u32_option(const char *text, uint32_t max_value, uint32_t *out) {
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 0);
    if (!end || *end != '\0' || parsed > (unsigned long)max_value) {
        return 0;
    }
    *out = (uint32_t)parsed;
    return 1;
}

int main(int argc, char **argv) {
    NgRenderMode mode = NG_RENDER_MODE_FIX;
    uint32_t palette_bank = NG_RENDER_PALETTE_BANK_AUTO;
    int debug_palette = 0;
    uint32_t sprite_base_word = 0;
    uint32_t sprite_cols = 32;
    uint32_t sprite_rows = 32;
    const char *lo_rom_path = NULL;
    int argi = 1;

    while (argi < argc && strncmp(argv[argi], "--", 2) == 0) {
        const char *option = argv[argi++];
        if (strcmp(option, "--mode") == 0) {
            if (argc <= argi) {
                usage(argv[0]);
                return 2;
            }
            if (strcmp(argv[argi], "fix") == 0) {
                mode = NG_RENDER_MODE_FIX;
            } else if (strcmp(argv[argi], "sprite-atlas") == 0) {
                mode = NG_RENDER_MODE_SPRITE_ATLAS;
            } else if (strcmp(argv[argi], "sprites") == 0) {
                mode = NG_RENDER_MODE_SPRITES;
            } else if (strcmp(argv[argi], "frame") == 0) {
                mode = NG_RENDER_MODE_FRAME;
            } else {
                fprintf(stderr, "mode must be fix, sprite-atlas, sprites, or frame\n");
                return 2;
            }
            ++argi;
        } else if (strcmp(option, "--palette-bank") == 0) {
            if (argc <= argi) {
                fprintf(stderr, "palette bank must be auto, 0, or 1\n");
                return 2;
            }
            if (strcmp(argv[argi], "auto") == 0) {
                palette_bank = NG_RENDER_PALETTE_BANK_AUTO;
            } else if (!parse_u32_option(argv[argi], 1u, &palette_bank)) {
                fprintf(stderr, "palette bank must be auto, 0, or 1\n");
                return 2;
            }
            ++argi;
        } else if (strcmp(option, "--debug-palette") == 0) {
            debug_palette = 1;
        } else if (strcmp(option, "--lo-rom") == 0) {
            if (argc <= argi) {
                fprintf(stderr, "LO ROM option requires a path\n");
                return 2;
            }
            lo_rom_path = argv[argi++];
        } else if (strcmp(option, "--sprite-base-word") == 0) {
            if (argc <= argi ||
                !parse_u32_option(argv[argi], NG_RENDER_VRAM_WORDS - 1u, &sprite_base_word)) {
                fprintf(stderr, "sprite base word must be a VRAM word address\n");
                return 2;
            }
            ++argi;
        } else if (strcmp(option, "--sprite-cols") == 0) {
            if (argc <= argi ||
                !parse_u32_option(argv[argi], 256u, &sprite_cols) ||
                sprite_cols == 0u) {
                fprintf(stderr, "sprite cols must be 1..256\n");
                return 2;
            }
            ++argi;
        } else if (strcmp(option, "--sprite-rows") == 0) {
            if (argc <= argi ||
                !parse_u32_option(argv[argi], 256u, &sprite_rows) ||
                sprite_rows == 0u) {
                fprintf(stderr, "sprite rows must be 1..256\n");
                return 2;
            }
            ++argi;
        } else {
            fprintf(stderr, "unknown option: %s\n", option);
            usage(argv[0]);
            return 2;
        }
    }

    if (argc - argi != 3) {
        usage(argv[0]);
        return 2;
    }

    const char *snapshot_dir = argv[argi];
    const char *neo_path = argv[argi + 1];
    const char *out_path = argv[argi + 2];

    uint32_t out_width = NG_NEO_FIX_FRAME_WIDTH;
    uint32_t out_height = NG_NEO_FIX_FRAME_HEIGHT;
    if (mode == NG_RENDER_MODE_SPRITE_ATLAS) {
        if (sprite_cols > UINT32_MAX / NG_NEO_SPRITE_TILE_PIXELS ||
            sprite_rows > UINT32_MAX / NG_NEO_SPRITE_TILE_PIXELS) {
            fprintf(stderr, "sprite atlas dimensions are too large\n");
            return 2;
        }
        out_width = sprite_cols * NG_NEO_SPRITE_TILE_PIXELS;
        out_height = sprite_rows * NG_NEO_SPRITE_TILE_PIXELS;
    } else if (mode == NG_RENDER_MODE_SPRITES ||
               mode == NG_RENDER_MODE_FRAME) {
        out_width = NG_NEO_SPRITE_FRAME_WIDTH;
        out_height = NG_NEO_SPRITE_FRAME_HEIGHT;
    }
    if (out_width == 0u || out_height == 0u ||
        (size_t)out_width > SIZE_MAX / (size_t)out_height ||
        (size_t)out_width * (size_t)out_height > SIZE_MAX / sizeof(uint32_t)) {
        fprintf(stderr, "render dimensions are too large\n");
        return 2;
    }

    uint16_t *palette_words = (uint16_t *)calloc(NG_RENDER_PALETTE_WORDS, sizeof(uint16_t));
    uint16_t *vram_words = (uint16_t *)calloc(NG_RENDER_VRAM_WORDS, sizeof(uint16_t));
    uint32_t *pixels = (uint32_t *)calloc((size_t)out_width * (size_t)out_height,
                                          sizeof(uint32_t));
    if (!palette_words || !vram_words || !pixels) {
        fprintf(stderr, "failed to allocate render buffers\n");
        free(palette_words);
        free(vram_words);
        free(pixels);
        return 1;
    }

    NgNeoRomImage image;
    memset(&image, 0, sizeof(image));
    uint8_t *zoom_rom = NULL;
    uint32_t zoom_rom_size = 0;
    uint32_t frame_count = 0;
    uint16_t lspc = 0;
    int ok = load_snapshot_words(snapshot_dir, palette_words, vram_words) &&
             ng_neo_rom_image_load(&image, neo_path);
    load_snapshot_render_state(snapshot_dir, &frame_count, &lspc);
    if (ok) {
        if (lo_rom_path) {
            ok = load_zoom_rom_file(lo_rom_path, &zoom_rom, &zoom_rom_size, 1);
        } else if (mode == NG_RENDER_MODE_SPRITES || mode == NG_RENDER_MODE_FRAME) {
            (void)try_load_default_zoom_rom(neo_path, &zoom_rom, &zoom_rom_size);
        }
    }
    if (ok) {
        if (debug_palette) {
            fill_debug_palette(palette_words);
            fill_debug_palette(palette_words + NG_NEO_PALETTE_COLORS_PER_BANK);
        }
        if (palette_bank == NG_RENDER_PALETTE_BANK_AUTO) {
            palette_bank = debug_palette ? 0u : choose_palette_bank(palette_words);
        }
        uint32_t palette_word_offset = palette_bank * NG_NEO_PALETTE_COLORS_PER_BANK;
        NgNeoVideoRenderOptions render_options;
        memset(&render_options, 0, sizeof(render_options));
        render_options.zoom_rom = zoom_rom;
        render_options.zoom_rom_size = zoom_rom_size;
        render_options.auto_animation_counter =
            snapshot_auto_animation_counter(frame_count, lspc);
        render_options.auto_animation_disabled = (uint8_t)((lspc >> 3) & 1u);
        if (mode == NG_RENDER_MODE_FIX) {
            ok = ng_neogeo_video_render_fix_layer_argb(
                image.s.data,
                image.s.size,
                vram_words,
                NG_RENDER_VRAM_WORDS,
                palette_words + palette_word_offset,
                NG_NEO_PALETTE_COLORS_PER_BANK,
                pixels,
                out_width,
                out_height,
                out_width);
        } else if (mode == NG_RENDER_MODE_SPRITE_ATLAS) {
            ok = ng_neogeo_video_render_sprite_map_atlas_argb(
                image.c.data,
                image.c.size,
                vram_words,
                NG_RENDER_VRAM_WORDS,
                sprite_base_word,
                palette_words + palette_word_offset,
                NG_NEO_PALETTE_COLORS_PER_BANK,
                pixels,
                sprite_cols,
                sprite_rows,
                out_width,
                out_height,
                out_width);
        } else if (mode == NG_RENDER_MODE_SPRITES) {
            ok = ng_neogeo_video_render_sprite_frame_argb_with_options(
                image.c.data,
                image.c.size,
                &render_options,
                vram_words,
                NG_RENDER_VRAM_WORDS,
                palette_words + palette_word_offset,
                NG_NEO_PALETTE_COLORS_PER_BANK,
                pixels,
                out_width,
                out_height,
                out_width);
        } else {
            ok = ng_neogeo_video_render_frame_argb_with_options(
                image.s.data,
                image.s.size,
                image.c.data,
                image.c.size,
                &render_options,
                vram_words,
                NG_RENDER_VRAM_WORDS,
                palette_words + palette_word_offset,
                NG_NEO_PALETTE_COLORS_PER_BANK,
                pixels,
                out_width,
                out_height,
                out_width);
        }
        ok = ok && write_ppm_argb(out_path,
                                  pixels,
                                  out_width,
                                  out_height,
                                  out_width);
    }

    ng_neo_rom_image_free(&image);
    free(zoom_rom);
    free(palette_words);
    free(vram_words);
    free(pixels);
    return ok ? 0 : 1;
}
