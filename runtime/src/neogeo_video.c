#include "ngrecomp/neogeo_video.h"

#include <string.h>

uint8_t ng_neogeo_video_scale5(uint8_t value) {
    return (uint8_t)(((uint16_t)(value & 0x1Fu) * 255u) / 31u);
}

NgNeoRgb ng_neogeo_video_palette_word_to_rgb(uint16_t word) {
    uint8_t r5 = (uint8_t)(((word >> 7) & 0x1Eu) | ((word >> 14) & 0x01u));
    uint8_t g5 = (uint8_t)(((word >> 3) & 0x1Eu) | ((word >> 13) & 0x01u));
    uint8_t b5 = (uint8_t)(((word << 1) & 0x1Eu) | ((word >> 12) & 0x01u));
    NgNeoRgb rgb = {
        ng_neogeo_video_scale5(r5),
        ng_neogeo_video_scale5(g5),
        ng_neogeo_video_scale5(b5),
    };

    if (word & 0x8000u) {
        rgb.r = (uint8_t)(((uint16_t)rgb.r * 2u) / 3u);
        rgb.g = (uint8_t)(((uint16_t)rgb.g * 2u) / 3u);
        rgb.b = (uint8_t)(((uint16_t)rgb.b * 2u) / 3u);
    }
    return rgb;
}

uint32_t ng_neogeo_video_palette_word_to_argb(uint16_t word) {
    NgNeoRgb rgb = ng_neogeo_video_palette_word_to_rgb(word);
    return 0xFF000000u | ((uint32_t)rgb.r << 16) |
           ((uint32_t)rgb.g << 8) | (uint32_t)rgb.b;
}

void ng_neogeo_video_decode_4bpp_planar_line(uint16_t plane0,
                                             uint16_t plane1,
                                             uint16_t plane2,
                                             uint16_t plane3,
                                             uint8_t out_pixels[NG_NEO_SPRITE_TILE_PIXELS]) {
    if (!out_pixels) {
        return;
    }

    for (uint8_t x = 0; x < NG_NEO_SPRITE_TILE_PIXELS; ++x) {
        uint8_t bit = (uint8_t)(15u - x);
        out_pixels[x] = (uint8_t)((((plane0 >> bit) & 1u) << 0) |
                                  (((plane1 >> bit) & 1u) << 1) |
                                  (((plane2 >> bit) & 1u) << 2) |
                                  (((plane3 >> bit) & 1u) << 3));
    }
}

NgNeoSpriteMapEntry ng_neogeo_video_decode_sprite_map_entry(uint16_t tile_word,
                                                            uint16_t attr_word) {
    NgNeoSpriteMapEntry entry;
    entry.tile_index = ((uint32_t)((attr_word >> 4) & 0x0Fu) << 16) |
                       (uint32_t)tile_word;
    entry.palette = (uint8_t)(attr_word >> 8);
    entry.auto_animation = (uint8_t)((attr_word >> 2) & 0x03u);
    entry.vflip = (uint8_t)((attr_word >> 1) & 0x01u);
    entry.hflip = (uint8_t)(attr_word & 0x01u);
    return entry;
}

static void ng_neogeo_video_decode_sprite_chunk(const uint8_t *chunk,
                                                uint8_t *out_pixels) {
    for (uint8_t x = 0; x < 8u; ++x) {
        uint8_t bit = (uint8_t)(7u - x);
        out_pixels[x] = (uint8_t)((((uint16_t)chunk[0] >> bit) & 1u) << 3 |
                                  ((((uint16_t)chunk[1] >> bit) & 1u) << 2) |
                                  ((((uint16_t)chunk[2] >> bit) & 1u) << 1) |
                                  ((((uint16_t)chunk[3] >> bit) & 1u) << 0));
    }
}

int ng_neogeo_video_decode_sprite_tile_line(const uint8_t *c_rom,
                                            uint32_t c_rom_size,
                                            uint32_t tile_index,
                                            uint8_t y,
                                            uint8_t hflip,
                                            uint8_t out_pixels[NG_NEO_SPRITE_TILE_PIXELS]) {
    if (!c_rom || !out_pixels || y >= NG_NEO_SPRITE_TILE_PIXELS) {
        return 0;
    }

    if (tile_index > UINT32_MAX / NG_NEO_SPRITE_TILE_BYTES) {
        memset(out_pixels, 0, NG_NEO_SPRITE_TILE_PIXELS);
        return 0;
    }
    uint32_t tile_offset = tile_index * NG_NEO_SPRITE_TILE_BYTES;
    if (tile_offset > c_rom_size ||
        (c_rom_size - tile_offset) < NG_NEO_SPRITE_TILE_BYTES) {
        memset(out_pixels, 0, NG_NEO_SPRITE_TILE_PIXELS);
        return 0;
    }

    const uint8_t *line = c_rom + tile_offset +
                          (uint32_t)y * (NG_NEO_SPRITE_TILE_BYTES /
                                         NG_NEO_SPRITE_TILE_PIXELS);
    ng_neogeo_video_decode_sprite_chunk(line, out_pixels);
    ng_neogeo_video_decode_sprite_chunk(line + 4u, out_pixels + 8u);

    if (hflip) {
        for (uint8_t i = 0; i < NG_NEO_SPRITE_TILE_PIXELS / 2u; ++i) {
            uint8_t tmp = out_pixels[i];
            out_pixels[i] = out_pixels[NG_NEO_SPRITE_TILE_PIXELS - 1u - i];
            out_pixels[NG_NEO_SPRITE_TILE_PIXELS - 1u - i] = tmp;
        }
    }
    return 1;
}

