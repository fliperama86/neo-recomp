#pragma once

#include <stdint.h>

typedef struct NgProgramRom {
    uint8_t *data;
    uint32_t size;
} NgProgramRom;

int ng_program_rom_load(NgProgramRom *rom, const char *p1_path, const char *p2_path);
void ng_program_rom_free(NgProgramRom *rom);

uint8_t ng_program_rom_read8(const NgProgramRom *rom, uint32_t addr);
uint16_t ng_program_rom_read16(const NgProgramRom *rom, uint32_t addr);
uint32_t ng_program_rom_read32(const NgProgramRom *rom, uint32_t addr);

uint32_t ng_program_rom_initial_ssp(const NgProgramRom *rom);
uint32_t ng_program_rom_initial_pc(const NgProgramRom *rom);
int ng_program_rom_addr_is_mapped(const NgProgramRom *rom, uint32_t addr);
int ng_program_rom_initial_pc_is_mapped(const NgProgramRom *rom);
