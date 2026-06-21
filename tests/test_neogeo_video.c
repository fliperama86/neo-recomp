#include "ngrecomp/neogeo_video.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static int test_palette_decode(void) {
    NgNeoRgb rgb = ng_neogeo_video_palette_word_to_rgb(0x0000u);
    CHECK(rgb.r == 0u && rgb.g == 0u && rgb.b == 0u);

    rgb = ng_neogeo_video_palette_word_to_rgb(0x4F00u);
    CHECK(rgb.r == 255u && rgb.g == 0u && rgb.b == 0u);

    rgb = ng_neogeo_video_palette_word_to_rgb(0x20F0u);
    CHECK(rgb.r == 0u && rgb.g == 255u && rgb.b == 0u);

    rgb = ng_neogeo_video_palette_word_to_rgb(0x100Fu);
    CHECK(rgb.r == 0u && rgb.g == 0u && rgb.b == 255u);

    uint32_t argb = ng_neogeo_video_palette_word_to_argb(0x4F00u);
    CHECK(argb == 0xFFFF0000u);

    rgb = ng_neogeo_video_palette_word_to_rgb(0xCF00u);
    CHECK(rgb.r < 255u && rgb.r > 0u && rgb.g == 0u && rgb.b == 0u);
    return 0;
}

static int test_planar_line_decode(void) {
    uint8_t pixels[NG_NEO_SPRITE_TILE_PIXELS];
    memset(pixels, 0xFF, sizeof(pixels));
    ng_neogeo_video_decode_4bpp_planar_line(0x8000u, 0x4000u, 0x2000u, 0x1000u, pixels);
    CHECK(pixels[0] == 1u);
    CHECK(pixels[1] == 2u);
    CHECK(pixels[2] == 4u);
    CHECK(pixels[3] == 8u);
    for (uint8_t i = 4; i < NG_NEO_SPRITE_TILE_PIXELS; ++i) {
        CHECK(pixels[i] == 0u);
    }

    ng_neogeo_video_decode_4bpp_planar_line(0xFFFFu, 0x0000u, 0xFFFFu, 0x0000u, pixels);
    for (uint8_t i = 0; i < NG_NEO_SPRITE_TILE_PIXELS; ++i) {
        CHECK(pixels[i] == 5u);
    }
    return 0;
}

static int test_fix_tile_line_decode(void) {
    uint8_t s_rom[NG_NEO_FIX_TILE_BYTES];
    memset(s_rom, 0, sizeof(s_rom));
    s_rom[0x10u + 2u] = 0x21u;
    s_rom[0x18u + 2u] = 0x43u;
    s_rom[0x00u + 2u] = 0x65u;
    s_rom[0x08u + 2u] = 0x87u;

    uint8_t pixels[NG_NEO_FIX_TILE_PIXELS];
    CHECK(ng_neogeo_video_decode_fix_tile_line(s_rom, sizeof(s_rom), 0, 2, pixels));
    for (uint8_t i = 0; i < NG_NEO_FIX_TILE_PIXELS; ++i) {
        CHECK(pixels[i] == (uint8_t)(i + 1u));
    }
    CHECK(!ng_neogeo_video_decode_fix_tile_line(s_rom, 4, 0, 2, pixels));
    CHECK(!ng_neogeo_video_decode_fix_tile_line(s_rom, sizeof(s_rom), 0, 8, pixels));
    return 0;
}

static void encode_sprite_line(uint8_t *tile,
                               uint8_t y,
                               const uint8_t pixels[16]) {
    for (uint8_t x = 0; x < 16u; ++x) {
        uint8_t bit = (uint8_t)(x & 7u);
        uint32_t plane_base = x < 8u ? 0x40u : 0x00u;
        uint8_t *planes = tile + plane_base + (uint32_t)y * 4u;
        uint8_t color = pixels[x] & 0x0Fu;
        planes[0] |= (uint8_t)(((color >> 0) & 1u) << bit);
        planes[2] |= (uint8_t)(((color >> 1) & 1u) << bit);
        planes[1] |= (uint8_t)(((color >> 2) & 1u) << bit);
        planes[3] |= (uint8_t)(((color >> 3) & 1u) << bit);
    }
}

