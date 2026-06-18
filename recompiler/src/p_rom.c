#include "p_rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NEO_HEADER_SIZE 0x1000u

static int read_whole_file(const char *path, uint8_t **out_data, uint32_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
        return 0;
    }

    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return 0;
    }
    rewind(f);

    uint8_t *data = NULL;
    if (size > 0) {
        data = (uint8_t *)malloc((size_t)size);
        if (!data) {
            fclose(f);
            return 0;
        }
        if (fread(data, 1, (size_t)size, f) != (size_t)size) {
            free(data);
            fclose(f);
            return 0;
        }
    }

    fclose(f);
    *out_data = data;
    *out_size = (uint32_t)size;
    return 1;
}

static uint32_t read_le32(const uint8_t *data) {
    return ((uint32_t)data[0]) |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static void byteswap_words(uint8_t *data, uint32_t size) {
    for (uint32_t i = 0; i + 1 < size; i += 2) {
        uint8_t tmp = data[i];
        data[i] = data[i + 1];
        data[i + 1] = tmp;
    }
}

int ng_program_rom_load(NgProgramRom *rom, const char *p1_path, const char *p2_path) {
    uint8_t *p1 = NULL;
    uint8_t *p2 = NULL;
    uint32_t p1_size = 0;
    uint32_t p2_size = 0;

    if (!read_whole_file(p1_path, &p1, &p1_size)) {
        return 0;
    }
    if (p2_path && !read_whole_file(p2_path, &p2, &p2_size)) {
        free(p1);
        return 0;
    }

    uint32_t total = p1_size + p2_size;
    uint8_t *image = (uint8_t *)malloc(total ? total : 1);
    if (!image) {
        free(p1);
        free(p2);
        return 0;
    }

    memcpy(image, p1, p1_size);
    if (p2_size) {
        memcpy(image + p1_size, p2, p2_size);
    }

    free(p1);
    free(p2);

    rom->data = image;
    rom->size = total;
    return 1;
}

int ng_program_rom_load_neo(NgProgramRom *rom, const char *neo_path) {
    uint8_t *file_data = NULL;
    uint32_t file_size = 0;
    if (!read_whole_file(neo_path, &file_data, &file_size)) {
        return 0;
    }

    if (file_size < NEO_HEADER_SIZE ||
        file_data[0] != 'N' || file_data[1] != 'E' ||
        file_data[2] != 'O' || file_data[3] != 1) {
        fprintf(stderr, "%s is not a supported .neo image\n", neo_path);
        free(file_data);
        return 0;
    }

    uint32_t p_size = read_le32(file_data + 4);
    if (p_size == 0 || p_size > file_size - NEO_HEADER_SIZE) {
        fprintf(stderr, "%s has an invalid P-region size: %u\n", neo_path, p_size);
        free(file_data);
        return 0;
    }

    uint8_t *program = (uint8_t *)malloc(p_size);
    if (!program) {
        free(file_data);
        return 0;
    }

    memcpy(program, file_data + NEO_HEADER_SIZE, p_size);
    byteswap_words(program, p_size);
    free(file_data);

    rom->data = program;
    rom->size = p_size;
    return 1;
}

void ng_program_rom_free(NgProgramRom *rom) {
    free(rom->data);
    rom->data = NULL;
    rom->size = 0;
}

uint8_t ng_program_rom_read8(const NgProgramRom *rom, uint32_t addr) {
    return addr < rom->size ? rom->data[addr] : 0xFF;
}

uint16_t ng_program_rom_read16(const NgProgramRom *rom, uint32_t addr) {
    uint16_t hi = ng_program_rom_read8(rom, addr);
    uint16_t lo = ng_program_rom_read8(rom, addr + 1);
    return (uint16_t)((hi << 8) | lo);
}

uint32_t ng_program_rom_read32(const NgProgramRom *rom, uint32_t addr) {
    uint32_t hi = ng_program_rom_read16(rom, addr);
    uint32_t lo = ng_program_rom_read16(rom, addr + 2);
    return (hi << 16) | lo;
}

uint32_t ng_program_rom_initial_ssp(const NgProgramRom *rom) {
    return ng_program_rom_read32(rom, 0);
}

uint32_t ng_program_rom_initial_pc(const NgProgramRom *rom) {
    return ng_program_rom_read32(rom, 4);
}

int ng_program_rom_addr_is_mapped(const NgProgramRom *rom, uint32_t addr) {
    return addr < rom->size;
}

int ng_program_rom_initial_pc_is_mapped(const NgProgramRom *rom) {
    return ng_program_rom_addr_is_mapped(rom, ng_program_rom_initial_pc(rom));
}

int ng_program_rom_has_cart_header(const NgProgramRom *rom) {
    return ng_program_rom_read8(rom, 0x100) == 'N' &&
           ng_program_rom_read8(rom, 0x101) == 'E' &&
           ng_program_rom_read8(rom, 0x102) == 'O' &&
           ng_program_rom_read8(rom, 0x103) == '-' &&
           ng_program_rom_read8(rom, 0x104) == 'G' &&
           ng_program_rom_read8(rom, 0x105) == 'E' &&
           ng_program_rom_read8(rom, 0x106) == 'O' &&
           ng_program_rom_read8(rom, 0x107) == 0;
}

int ng_program_rom_cart_entry(const NgProgramRom *rom, uint32_t *out_addr) {
    if (!ng_program_rom_has_cart_header(rom)) {
        return 0;
    }
    if (ng_program_rom_read16(rom, 0x122) != 0x4EF9u) {
        return 0;
    }
    *out_addr = ng_program_rom_read32(rom, 0x124);
    return 1;
}
