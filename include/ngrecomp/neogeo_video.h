#pragma once

#include <stdint.h>

#define NG_NEO_FIX_TILE_PIXELS 8u
#define NG_NEO_FIX_TILE_BYTES 32u
#define NG_NEO_FIX_MAP_BASE 0x7000u
#define NG_NEO_FIX_MAP_COLS 40u
#define NG_NEO_FIX_MAP_ROWS 32u
#define NG_NEO_FIX_FRAME_WIDTH (NG_NEO_FIX_MAP_COLS * NG_NEO_FIX_TILE_PIXELS)
#define NG_NEO_FIX_FRAME_HEIGHT (NG_NEO_FIX_MAP_ROWS * NG_NEO_FIX_TILE_PIXELS)
#define NG_NEO_SPRITE_TILE_PIXELS 16u
#define NG_NEO_PALETTE_COLORS_PER_BANK 0x1000u

typedef struct NgNeoRgb {
    uint8_t r;
    uint8_t g;
    uint8_t b;
} NgNeoRgb;

uint8_t ng_neogeo_video_scale5(uint8_t value);
NgNeoRgb ng_neogeo_video_palette_word_to_rgb(uint16_t word);
uint32_t ng_neogeo_video_palette_word_to_argb(uint16_t word);

void ng_neogeo_video_decode_4bpp_planar_line(uint16_t plane0,
                                             uint16_t plane1,
                                             uint16_t plane2,
                                             uint16_t plane3,
                                             uint8_t out_pixels[NG_NEO_SPRITE_TILE_PIXELS]);

int ng_neogeo_video_decode_fix_tile_line(const uint8_t *s_rom,
                                         uint32_t s_rom_size,
                                         uint16_t tile_index,
                                         uint8_t y,
                                         uint8_t out_pixels[NG_NEO_FIX_TILE_PIXELS]);

int ng_neogeo_video_render_fix_layer_argb(const uint8_t *s_rom,
                                          uint32_t s_rom_size,
                                          const uint16_t *vram_words,
                                          uint32_t vram_word_count,
                                          const uint16_t *palette_words,
                                          uint32_t palette_word_count,
                                          uint32_t *out_argb,
                                          uint32_t out_width,
                                          uint32_t out_height,
                                          uint32_t out_stride_pixels);
