#include "p_rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