static int test_sprite_tile_line_decode(void) {
    uint8_t c_rom[NG_NEO_SPRITE_TILE_BYTES];
    memset(c_rom, 0, sizeof(c_rom));
    uint8_t source[NG_NEO_SPRITE_TILE_PIXELS];
    for (uint8_t i = 0; i < NG_NEO_SPRITE_TILE_PIXELS; ++i) {
        source[i] = i;
    }
    encode_sprite_line(c_rom, 3u, source);

    uint8_t pixels[NG_NEO_SPRITE_TILE_PIXELS];
    CHECK(ng_neogeo_video_decode_sprite_tile_line(c_rom,
                                                  sizeof(c_rom),
                                                  0,
                                                  3,
                                                  0,
                                                  pixels));
    for (uint8_t i = 0; i < NG_NEO_SPRITE_TILE_PIXELS; ++i) {
        CHECK(pixels[i] == i);
    }

    CHECK(ng_neogeo_video_decode_sprite_tile_line(c_rom,
                                                  sizeof(c_rom),
                                                  0,
                                                  3,
                                                  1,
                                                  pixels));
    for (uint8_t i = 0; i < NG_NEO_SPRITE_TILE_PIXELS; ++i) {
        CHECK(pixels[i] == (uint8_t)(NG_NEO_SPRITE_TILE_PIXELS - 1u - i));
    }

    CHECK(!ng_neogeo_video_decode_sprite_tile_line(c_rom,
                                                   sizeof(c_rom),
                                                   0,
                                                   NG_NEO_SPRITE_TILE_PIXELS,
                                                   0,
                                                   pixels));
    CHECK(!ng_neogeo_video_decode_sprite_tile_line(c_rom, 4, 0, 0, 0, pixels));
    return 0;
}

static int test_sprite_map_decode(void) {
    NgNeoSpriteMapEntry entry =
        ng_neogeo_video_decode_sprite_map_entry(0x3456u, 0xABCDu);
    CHECK(entry.tile_index == 0xC3456u);
    CHECK(entry.palette == 0xABu);
    CHECK(entry.auto_animation == 3u);
    CHECK(entry.vflip == 0u);
    CHECK(entry.hflip == 1u);
    return 0;
}

static int test_fix_layer_render(void) {
    uint8_t s_rom[NG_NEO_FIX_TILE_BYTES];
    uint16_t *vram = (uint16_t *)calloc(0x10000u, sizeof(uint16_t));
    uint16_t palette[NG_NEO_PALETTE_COLORS_PER_BANK];
    uint32_t *frame = (uint32_t *)calloc(NG_NEO_FIX_FRAME_WIDTH * NG_NEO_FIX_FRAME_HEIGHT,
                                         sizeof(uint32_t));
    CHECK(vram != NULL && frame != NULL);
    memset(s_rom, 0, sizeof(s_rom));
    memset(palette, 0, sizeof(palette));

    s_rom[0x10u] = 0x21u;
    vram[NG_NEO_FIX_MAP_BASE] = 0x1000u;
    palette[0x11u] = 0x4F00u;
    palette[0x12u] = 0x20F0u;

    CHECK(ng_neogeo_video_render_fix_layer_argb(s_rom,
                                                sizeof(s_rom),
                                                vram,
                                                0x10000u,
                                                palette,
                                                NG_NEO_PALETTE_COLORS_PER_BANK,
                                                frame,
                                                NG_NEO_FIX_FRAME_WIDTH,
                                                NG_NEO_FIX_FRAME_HEIGHT,
                                                NG_NEO_FIX_FRAME_WIDTH));
    CHECK(frame[0] == 0xFFFF0000u);
    CHECK(frame[1] == 0xFF00FF00u);
    CHECK(frame[2] == 0xFF000000u);

    free(vram);
    free(frame);
    return 0;
}