int ng_neogeo_video_decode_fix_tile_line(const uint8_t *s_rom,
                                         uint32_t s_rom_size,
                                         uint16_t tile_index,
                                         uint8_t y,
                                         uint8_t out_pixels[NG_NEO_FIX_TILE_PIXELS]) {
    if (!s_rom || !out_pixels || y >= NG_NEO_FIX_TILE_PIXELS) {
        return 0;
    }

    uint32_t tile_offset = (uint32_t)tile_index * NG_NEO_FIX_TILE_BYTES;
    if (tile_offset > s_rom_size ||
        (s_rom_size - tile_offset) < NG_NEO_FIX_TILE_BYTES) {
        memset(out_pixels, 0, NG_NEO_FIX_TILE_PIXELS);
        return 0;
    }

    const uint8_t *tile = s_rom + tile_offset;
    const uint8_t pairs[4] = {
        tile[0x10u + y],
        tile[0x18u + y],
        tile[0x00u + y],
        tile[0x08u + y],
    };

    for (uint8_t pair = 0; pair < 4u; ++pair) {
        out_pixels[pair * 2u + 0u] = (uint8_t)(pairs[pair] & 0x0Fu);
        out_pixels[pair * 2u + 1u] = (uint8_t)(pairs[pair] >> 4);
    }
    return 1;
}

static uint32_t ng_neogeo_video_palette_argb(const uint16_t *palette_words,
                                             uint32_t palette_word_count,
                                             uint16_t palette_index);

int ng_neogeo_video_render_sprite_map_atlas_argb(const uint8_t *c_rom,
                                                 uint32_t c_rom_size,
                                                 const uint16_t *vram_words,
                                                 uint32_t vram_word_count,
                                                 uint32_t sprite_map_base_word,
                                                 const uint16_t *palette_words,
                                                 uint32_t palette_word_count,
                                                 uint32_t *out_argb,
                                                 uint32_t atlas_cols,
                                                 uint32_t atlas_rows,
                                                 uint32_t out_width,
                                                 uint32_t out_height,
                                                 uint32_t out_stride_pixels) {
    if (!c_rom || !vram_words || !out_argb || atlas_cols == 0u ||
        atlas_rows == 0u || out_stride_pixels < out_width) {
        return 0;
    }

    if (atlas_cols > UINT32_MAX / atlas_rows) {
        return 0;
    }
    uint32_t entry_count = atlas_cols * atlas_rows;
    if (atlas_cols > UINT32_MAX / NG_NEO_SPRITE_TILE_PIXELS ||
        atlas_rows > UINT32_MAX / NG_NEO_SPRITE_TILE_PIXELS) {
        return 0;
    }
    uint32_t needed_width = atlas_cols * NG_NEO_SPRITE_TILE_PIXELS;
    uint32_t needed_height = atlas_rows * NG_NEO_SPRITE_TILE_PIXELS;
    if (out_width < needed_width || out_height < needed_height) {
        return 0;
    }
    if (entry_count > UINT32_MAX / 2u) {
        return 0;
    }
    if (sprite_map_base_word > vram_word_count ||
        (vram_word_count - sprite_map_base_word) < entry_count * 2u) {
        return 0;
    }

    for (uint32_t y = 0; y < out_height; ++y) {
        for (uint32_t x = 0; x < out_width; ++x) {
            out_argb[y * out_stride_pixels + x] = 0xFF000000u;
        }
    }

    for (uint32_t tile = 0; tile < entry_count; ++tile) {
        uint32_t vram_addr = sprite_map_base_word + tile * 2u;
        NgNeoSpriteMapEntry entry = ng_neogeo_video_decode_sprite_map_entry(
            vram_words[vram_addr],
            vram_words[vram_addr + 1u]);
        uint32_t atlas_x = tile % atlas_cols;
        uint32_t atlas_y = tile / atlas_cols;

        for (uint8_t py = 0; py < NG_NEO_SPRITE_TILE_PIXELS; ++py) {
            uint8_t source_y = entry.vflip ?
                (uint8_t)(NG_NEO_SPRITE_TILE_PIXELS - 1u - py) : py;
            uint8_t pixels[NG_NEO_SPRITE_TILE_PIXELS];
            if (!ng_neogeo_video_decode_sprite_tile_line(c_rom,
                                                         c_rom_size,
                                                         entry.tile_index,
                                                         source_y,
                                                         entry.hflip,
                                                         pixels)) {
                memset(pixels, 0, sizeof(pixels));
            }

            uint32_t out_y = atlas_y * NG_NEO_SPRITE_TILE_PIXELS + py;
            uint32_t *dst = out_argb + out_y * out_stride_pixels +
                            atlas_x * NG_NEO_SPRITE_TILE_PIXELS;
            uint16_t palette_base = (uint16_t)((uint16_t)entry.palette << 4);
            for (uint8_t px = 0; px < NG_NEO_SPRITE_TILE_PIXELS; ++px) {
                uint8_t color = pixels[px] & 0x0Fu;
                if (color == 0u) {
                    dst[px] = 0xFF000000u;
                } else {
                    dst[px] = ng_neogeo_video_palette_argb(
                        palette_words,
                        palette_word_count,
                        (uint16_t)(palette_base | color));
                }
            }
        }
    }

    return 1;
}

