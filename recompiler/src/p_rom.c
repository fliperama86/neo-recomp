#include "p_rom.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NEO_HEADER_SIZE 0x1000u
#define NEO_HEADER_P_SIZE 0x04u
#define NEO_HEADER_S_SIZE 0x08u
#define NEO_HEADER_M_SIZE 0x0Cu
#define NEO_HEADER_V1_SIZE 0x10u
#define NEO_HEADER_V2_SIZE 0x14u
#define NEO_HEADER_C_SIZE 0x18u

typedef struct NgNeoHeader {
    uint32_t p_size;
    uint32_t s_size;
    uint32_t m_size;
    uint32_t v1_size;
    uint32_t v2_size;
    uint32_t c_size;
} NgNeoHeader;

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

static int parse_neo_header(const char *path,
                            const uint8_t *file_data,
                            uint32_t file_size,
                            NgNeoHeader *header) {
    if (file_size < NEO_HEADER_SIZE ||
        file_data[0] != 'N' || file_data[1] != 'E' ||
        file_data[2] != 'O' || file_data[3] != 1) {
        fprintf(stderr, "%s is not a supported .neo image\n", path);
        return 0;
    }

    header->p_size = read_le32(file_data + NEO_HEADER_P_SIZE);
    header->s_size = read_le32(file_data + NEO_HEADER_S_SIZE);
    header->m_size = read_le32(file_data + NEO_HEADER_M_SIZE);
    header->v1_size = read_le32(file_data + NEO_HEADER_V1_SIZE);
    header->v2_size = read_le32(file_data + NEO_HEADER_V2_SIZE);
    header->c_size = read_le32(file_data + NEO_HEADER_C_SIZE);
    uint64_t expected_size =
        (uint64_t)NEO_HEADER_SIZE + header->p_size + header->s_size +
        header->m_size + header->v1_size + header->v2_size + header->c_size;
    if (header->p_size == 0u || expected_size != file_size) {
        fprintf(stderr,
                "%s has invalid .neo region sizes "
                "(P=%u S=%u M=%u V1=%u V2=%u C=%u, file=%u expected=%llu)\n",
                path,
                header->p_size,
                header->s_size,
                header->m_size,
                header->v1_size,
                header->v2_size,
                header->c_size,
                file_size,
                (unsigned long long)expected_size);
        return 0;
    }
    return 1;
}

static int copy_region(NgNeoRomRegion *region,
                       const uint8_t *data,
                       uint32_t size) {
    region->data = NULL;
    region->size = 0;
    if (size == 0u) {
        return 1;
    }

    region->data = (uint8_t *)malloc(size);
    if (!region->data) {
        return 0;
    }
    memcpy(region->data, data, size);
    region->size = size;
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

    memset(rom, 0, sizeof(*rom));
    rom->data = image;
    rom->size = total;
    return 1;
}

int ng_neo_rom_image_load(NgNeoRomImage *image, const char *neo_path) {
    if (!image) {
        return 0;
    }
    memset(image, 0, sizeof(*image));

    uint8_t *file_data = NULL;
    uint32_t file_size = 0;
    if (!read_whole_file(neo_path, &file_data, &file_size)) {
        return 0;
    }

    NgNeoHeader header;
    if (!parse_neo_header(neo_path, file_data, file_size, &header)) {
        free(file_data);
        return 0;
    }

    const uint8_t *cursor = file_data + NEO_HEADER_SIZE;
    int ok = copy_region(&image->p, cursor, header.p_size);
    cursor += header.p_size;
    ok = ok && copy_region(&image->s, cursor, header.s_size);
    cursor += header.s_size;
    ok = ok && copy_region(&image->m, cursor, header.m_size);
    cursor += header.m_size;
    ok = ok && copy_region(&image->v1, cursor, header.v1_size);
    cursor += header.v1_size;
    ok = ok && copy_region(&image->v2, cursor, header.v2_size);
    cursor += header.v2_size;
    ok = ok && copy_region(&image->c, cursor, header.c_size);
    if (!ok) {
        ng_neo_rom_image_free(image);
        free(file_data);
        return 0;
    }
    byteswap_words(image->p.data, image->p.size);

    free(file_data);
    return 1;
}

void ng_neo_rom_image_free(NgNeoRomImage *image) {
    if (!image) {
        return;
    }
    free(image->p.data);
    free(image->s.data);
    free(image->m.data);
    free(image->v1.data);
    free(image->v2.data);
    free(image->c.data);
    memset(image, 0, sizeof(*image));
}

int ng_program_rom_load_neo(NgProgramRom *rom, const char *neo_path) {
    uint8_t *file_data = NULL;
    uint32_t file_size = 0;
    if (!read_whole_file(neo_path, &file_data, &file_size)) {
        return 0;
    }

    NgNeoHeader header;
    if (!parse_neo_header(neo_path, file_data, file_size, &header)) {
        free(file_data);
        return 0;
    }

    uint8_t *program = (uint8_t *)malloc(header.p_size);
    if (!program) {
        free(file_data);
        return 0;
    }

    memcpy(program, file_data + NEO_HEADER_SIZE, header.p_size);
    byteswap_words(program, header.p_size);
    free(file_data);

    memset(rom, 0, sizeof(*rom));
    rom->data = program;
    rom->size = header.p_size;
    return 1;
}

void ng_program_rom_free(NgProgramRom *rom) {
    free(rom->data);
    rom->data = NULL;
    rom->size = 0;
    rom->fixed_base = 0;
    rom->fixed_size = 0;
    rom->bank_window_base = 0;
    rom->bank_window_size = 0;
    rom->address_map_enabled = 0;
}

void ng_program_rom_set_address_map(NgProgramRom *rom,
                                    uint32_t fixed_base,
                                    uint32_t fixed_size,
                                    uint32_t bank_window_base,
                                    uint32_t bank_window_size) {
    if (!rom) {
        return;
    }
    rom->fixed_base = fixed_base;
    rom->fixed_size = fixed_size;
    rom->bank_window_base = bank_window_base;
    rom->bank_window_size = bank_window_size;
    rom->address_map_enabled = fixed_size != 0u || bank_window_size != 0u;
}

static int ng_program_rom_translate_addr(const NgProgramRom *rom,
                                         uint32_t addr,
                                         uint32_t *out_offset) {
    if (!rom) {
        return 0;
    }

    if (!rom->address_map_enabled) {
        if (addr >= rom->size) {
            return 0;
        }
        if (out_offset) {
            *out_offset = addr;
        }
        return 1;
    }

    if (rom->fixed_size != 0u && addr >= rom->fixed_base) {
        uint32_t rel = addr - rom->fixed_base;
        if (rel < rom->fixed_size && rel < rom->size) {
            if (out_offset) {
                *out_offset = rel;
            }
            return 1;
        }
    }

    if (rom->bank_window_size != 0u && addr >= rom->bank_window_base) {
        uint32_t rel = addr - rom->bank_window_base;
        if (rel < rom->bank_window_size) {
            uint32_t offset = rom->fixed_size + rel;
            if (offset >= rom->fixed_size && offset < rom->size) {
                if (out_offset) {
                    *out_offset = offset;
                }
                return 1;
            }
        }
    }

    return 0;
}

uint8_t ng_program_rom_read8(const NgProgramRom *rom, uint32_t addr) {
    uint32_t offset = 0;
    return ng_program_rom_translate_addr(rom, addr, &offset) ? rom->data[offset] : 0xFF;
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
    return ng_program_rom_translate_addr(rom, addr, NULL);
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