static int test_sprite_map_atlas_render(void) {
    uint8_t c_rom[NG_NEO_SPRITE_TILE_BYTES];
    uint16_t vram[2] = {0, 0x0100u};
    uint16_t palette[NG_NEO_PALETTE_COLORS_PER_BANK];
    uint32_t frame[NG_NEO_SPRITE_TILE_PIXELS * NG_NEO_SPRITE_TILE_PIXELS];
    memset(c_rom, 0, sizeof(c_rom));
    memset(palette, 0, sizeof(palette));
    memset(frame, 0, sizeof(frame));

    uint8_t source[NG_NEO_SPRITE_TILE_PIXELS];
    memset(source, 0, sizeof(source));
    source[0] = 1u;
    source[1] = 2u;
    encode_sprite_line(c_rom, 0u, source);
    palette[0x11u] = 0x4F00u;
    palette[0x12u] = 0x20F0u;

    CHECK(ng_neogeo_video_render_sprite_map_atlas_argb(
        c_rom,
        sizeof(c_rom),
        vram,
        2,
        0,
        palette,
        NG_NEO_PALETTE_COLORS_PER_BANK,
        frame,
        1,
        1,
        NG_NEO_SPRITE_TILE_PIXELS,
        NG_NEO_SPRITE_TILE_PIXELS,
        NG_NEO_SPRITE_TILE_PIXELS));
    CHECK(frame[0] == 0xFFFF0000u);
    CHECK(frame[1] == 0xFF00FF00u);
    CHECK(frame[2] == 0xFF000000u);
    return 0;
}

static int test_sprite_frame_render(void) {
    uint8_t c_rom[NG_NEO_SPRITE_TILE_BYTES];
    uint16_t *vram = (uint16_t *)calloc(0x10000u, sizeof(uint16_t));
    uint16_t palette[NG_NEO_PALETTE_COLORS_PER_BANK];
    uint32_t *frame = (uint32_t *)calloc(NG_NEO_SPRITE_FRAME_WIDTH *
                                         NG_NEO_SPRITE_FRAME_HEIGHT,
                                         sizeof(uint32_t));
    CHECK(vram != NULL && frame != NULL);
    memset(c_rom, 0, sizeof(c_rom));
    memset(palette, 0, sizeof(palette));

    uint8_t source[NG_NEO_SPRITE_TILE_PIXELS];
    memset(source, 0, sizeof(source));
    source[0] = 1u;
    source[1] = 2u;
    encode_sprite_line(c_rom, 0u, source);

    vram[0x8001u] = 0x0FFFu;
    vram[0x8201u] = (uint16_t)((496u << 7) | 1u);
    vram[0x8401u] = 0x0000u;
    vram[64u] = 0x0000u;
    vram[65u] = 0x0100u;
    palette[0x11u] = 0x4F00u;
    palette[0x12u] = 0x20F0u;

    CHECK(ng_neogeo_video_render_sprite_frame_argb(
        c_rom,
        sizeof(c_rom),
        vram,
        0x10000u,
        palette,
        NG_NEO_PALETTE_COLORS_PER_BANK,
        frame,
        NG_NEO_SPRITE_FRAME_WIDTH,
        NG_NEO_SPRITE_FRAME_HEIGHT,
        NG_NEO_SPRITE_FRAME_WIDTH));
    CHECK(frame[0] == 0xFFFF0000u);
    CHECK(frame[1] == 0xFF00FF00u);
    CHECK(frame[2] == 0xFF000000u);

    free(vram);
    free(frame);
    return 0;
}

static int test_sprite_frame_uses_background_pen(void) {
    uint8_t c_rom[NG_NEO_SPRITE_TILE_BYTES];
    uint16_t *vram = (uint16_t *)calloc(0x10000u, sizeof(uint16_t));
    uint16_t palette[NG_NEO_PALETTE_COLORS_PER_BANK];
    uint32_t *frame = (uint32_t *)calloc(NG_NEO_SPRITE_FRAME_WIDTH *
                                         NG_NEO_SPRITE_FRAME_HEIGHT,
                                         sizeof(uint32_t));
    CHECK(vram != NULL && frame != NULL);
    memset(c_rom, 0, sizeof(c_rom));
    memset(palette, 0, sizeof(palette));

    uint8_t source[NG_NEO_SPRITE_TILE_PIXELS];
    memset(source, 0, sizeof(source));
    source[0] = 1u;
    encode_sprite_line(c_rom, 0u, source);

    vram[0x8001u] = 0x0FFFu;
    vram[0x8201u] = (uint16_t)((496u << 7) | 1u);
    vram[0x8401u] = 0x0000u;
    vram[64u] = 0x0000u;
    vram[65u] = 0x0100u;
    palette[0x11u] = 0x4F00u;
    palette[0x0FFFu] = 0x100Fu;

    CHECK(ng_neogeo_video_render_sprite_frame_argb(
        c_rom,
        sizeof(c_rom),
        vram,
        0x10000u,
        palette,
        NG_NEO_PALETTE_COLORS_PER_BANK,
        frame,
        NG_NEO_SPRITE_FRAME_WIDTH,
        NG_NEO_SPRITE_FRAME_HEIGHT,
        NG_NEO_SPRITE_FRAME_WIDTH));
    CHECK(frame[0] == 0xFFFF0000u);
    CHECK(frame[1] == 0xFF0000FFu);
    CHECK(frame[NG_NEO_SPRITE_FRAME_WIDTH + 1u] == 0xFF0000FFu);

    free(vram);
    free(frame);
    return 0;
}