static uint32_t ng_neogeo_video_palette_argb(const uint16_t *palette_words,
                                             uint32_t palette_word_count,
                                             uint16_t palette_index) {
    if (!palette_words || palette_index >= palette_word_count) {
        return 0xFF000000u;
    }
    return ng_neogeo_video_palette_word_to_argb(palette_words[palette_index]);
}

int ng_neogeo_video_render_fix_layer_argb(const uint8_t *s_rom,
                                          uint32_t s_rom_size,
                                          const uint16_t *vram_words,
                                          uint32_t vram_word_count,
                                          const uint16_t *palette_words,
                                          uint32_t palette_word_count,
                                          uint32_t *out_argb,
                                          uint32_t out_width,
                                          uint32_t out_height,
                                          uint32_t out_stride_pixels) {
    if (!s_rom || !vram_words || !out_argb ||
        vram_word_count < (NG_NEO_FIX_MAP_BASE +
                           NG_NEO_FIX_MAP_COLS * NG_NEO_FIX_MAP_ROWS) ||
        out_width < NG_NEO_FIX_FRAME_WIDTH ||
        out_height < NG_NEO_FIX_FRAME_HEIGHT ||
        out_stride_pixels < out_width) {
        return 0;
    }

    for (uint32_t y = 0; y < out_height; ++y) {
        for (uint32_t x = 0; x < out_width; ++x) {
            out_argb[y * out_stride_pixels + x] = 0xFF000000u;
        }
    }

    for (uint32_t tile_x = 0; tile_x < NG_NEO_FIX_MAP_COLS; ++tile_x) {
        for (uint32_t tile_y = 0; tile_y < NG_NEO_FIX_MAP_ROWS; ++tile_y) {
            uint32_t map_addr = NG_NEO_FIX_MAP_BASE +
                                tile_x * NG_NEO_FIX_MAP_ROWS + tile_y;
            uint16_t map_word = vram_words[map_addr];
            uint16_t tile_index = (uint16_t)(map_word & 0x0FFFu);
            uint16_t palette_base = (uint16_t)((map_word >> 12) << 4);

            for (uint8_t py = 0; py < NG_NEO_FIX_TILE_PIXELS; ++py) {
                uint8_t pixels[NG_NEO_FIX_TILE_PIXELS];
                if (!ng_neogeo_video_decode_fix_tile_line(s_rom,
                                                          s_rom_size,
                                                          tile_index,
                                                          py,
                                                          pixels)) {
                    memset(pixels, 0, sizeof(pixels));
                }

                uint32_t out_y = tile_y * NG_NEO_FIX_TILE_PIXELS + py;
                uint32_t *dst = out_argb + out_y * out_stride_pixels +
                                tile_x * NG_NEO_FIX_TILE_PIXELS;
                for (uint8_t px = 0; px < NG_NEO_FIX_TILE_PIXELS; ++px) {
                    uint8_t color = pixels[px] & 0x0Fu;
                    if (color == 0u) {
                        dst[px] = 0xFF000000u;
                    } else {
                        dst[px] = ng_neogeo_video_palette_argb(
                            palette_words,
                            palette_word_count,
                            (uint16_t)(palette_base | color));
                    }
                }
            }
        }
    }

    return 1;
}
