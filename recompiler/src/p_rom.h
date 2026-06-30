#pragma once

#include <stdint.h>

#define NG_PROGRAM_ROM_MAX_BANKS 256u

typedef struct NgProgramRom {
    uint8_t *data;
    uint32_t size;
    uint32_t fixed_base;
    uint32_t fixed_size;
    uint32_t bank_window_base;
    uint32_t bank_window_size;
    uint32_t active_bank;
    uint32_t bank_count;
    uint32_t bank_offsets[NG_PROGRAM_ROM_MAX_BANKS];
    uint32_t bank_sizes[NG_PROGRAM_ROM_MAX_BANKS];
    uint8_t bank_configured[NG_PROGRAM_ROM_MAX_BANKS];
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
int ng_program_rom_configure_bank(NgProgramRom *rom,
                                  uint32_t bank_id,
                                  uint32_t physical_offset,
                                  uint32_t size);
void ng_program_rom_select_bank(NgProgramRom *rom, uint32_t bank_id);
uint32_t ng_program_rom_bank_count(const NgProgramRom *rom);
int ng_program_rom_bank_is_configured(const NgProgramRom *rom,
                                      uint32_t bank_id);
int ng_program_rom_addr_is_banked(const NgProgramRom *rom, uint32_t addr);

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