static int test_sprite_frame_hshrink_and_sticky_chain(void) {
    uint8_t c_rom[NG_NEO_SPRITE_TILE_BYTES * 2u];
    uint16_t *vram = (uint16_t *)calloc(0x10000u, sizeof(uint16_t));
    uint16_t palette[NG_NEO_PALETTE_COLORS_PER_BANK];
    uint32_t *frame = (uint32_t *)calloc(NG_NEO_SPRITE_FRAME_WIDTH *
                                         NG_NEO_SPRITE_FRAME_HEIGHT,
                                         sizeof(uint32_t));
    CHECK(vram != NULL && frame != NULL);
    memset(c_rom, 0, sizeof(c_rom));
    memset(palette, 0, sizeof(palette));

    uint8_t source[NG_NEO_SPRITE_TILE_PIXELS];
    memset(source, 0, sizeof(source));
    source[8] = 1u;
    encode_sprite_line(c_rom, 0u, source);
    source[8] = 2u;
    encode_sprite_line(c_rom + NG_NEO_SPRITE_TILE_BYTES, 0u, source);

    vram[0x8001u] = 0x00FFu; /* full vertical zoom, minimum horizontal zoom */
    vram[0x8201u] = (uint16_t)((496u << 7) | 1u);
    vram[0x8401u] = 0x0000u;
    vram[64u] = 0x0000u;
    vram[65u] = 0x0100u;

    vram[0x8002u] = 0x00FFu;
    vram[0x8202u] = 0x0040u; /* sticky chain: inherit Y/rows from sprite 1 */
    vram[0x8402u] = 0x0000u;
    vram[128u] = 0x0001u;
    vram[129u] = 0x0100u;

    palette[0x11u] = 0x4F00u;
    palette[0x12u] = 0x20F0u;

    CHECK(ng_neogeo_video_render_sprite_frame_argb(
        c_rom,
        sizeof(c_rom),
        vram,
        0x10000u,
        palette,
        NG_NEO_PALETTE_COLORS_PER_BANK,
        frame,
        NG_NEO_SPRITE_FRAME_WIDTH,
        NG_NEO_SPRITE_FRAME_HEIGHT,
        NG_NEO_SPRITE_FRAME_WIDTH));

    CHECK(frame[0] == 0xFFFF0000u);
    CHECK(frame[1] == 0xFF00FF00u);
    CHECK(frame[2] == 0xFF000000u);

    free(vram);
    free(frame);
    return 0;
}

static int test_sprite_frame_wraparound_x_alignment(void) {
    uint8_t c_rom[NG_NEO_SPRITE_TILE_BYTES];
    uint16_t *vram = (uint16_t *)calloc(0x10000u, sizeof(uint16_t));
    uint16_t palette[NG_NEO_PALETTE_COLORS_PER_BANK];
    uint32_t *frame = (uint32_t *)calloc(NG_NEO_SPRITE_FRAME_WIDTH *
                                         NG_NEO_SPRITE_FRAME_HEIGHT,
                                         sizeof(uint32_t));
    CHECK(vram != NULL && frame != NULL);
    memset(c_rom, 0, sizeof(c_rom));
    memset(palette, 0, sizeof(palette));

    uint8_t source[NG_NEO_SPRITE_TILE_PIXELS];
    memset(source, 0, sizeof(source));
    source[0] = 1u;
    source[1] = 2u;
    encode_sprite_line(c_rom, 0u, source);

    vram[0x8001u] = 0x0FFFu;
    vram[0x8201u] = (uint16_t)((496u << 7) | 1u);
    vram[0x8401u] = 0xFF80u; /* X=511: first kept source pixel is off-screen. */
    vram[64u] = 0x0000u;
    vram[65u] = 0x0100u;
    palette[0x11u] = 0x4F00u;
    palette[0x12u] = 0x20F0u;

    CHECK(ng_neogeo_video_render_sprite_frame_argb(
        c_rom,
        sizeof(c_rom),
        vram,
        0x10000u,
        palette,
        NG_NEO_PALETTE_COLORS_PER_BANK,
        frame,
        NG_NEO_SPRITE_FRAME_WIDTH,
        NG_NEO_SPRITE_FRAME_HEIGHT,
        NG_NEO_SPRITE_FRAME_WIDTH));

    CHECK(frame[0] == 0xFF00FF00u);
    CHECK(frame[1] == 0xFF000000u);

    free(vram);
    free(frame);
    return 0;
}

