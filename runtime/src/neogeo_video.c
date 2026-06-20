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
        /* .neo C data is the MiSTer/MAME interleaved sprite region.  After
         * applying MiSTer's sprite load swizzle for one 8-pixel CR chunk, the
         * four bytes are C2, C2, C1, C1: C1 carries color bits 0/1 and C2
         * carries color bits 2/3. */
        out_pixels[x] = (uint8_t)((((uint16_t)chunk[2] >> bit) & 1u) << 0 |
                                  ((((uint16_t)chunk[3] >> bit) & 1u) << 1) |
                                  ((((uint16_t)chunk[0] >> bit) & 1u) << 2) |
                                  ((((uint16_t)chunk[1] >> bit) & 1u) << 3));
    }
}

static uint8_t ng_neogeo_video_sprite_raw_byte_after_bswap(const uint8_t *tile,
                                                           uint8_t byte_offset) {
    static const uint8_t raw_byte_for_swapped_byte[4] = {0u, 2u, 1u, 3u};
    return tile[(byte_offset & 0xFCu) |
                raw_byte_for_swapped_byte[byte_offset & 0x03u]];
}

static void ng_neogeo_video_load_sprite_line(const uint8_t *tile,
                                             uint8_t y,
                                             uint8_t out_line[8]) {
    for (uint8_t word = 0; word < 4u; ++word) {
        uint8_t converted_word = (uint8_t)(y * 4u + word);
        uint8_t source_word = (uint8_t)(((converted_word ^ 1u) & 1u) |
                                        ((converted_word >> 1u) & 0x1Eu) |
                                        (((converted_word & 2u) ^ 2u) << 4u));
        out_line[word * 2u + 0u] =
            ng_neogeo_video_sprite_raw_byte_after_bswap(
                tile,
                (uint8_t)(source_word * 2u + 0u));
        out_line[word * 2u + 1u] =
            ng_neogeo_video_sprite_raw_byte_after_bswap(
                tile,
                (uint8_t)(source_word * 2u + 1u));
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

    uint8_t line[NG_NEO_SPRITE_TILE_BYTES / NG_NEO_SPRITE_TILE_PIXELS];
    ng_neogeo_video_load_sprite_line(c_rom + tile_offset, y, line);
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

/* Set bits are the source pixels kept by the Neo Geo horizontal shrinker. */
static const uint16_t ng_neogeo_video_hshrink_masks[16] = {
    0x0080u, 0x0880u, 0x0888u, 0x2888u,
    0x288Au, 0x2A8Au, 0x2AAAu, 0xAAAAu,
    0xAAEAu, 0xBAEAu, 0xBAEBu, 0xBBEBu,
    0xBBEFu, 0xFBEFu, 0xFBFFu, 0xFFFFu,
};

static uint16_t ng_neogeo_video_sprite_y(uint16_t scb3_word) {
    return (uint16_t)((496u - ((scb3_word >> 7) & 0x01FFu)) & 0x01FFu);
}

static int ng_neogeo_video_sprite_on_scanline(uint32_t scanline,
                                              uint16_t y,
                                              uint8_t rows) {
    return rows >= 0x20u ||
           (((scanline - (uint32_t)y) & 0x01FFu) <
            (uint32_t)rows * NG_NEO_SPRITE_TILE_PIXELS);
}

static void ng_neogeo_video_sprite_zoom_source(const uint8_t *zoom_rom,
                                               uint32_t zoom_rom_size,
                                               uint8_t zoom_y,
                                               uint8_t zoom_line,
                                               uint8_t *out_tile,
                                               uint8_t *out_line) {
    if (zoom_rom && zoom_rom_size >= NG_NEO_ZOOM_ROM_BYTES) {
        uint8_t source_line =
            zoom_rom[((uint32_t)zoom_y << 8) | (uint32_t)zoom_line];
        *out_tile = (uint8_t)(source_line >> 4);
        *out_line = (uint8_t)(source_line & 0x0Fu);
        return;
    }

    /* Hardware uses the LO zoom ROM as a 256x256 line/tile lookup.  Keep the
       full-size path exact and use a monotonic approximation for shrunken
       sprites when the user-provided LO region is not available. */
    uint16_t source_line;
    if (zoom_y == 0xFFu) {
        source_line = zoom_line;
    } else if (zoom_line <= zoom_y) {
        source_line = (uint16_t)(((uint32_t)zoom_line * 256u) /
                                 ((uint32_t)zoom_y + 1u));
    } else {
        source_line = 0xFFu;
    }

    *out_tile = (uint8_t)(source_line >> 4);
    *out_line = (uint8_t)(source_line & 0x0Fu);
}

static uint8_t ng_neogeo_video_build_active_sprite_list(
    const uint16_t *vram_words,
    uint32_t scanline,
    uint16_t active_sprites[NG_NEO_SPRITES_PER_SCANLINE]) {
    uint16_t y = 0;
    uint8_t rows = 0;
    uint8_t active_count = 0;

    for (uint16_t sprite = 0; sprite < NG_NEO_SPRITE_DISPLAY_LIMIT; ++sprite) {
        uint16_t scb3 = vram_words[0x8200u + sprite];
        if (((scb3 >> 6) & 1u) == 0u) {
            y = ng_neogeo_video_sprite_y(scb3);
            rows = (uint8_t)(scb3 & 0x3Fu);
        }

        if (rows == 0u ||
            !ng_neogeo_video_sprite_on_scanline(scanline, y, rows)) {
            continue;
        }

        active_sprites[active_count++] = sprite;
        if (active_count == NG_NEO_SPRITES_PER_SCANLINE) {
            break;
        }
    }

    return active_count;
}

static void ng_neogeo_video_draw_sprite_scanline(
    const uint8_t *c_rom,
    uint32_t c_rom_size,
    const NgNeoVideoRenderOptions *options,
    const uint16_t *vram_words,
    const uint16_t *palette_words,
    uint32_t palette_word_count,
    uint32_t *dst,
    uint32_t scanline,
    uint16_t sprite,
    uint16_t *chain_x,
    uint16_t *chain_y,
    uint8_t *chain_rows,
    uint8_t *chain_zoom_y,
    uint8_t *chain_zoom_x) {
    uint16_t scb2 = vram_words[0x8000u + sprite];
    uint16_t scb3 = vram_words[0x8200u + sprite];
    uint16_t scb4 = vram_words[0x8400u + sprite];
    uint8_t sticky = (uint8_t)((scb3 >> 6) & 1u);

    uint16_t x;
    uint16_t y;
    uint8_t rows;
    uint8_t zoom_y;
    uint8_t zoom_x;
    if (sticky) {
        x = (uint16_t)((*chain_x + (uint16_t)*chain_zoom_x + 1u) & 0x01FFu);
        y = *chain_y;
        rows = *chain_rows;
        zoom_y = *chain_zoom_y;
        zoom_x = (uint8_t)((scb2 >> 8) & 0x0Fu);
    } else {
        x = (uint16_t)((scb4 >> 7) & 0x01FFu);
        y = ng_neogeo_video_sprite_y(scb3);
        rows = (uint8_t)(scb3 & 0x3Fu);
        zoom_y = (uint8_t)(scb2 & 0xFFu);
        zoom_x = (uint8_t)((scb2 >> 8) & 0x0Fu);
    }

    *chain_x = x;
    *chain_y = y;
    *chain_rows = rows;
    *chain_zoom_y = zoom_y;
    *chain_zoom_x = zoom_x;

    if (rows == 0u || (x >= NG_NEO_SPRITE_FRAME_WIDTH && x <= 0x01F0u) ||
        !ng_neogeo_video_sprite_on_scanline(scanline, y, rows)) {
        return;
    }

    uint16_t sprite_line = (uint16_t)(((uint32_t)scanline - (uint32_t)y) & 0x01FFu);
    uint8_t zoom_line = (uint8_t)sprite_line;
    uint8_t invert = (uint8_t)((sprite_line >> 8) & 1u);
    if (invert) {
        zoom_line ^= 0xFFu;
    }

    if (rows > 0x20u) {
        uint16_t period = (uint16_t)(((uint16_t)zoom_y + 1u) << 1);
        if (period != 0u) {
            zoom_line = (uint8_t)(zoom_line % period);
            if (zoom_line > zoom_y) {
                zoom_line = (uint8_t)(period - 1u - zoom_line);
                invert = (uint8_t)!invert;
            }
        }
    }

    uint8_t tile_y;
    uint8_t source_y;
    ng_neogeo_video_sprite_zoom_source(options ? options->zoom_rom : NULL,
                                       options ? options->zoom_rom_size : 0,
                                       zoom_y,
                                       zoom_line,
                                       &tile_y,
                                       &source_y);
    if (invert) {
        tile_y ^= 0x1Fu;
        source_y ^= 0x0Fu;
    }

    uint32_t map_addr = (uint32_t)sprite * 64u + (uint32_t)tile_y * 2u;
    if (map_addr + 1u >= 0x7000u) {
        return;
    }

    NgNeoSpriteMapEntry entry = ng_neogeo_video_decode_sprite_map_entry(
        vram_words[map_addr],
        vram_words[map_addr + 1u]);
    uint32_t tile_index = entry.tile_index;
    if (!options || !options->auto_animation_disabled) {
        if (entry.auto_animation & 0x02u) {
            tile_index = (tile_index & ~0x07u) |
                         (uint32_t)((options ? options->auto_animation_counter : 0u) & 0x07u);
        } else if (entry.auto_animation & 0x01u) {
            tile_index = (tile_index & ~0x03u) |
                         (uint32_t)((options ? options->auto_animation_counter : 0u) & 0x03u);
        }
    }
    if (entry.vflip) {
        source_y ^= 0x0Fu;
    }

    uint8_t pixels[NG_NEO_SPRITE_TILE_PIXELS];
    if (!ng_neogeo_video_decode_sprite_tile_line(c_rom,
                                                 c_rom_size,
                                                 tile_index,
                                                 source_y,
                                                 entry.hflip,
                                                 pixels)) {
        memset(pixels, 0, sizeof(pixels));
    }

    uint16_t hmask = ng_neogeo_video_hshrink_masks[zoom_x & 0x0Fu];
    uint16_t palette_base = (uint16_t)((uint16_t)entry.palette << 4);
    uint16_t wrap_x = x;
    uint32_t out_x = x <= 0x01F0u ? (uint32_t)x : 0u;

    for (uint8_t px = 0; px < NG_NEO_SPRITE_TILE_PIXELS; ++px) {
        if ((hmask & 0x8000u) != 0u) {
            uint8_t color = pixels[px] & 0x0Fu;
            if (x > 0x01F0u) {
                if (wrap_x >= 0x0200u) {
                    if (out_x < NG_NEO_SPRITE_FRAME_WIDTH && color != 0u) {
                        dst[out_x] = ng_neogeo_video_palette_argb(
                            palette_words,
                            palette_word_count,
                            (uint16_t)(palette_base | color));
                    }
                    out_x++;
                }
                wrap_x = (uint16_t)(wrap_x + 1u);
            } else {
                if (out_x < NG_NEO_SPRITE_FRAME_WIDTH && color != 0u) {
                    dst[out_x] = ng_neogeo_video_palette_argb(
                        palette_words,
                        palette_word_count,
                        (uint16_t)(palette_base | color));
                }
                out_x++;
            }
        }

        hmask = (uint16_t)(hmask << 1);
        if (hmask == 0u) {
            break;
        }
    }
}

int ng_neogeo_video_render_sprite_frame_argb_with_options(
    const uint8_t *c_rom,
    uint32_t c_rom_size,
    const NgNeoVideoRenderOptions *options,
    const uint16_t *vram_words,
    uint32_t vram_word_count,
    const uint16_t *palette_words,
    uint32_t palette_word_count,
    uint32_t *out_argb,
    uint32_t out_width,
    uint32_t out_height,
    uint32_t out_stride_pixels) {
    if (!c_rom || !vram_words || !out_argb ||
        vram_word_count < 0x8600u ||
        out_width < NG_NEO_SPRITE_FRAME_WIDTH ||
        out_height < NG_NEO_SPRITE_FRAME_HEIGHT ||
        out_stride_pixels < out_width) {
        return 0;
    }

    for (uint32_t y = 0; y < out_height; ++y) {
        for (uint32_t x = 0; x < out_width; ++x) {
            out_argb[y * out_stride_pixels + x] = 0xFF000000u;
        }
    }

    for (uint32_t scanline = 0; scanline < NG_NEO_SPRITE_FRAME_HEIGHT; ++scanline) {
        uint16_t active_sprites[NG_NEO_SPRITES_PER_SCANLINE];
        uint8_t active_count = ng_neogeo_video_build_active_sprite_list(
            vram_words,
            scanline,
            active_sprites);
        uint16_t chain_x = 0;
        uint16_t chain_y = 0;
        uint8_t chain_rows = 0;
        uint8_t chain_zoom_y = 0xFFu;
        uint8_t chain_zoom_x = 0x0Fu;
        uint32_t *dst = out_argb + scanline * out_stride_pixels;

        for (uint8_t i = 0; i < active_count; ++i) {
            ng_neogeo_video_draw_sprite_scanline(c_rom,
                                                 c_rom_size,
                                                 options,
                                                 vram_words,
                                                 palette_words,
                                                 palette_word_count,
                                                 dst,
                                                 scanline,
                                                 active_sprites[i],
                                                 &chain_x,
                                                 &chain_y,
                                                 &chain_rows,
                                                 &chain_zoom_y,
                                                 &chain_zoom_x);
        }
    }

    return 1;
}

int ng_neogeo_video_render_sprite_frame_argb_with_zoom(
    const uint8_t *c_rom,
    uint32_t c_rom_size,
    const uint8_t *zoom_rom,
    uint32_t zoom_rom_size,
    const uint16_t *vram_words,
    uint32_t vram_word_count,
    const uint16_t *palette_words,
    uint32_t palette_word_count,
    uint32_t *out_argb,
    uint32_t out_width,
    uint32_t out_height,
    uint32_t out_stride_pixels) {
    NgNeoVideoRenderOptions options;
    memset(&options, 0, sizeof(options));
    options.zoom_rom = zoom_rom;
    options.zoom_rom_size = zoom_rom_size;
    return ng_neogeo_video_render_sprite_frame_argb_with_options(
        c_rom,
        c_rom_size,
        &options,
        vram_words,
        vram_word_count,
        palette_words,
        palette_word_count,
        out_argb,
        out_width,
        out_height,
        out_stride_pixels);
}

int ng_neogeo_video_render_sprite_frame_argb(const uint8_t *c_rom,
                                             uint32_t c_rom_size,
                                             const uint16_t *vram_words,
                                             uint32_t vram_word_count,
                                             const uint16_t *palette_words,
                                             uint32_t palette_word_count,
                                             uint32_t *out_argb,
                                             uint32_t out_width,
                                             uint32_t out_height,
                                             uint32_t out_stride_pixels) {
    return ng_neogeo_video_render_sprite_frame_argb_with_options(c_rom,
                                                                 c_rom_size,
                                                                 NULL,
                                                                 vram_words,
                                                                 vram_word_count,
                                                                 palette_words,
                                                                 palette_word_count,
                                                                 out_argb,
                                                                 out_width,
                                                                 out_height,
                                                                 out_stride_pixels);
}

static int ng_neogeo_video_overlay_fix_layer_argb(const uint8_t *s_rom,
                                                  uint32_t s_rom_size,
                                                  const uint16_t *vram_words,
                                                  uint32_t vram_word_count,
                                                  const uint16_t *palette_words,
                                                  uint32_t palette_word_count,
                                                  uint32_t *out_argb,
                                                  uint32_t out_width,
                                                  uint32_t out_height,
                                                  uint32_t out_stride_pixels,
                                                  uint32_t source_y_offset) {
    if (!s_rom || !vram_words || !out_argb ||
        vram_word_count < (NG_NEO_FIX_MAP_BASE +
                           NG_NEO_FIX_MAP_COLS * NG_NEO_FIX_MAP_ROWS) ||
        out_width < NG_NEO_SPRITE_FRAME_WIDTH ||
        out_height < NG_NEO_SPRITE_FRAME_HEIGHT ||
        out_stride_pixels < out_width ||
        source_y_offset > NG_NEO_FIX_FRAME_HEIGHT ||
        out_height > NG_NEO_FIX_FRAME_HEIGHT - source_y_offset) {
        return 0;
    }

    for (uint32_t tile_x = 0; tile_x < NG_NEO_FIX_MAP_COLS; ++tile_x) {
        for (uint32_t tile_y = 0; tile_y < NG_NEO_FIX_MAP_ROWS; ++tile_y) {
            uint32_t map_addr = NG_NEO_FIX_MAP_BASE +
                                tile_x * NG_NEO_FIX_MAP_ROWS + tile_y;
            uint16_t map_word = vram_words[map_addr];
            uint16_t tile_index = (uint16_t)(map_word & 0x0FFFu);
            uint16_t palette_base = (uint16_t)((map_word >> 12) << 4);

            for (uint8_t py = 0; py < NG_NEO_FIX_TILE_PIXELS; ++py) {
                uint32_t source_y = tile_y * NG_NEO_FIX_TILE_PIXELS + py;
                if (source_y < source_y_offset ||
                    source_y >= source_y_offset + out_height) {
                    continue;
                }

                uint8_t pixels[NG_NEO_FIX_TILE_PIXELS];
                if (!ng_neogeo_video_decode_fix_tile_line(s_rom,
                                                          s_rom_size,
                                                          tile_index,
                                                          py,
                                                          pixels)) {
                    memset(pixels, 0, sizeof(pixels));
                }

                uint32_t out_y = source_y - source_y_offset;
                uint32_t *dst = out_argb + out_y * out_stride_pixels +
                                tile_x * NG_NEO_FIX_TILE_PIXELS;
                for (uint8_t px = 0; px < NG_NEO_FIX_TILE_PIXELS; ++px) {
                    uint8_t color = pixels[px] & 0x0Fu;
                    if (color == 0u) {
                        continue;
                    }
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

int ng_neogeo_video_render_frame_argb_with_options(
    const uint8_t *s_rom,
    uint32_t s_rom_size,
    const uint8_t *c_rom,
    uint32_t c_rom_size,
    const NgNeoVideoRenderOptions *options,
    const uint16_t *vram_words,
    uint32_t vram_word_count,
    const uint16_t *palette_words,
    uint32_t palette_word_count,
    uint32_t *out_argb,
    uint32_t out_width,
    uint32_t out_height,
    uint32_t out_stride_pixels) {
    if (!ng_neogeo_video_render_sprite_frame_argb_with_options(c_rom,
                                                               c_rom_size,
                                                               options,
                                                               vram_words,
                                                               vram_word_count,
                                                               palette_words,
                                                               palette_word_count,
                                                               out_argb,
                                                               out_width,
                                                               out_height,
                                                               out_stride_pixels)) {
        return 0;
    }
    return ng_neogeo_video_overlay_fix_layer_argb(s_rom,
                                                  s_rom_size,
                                                  vram_words,
                                                  vram_word_count,
                                                  palette_words,
                                                  palette_word_count,
                                                  out_argb,
                                                  out_width,
                                                  out_height,
                                                  out_stride_pixels,
                                                  NG_NEO_FIX_VISIBLE_Y_OFFSET);
}

int ng_neogeo_video_render_frame_argb_with_zoom(const uint8_t *s_rom,
                                                uint32_t s_rom_size,
                                                const uint8_t *c_rom,
                                                uint32_t c_rom_size,
                                                const uint8_t *zoom_rom,
                                                uint32_t zoom_rom_size,
                                                const uint16_t *vram_words,
                                                uint32_t vram_word_count,
                                                const uint16_t *palette_words,
                                                uint32_t palette_word_count,
                                                uint32_t *out_argb,
                                                uint32_t out_width,
                                                uint32_t out_height,
                                                uint32_t out_stride_pixels) {
    NgNeoVideoRenderOptions options;
    memset(&options, 0, sizeof(options));
    options.zoom_rom = zoom_rom;
    options.zoom_rom_size = zoom_rom_size;
    return ng_neogeo_video_render_frame_argb_with_options(s_rom,
                                                          s_rom_size,
                                                          c_rom,
                                                          c_rom_size,
                                                          &options,
                                                          vram_words,
                                                          vram_word_count,
                                                          palette_words,
                                                          palette_word_count,
                                                          out_argb,
                                                          out_width,
                                                          out_height,
                                                          out_stride_pixels);
}

int ng_neogeo_video_render_frame_argb(const uint8_t *s_rom,
                                      uint32_t s_rom_size,
                                      const uint8_t *c_rom,
                                      uint32_t c_rom_size,
                                      const uint16_t *vram_words,
                                      uint32_t vram_word_count,
                                      const uint16_t *palette_words,
                                      uint32_t palette_word_count,
                                      uint32_t *out_argb,
                                      uint32_t out_width,
                                      uint32_t out_height,
                                      uint32_t out_stride_pixels) {
    return ng_neogeo_video_render_frame_argb_with_options(s_rom,
                                                          s_rom_size,
                                                          c_rom,
                                                          c_rom_size,
                                                          NULL,
                                                          vram_words,
                                                          vram_word_count,
                                                          palette_words,
                                                          palette_word_count,
                                                          out_argb,
                                                          out_width,
                                                          out_height,
                                                          out_stride_pixels);
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
