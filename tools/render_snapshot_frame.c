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
            "usage: %s [--palette-bank 0|1] <snapshot-dir> <game.neo> <out.ppm>\n"
            "\n"
            "Renders the CPU-visible 40x32 Neo Geo fix layer from a headless\n"
            "snapshot and the cartridge S region. Sprites are not rendered yet.\n",
            argv0);
}

int main(int argc, char **argv) {
    uint32_t palette_bank = 0;
    int argi = 1;
    if (argc > 1 && strcmp(argv[argi], "--palette-bank") == 0) {
        if (argc <= argi + 1) {
            usage(argv[0]);
            return 2;
        }
        char *end = NULL;
        unsigned long parsed = strtoul(argv[argi + 1], &end, 0);
        if (!end || *end != '\0' || parsed > 1ul) {
            fprintf(stderr, "palette bank must be 0 or 1\n");
            return 2;
        }
        palette_bank = (uint32_t)parsed;
        argi += 2;
    }

    if (argc - argi != 3) {
        usage(argv[0]);
        return 2;
    }

    const char *snapshot_dir = argv[argi];
    const char *neo_path = argv[argi + 1];
    const char *out_path = argv[argi + 2];

    uint16_t *palette_words = (uint16_t *)calloc(NG_RENDER_PALETTE_WORDS, sizeof(uint16_t));
    uint16_t *vram_words = (uint16_t *)calloc(NG_RENDER_VRAM_WORDS, sizeof(uint16_t));
    uint32_t *pixels = (uint32_t *)calloc(NG_NEO_FIX_FRAME_WIDTH * NG_NEO_FIX_FRAME_HEIGHT,
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
    int ok = load_snapshot_words(snapshot_dir, palette_words, vram_words) &&
             ng_neo_rom_image_load(&image, neo_path);
    if (ok) {
        uint32_t palette_word_offset = palette_bank * NG_NEO_PALETTE_COLORS_PER_BANK;
        ok = ng_neogeo_video_render_fix_layer_argb(
                 image.s.data,
                 image.s.size,
                 vram_words,
                 NG_RENDER_VRAM_WORDS,
                 palette_words + palette_word_offset,
                 NG_NEO_PALETTE_COLORS_PER_BANK,
                 pixels,
                 NG_NEO_FIX_FRAME_WIDTH,
                 NG_NEO_FIX_FRAME_HEIGHT,
                 NG_NEO_FIX_FRAME_WIDTH) &&
             write_ppm_argb(out_path,
                            pixels,
                            NG_NEO_FIX_FRAME_WIDTH,
                            NG_NEO_FIX_FRAME_HEIGHT,
                            NG_NEO_FIX_FRAME_WIDTH);
    }

    ng_neo_rom_image_free(&image);
    free(palette_words);
    free(vram_words);
    free(pixels);
    return ok ? 0 : 1;
}