static int test_sprite_frame_uses_zoom_rom_table(void) {
    uint8_t c_rom[NG_NEO_SPRITE_TILE_BYTES * 2u];
    uint8_t *zoom_rom = (uint8_t *)calloc(NG_NEO_ZOOM_ROM_BYTES, sizeof(uint8_t));
    uint16_t *vram = (uint16_t *)calloc(0x10000u, sizeof(uint16_t));
    uint16_t palette[NG_NEO_PALETTE_COLORS_PER_BANK];
    uint32_t *frame = (uint32_t *)calloc(NG_NEO_SPRITE_FRAME_WIDTH *
                                         NG_NEO_SPRITE_FRAME_HEIGHT,
                                         sizeof(uint32_t));
    CHECK(zoom_rom != NULL && vram != NULL && frame != NULL);
    memset(c_rom, 0, sizeof(c_rom));
    memset(palette, 0, sizeof(palette));

    uint8_t source[NG_NEO_SPRITE_TILE_PIXELS];
    memset(source, 0, sizeof(source));
    source[0] = 1u;
    encode_sprite_line(c_rom, 0u, source);
    source[0] = 2u;
    encode_sprite_line(c_rom + NG_NEO_SPRITE_TILE_BYTES, 0u, source);

    vram[0x8001u] = 0x0F01u; /* vertical zoom value 1, full horizontal zoom */
    vram[0x8201u] = (uint16_t)((496u << 7) | 2u);
    vram[0x8401u] = 0x0000u;
    vram[64u] = 0x0000u;
    vram[65u] = 0x0100u;
    vram[66u] = 0x0001u;
    vram[67u] = 0x0100u;
    zoom_rom[(0x01u << 8) | 0x00u] = 0x10u; /* map first scanline to map row 1 */

    palette[0x11u] = 0x4F00u;
    palette[0x12u] = 0x20F0u;

    CHECK(ng_neogeo_video_render_sprite_frame_argb_with_zoom(
        c_rom,
        sizeof(c_rom),
        zoom_rom,
        NG_NEO_ZOOM_ROM_BYTES,
        vram,
        0x10000u,
        palette,
        NG_NEO_PALETTE_COLORS_PER_BANK,
        frame,
        NG_NEO_SPRITE_FRAME_WIDTH,
        NG_NEO_SPRITE_FRAME_HEIGHT,
        NG_NEO_SPRITE_FRAME_WIDTH));

    CHECK(frame[0] == 0xFF00FF00u);

    free(zoom_rom);
    free(vram);
    free(frame);
    return 0;
}

static int test_sprite_frame_auto_animation_counter(void) {
    uint8_t c_rom[NG_NEO_SPRITE_TILE_BYTES * 4u];
    uint16_t *vram = (uint16_t *)calloc(0x10000u, sizeof(uint16_t));
    uint16_t palette[NG_NEO_PALETTE_COLORS_PER_BANK];
    uint32_t *frame = (uint32_t *)calloc(NG_NEO_SPRITE_FRAME_WIDTH *
                                         NG_NEO_SPRITE_FRAME_HEIGHT,
                                         sizeof(uint32_t));
    CHECK(vram != NULL && frame != NULL);
    memset(c_rom, 0, sizeof(c_rom));
    memset(palette, 0, sizeof(palette));

    uint8_t source[NG_NEO_SPRITE_TILE_PIXELS];
    memset(source, 0, sizeof(source));
    source[0] = 1u;
    encode_sprite_line(c_rom, 0u, source);
    source[0] = 2u;
    encode_sprite_line(c_rom + NG_NEO_SPRITE_TILE_BYTES * 2u, 0u, source);

    vram[0x8001u] = 0x0FFFu;
    vram[0x8201u] = (uint16_t)((496u << 7) | 1u);
    vram[0x8401u] = 0x0000u;
    vram[64u] = 0x0000u;
    vram[65u] = 0x0104u; /* 2-frame auto-animation replaces code bits 0..1. */
    palette[0x11u] = 0x4F00u;
    palette[0x12u] = 0x20F0u;

    NgNeoVideoRenderOptions options;
    memset(&options, 0, sizeof(options));
    options.auto_animation_counter = 2u;
    CHECK(ng_neogeo_video_render_sprite_frame_argb_with_options(
        c_rom,
        sizeof(c_rom),
        &options,
        vram,
        0x10000u,
        palette,
        NG_NEO_PALETTE_COLORS_PER_BANK,
        frame,
        NG_NEO_SPRITE_FRAME_WIDTH,
        NG_NEO_SPRITE_FRAME_HEIGHT,
        NG_NEO_SPRITE_FRAME_WIDTH));

    CHECK(frame[0] == 0xFF00FF00u);

    free(vram);
    free(frame);
    return 0;
}

