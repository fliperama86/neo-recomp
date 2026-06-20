#pragma once

#include <stdint.h>

typedef struct NgProgramRom {
    uint8_t *data;
    uint32_t size;
    uint32_t fixed_base;
    uint32_t fixed_size;
    uint32_t bank_window_base;
    uint32_t bank_window_size;
    int address_map_enabled;
} NgProgramRom;

typedef struct NgNeoRomRegion {
    uint8_t *data;
    uint32_t size;
} NgNeoRomRegion;

typedef struct NgNeoRomImage {
    /* P is normalized to 68000 byte order. The remaining regions are copied
     * exactly as stored in the .neo container: S fix, M Z80, V1/V2 audio, and
     * interleaved C sprite data. */
    NgNeoRomRegion p;
    NgNeoRomRegion s;
    NgNeoRomRegion m;
    NgNeoRomRegion v1;
    NgNeoRomRegion v2;
    NgNeoRomRegion c;
} NgNeoRomImage;

int ng_program_rom_load(NgProgramRom *rom, const char *p1_path, const char *p2_path);
int ng_program_rom_load_neo(NgProgramRom *rom, const char *neo_path);
void ng_program_rom_free(NgProgramRom *rom);
void ng_program_rom_set_address_map(NgProgramRom *rom,
                                    uint32_t fixed_base,
                                    uint32_t fixed_size,
                                    uint32_t bank_window_base,
                                    uint32_t bank_window_size);

int ng_neo_rom_image_load(NgNeoRomImage *image, const char *neo_path);
void ng_neo_rom_image_free(NgNeoRomImage *image);

uint8_t ng_program_rom_read8(const NgProgramRom *rom, uint32_t addr);
uint16_t ng_program_rom_read16(const NgProgramRom *rom, uint32_t addr);
uint32_t ng_program_rom_read32(const NgProgramRom *rom, uint32_t addr);

uint32_t ng_program_rom_initial_ssp(const NgProgramRom *rom);
uint32_t ng_program_rom_initial_pc(const NgProgramRom *rom);
int ng_program_rom_addr_is_mapped(const NgProgramRom *rom, uint32_t addr);
int ng_program_rom_initial_pc_is_mapped(const NgProgramRom *rom);
int ng_program_rom_has_cart_header(const NgProgramRom *rom);
int ng_program_rom_cart_entry(const NgProgramRom *rom, uint32_t *out_addr);