static int test_frame_render_overlays_visible_fix_layer(void) {
    uint8_t s_rom[NG_NEO_FIX_TILE_BYTES];
    uint8_t c_rom[NG_NEO_SPRITE_TILE_BYTES];
    uint16_t *vram = (uint16_t *)calloc(0x10000u, sizeof(uint16_t));
    uint16_t palette[NG_NEO_PALETTE_COLORS_PER_BANK];
    uint32_t *frame = (uint32_t *)calloc(NG_NEO_SPRITE_FRAME_WIDTH *
                                         NG_NEO_SPRITE_FRAME_HEIGHT,
                                         sizeof(uint32_t));
    CHECK(vram != NULL && frame != NULL);
    memset(s_rom, 0, sizeof(s_rom));
    memset(c_rom, 0, sizeof(c_rom));
    memset(palette, 0, sizeof(palette));

    uint8_t source[NG_NEO_SPRITE_TILE_PIXELS];
    memset(source, 0, sizeof(source));
    source[0] = 1u;
    source[1] = 2u;
    encode_sprite_line(c_rom, 0u, source);

    vram[0x8001u] = 0x0FFFu;
    vram[0x8201u] = (uint16_t)((496u << 7) | 1u);
    vram[0x8401u] = 0x0000u;
    vram[64u] = 0x0000u;
    vram[65u] = 0x0100u;

    s_rom[0x10u] = 0x03u;
    vram[NG_NEO_FIX_MAP_BASE + 2u] = 0x1000u;

    palette[0x11u] = 0x4F00u;
    palette[0x12u] = 0x20F0u;
    palette[0x13u] = 0x100Fu;

    CHECK(ng_neogeo_video_render_frame_argb(
        s_rom,
        sizeof(s_rom),
        c_rom,
        sizeof(c_rom),
        vram,
        0x10000u,
        palette,
        NG_NEO_PALETTE_COLORS_PER_BANK,
        frame,
        NG_NEO_SPRITE_FRAME_WIDTH,
        NG_NEO_SPRITE_FRAME_HEIGHT,
        NG_NEO_SPRITE_FRAME_WIDTH));
    CHECK(frame[0] == 0xFF0000FFu);
    CHECK(frame[1] == 0xFF00FF00u);
    CHECK(frame[2] == 0xFF000000u);

    free(vram);
    free(frame);
    return 0;
}

int main(void) {
    if (test_palette_decode() != 0) return 1;
    if (test_planar_line_decode() != 0) return 1;
    if (test_fix_tile_line_decode() != 0) return 1;
    if (test_sprite_tile_line_decode() != 0) return 1;
    if (test_sprite_map_decode() != 0) return 1;
    if (test_fix_layer_render() != 0) return 1;
    if (test_sprite_map_atlas_render() != 0) return 1;
    if (test_sprite_frame_render() != 0) return 1;
    if (test_sprite_frame_uses_background_pen() != 0) return 1;
    if (test_sprite_frame_hshrink_and_sticky_chain() != 0) return 1;
    if (test_sprite_frame_wraparound_x_alignment() != 0) return 1;
    if (test_sprite_frame_uses_zoom_rom_table() != 0) return 1;
    if (test_sprite_frame_auto_animation_counter() != 0) return 1;
    if (test_frame_render_overlays_visible_fix_layer() != 0) return 1;
    return 0;
}
