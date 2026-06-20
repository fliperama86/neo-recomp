#include "p_rom.h"
#include "neogeo_map.h"

#include <stdio.h>
#include <stdlib.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static int write_synthetic_p1(const char *path) {
    enum { image_size = 0x1238 };
    unsigned char *image = (unsigned char *)calloc(image_size, 1);
    if (!image) {
        return 0;
    }

    image[0] = 0x00;
    image[1] = 0x10;
    image[2] = 0xF3;
    image[3] = 0x00;
    image[4] = 0x00;
    image[5] = 0x00;
    image[6] = 0x12;
    image[7] = 0x34;
    image[8] = 0x12;
    image[9] = 0x34;
    image[0x100] = 'N';
    image[0x101] = 'E';
    image[0x102] = 'O';
    image[0x103] = '-';
    image[0x104] = 'G';
    image[0x105] = 'E';
    image[0x106] = 'O';
    image[0x107] = 0;
    image[0x122] = 0x4E;
    image[0x123] = 0xF9;
    image[0x124] = 0x00;
    image[0x125] = 0x00;
    image[0x126] = 0x12;
    image[0x127] = 0x34;

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(image);
        return 0;
    }

    int ok = fwrite(image, 1, image_size, f) == image_size;
    fclose(f);
    free(image);
    return ok;
}

static void write_le32(unsigned char *dst, unsigned value) {
    dst[0] = (unsigned char)(value & 0xFFu);
    dst[1] = (unsigned char)((value >> 8) & 0xFFu);
    dst[2] = (unsigned char)((value >> 16) & 0xFFu);
    dst[3] = (unsigned char)((value >> 24) & 0xFFu);
}

static int write_synthetic_neo(const char *path) {
    enum {
        header_size = 0x1000,
        p_size = 0x1238,
        s_size = 4,
        m_size = 3,
        v1_size = 5,
        v2_size = 2,
        c_size = 6
    };
    size_t image_size = header_size + p_size + s_size + m_size +
                        v1_size + v2_size + c_size;
    unsigned char *image = (unsigned char *)calloc(image_size, 1);
    if (!image) {
        return 0;
    }

    image[0] = 'N';
    image[1] = 'E';
    image[2] = 'O';
    image[3] = 1;
    write_le32(image + 4, p_size);
    write_le32(image + 8, s_size);
    write_le32(image + 0x0C, m_size);
    write_le32(image + 0x10, v1_size);
    write_le32(image + 0x14, v2_size);
    write_le32(image + 0x18, c_size);

    unsigned char *p = image + header_size;
    p[0] = 0x10;
    p[1] = 0x00;
    p[2] = 0x00;
    p[3] = 0xF3;
    p[4] = 0x00;
    p[5] = 0x00;
    p[6] = 0x34;
    p[7] = 0x12;
    p[8] = 0x34;
    p[9] = 0x12;
    p[0x100] = 'E';
    p[0x101] = 'N';
    p[0x102] = '-';
    p[0x103] = 'O';
    p[0x104] = 'E';
    p[0x105] = 'G';
    p[0x106] = 0;
    p[0x107] = 'O';
    p[0x122] = 0xF9;
    p[0x123] = 0x4E;
    p[0x124] = 0x00;
    p[0x125] = 0x00;
    p[0x126] = 0x34;
    p[0x127] = 0x12;
    unsigned char *s = p + p_size;
    s[0] = 0x53;
    s[1] = 0x10;
    s[2] = 0x53;
    s[3] = 0x11;
    unsigned char *m = s + s_size;
    m[0] = 0x4D;
    m[1] = 0x10;
    m[2] = 0x4D;
    unsigned char *v1 = m + m_size;
    v1[0] = 0xA1;
    v1[1] = 0xA2;
    v1[2] = 0xA3;
    v1[3] = 0xA4;
    v1[4] = 0xA5;
    unsigned char *v2 = v1 + v1_size;
    v2[0] = 0xB1;
    v2[1] = 0xB2;
    unsigned char *c = v2 + v2_size;
    c[0] = 0xC1;
    c[1] = 0xC2;
    c[2] = 0xC3;
    c[3] = 0xC4;
    c[4] = 0xC5;
    c[5] = 0xC6;

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(image);
        return 0;
    }

    int ok = fwrite(image, 1, image_size, f) == image_size;
    fclose(f);
    free(image);
    return ok;
}

int main(void) {
    const char *path = "synthetic_p1.bin";
    CHECK(write_synthetic_p1(path));

    NgProgramRom rom = {0};
    CHECK(ng_program_rom_load(&rom, path, NULL));
    remove(path);

    CHECK(rom.size == 0x1238u);
    CHECK(ng_program_rom_read8(&rom, 0) == 0x00u);
    CHECK(ng_program_rom_read16(&rom, 8) == 0x1234u);
    CHECK(ng_program_rom_read32(&rom, 0) == 0x0010F300u);
    CHECK(ng_program_rom_read32(&rom, 4) == 0x00001234u);
    CHECK(ng_program_rom_initial_ssp(&rom) == 0x0010F300u);
    CHECK(ng_program_rom_initial_pc(&rom) == 0x00001234u);
    CHECK(ng_program_rom_addr_is_mapped(&rom, 0x00001234u));
    CHECK(!ng_program_rom_addr_is_mapped(&rom, 0x00001238u));
    CHECK(ng_program_rom_initial_pc_is_mapped(&rom));
    CHECK(ng_program_rom_read16(&rom, 0x00001238u) == 0xFFFFu);
    CHECK(ng_program_rom_has_cart_header(&rom));
    uint32_t cart_entry = 0;
    CHECK(ng_program_rom_cart_entry(&rom, &cart_entry));
    CHECK(cart_entry == 0x00001234u);

    ng_program_rom_set_address_map(&rom, 0x000000u, 0x1000u, 0x200000u, 0x238u);
    CHECK(ng_program_rom_addr_is_mapped(&rom, 0x00000FFEu));
    CHECK(!ng_program_rom_addr_is_mapped(&rom, 0x00001000u));
    CHECK(ng_program_rom_addr_is_mapped(&rom, 0x200000u));
    CHECK(ng_program_rom_addr_is_mapped(&rom, 0x200237u));
    CHECK(!ng_program_rom_addr_is_mapped(&rom, 0x200238u));
    CHECK(ng_program_rom_read8(&rom, 0x200000u) == rom.data[0x1000u]);

    ng_program_rom_free(&rom);

    const char *neo_path = "synthetic.neo";
    CHECK(write_synthetic_neo(neo_path));

    NgNeoRomImage image = {0};
    CHECK(ng_neo_rom_image_load(&image, neo_path));
    CHECK(image.p.size == 0x1238u);
    CHECK(image.s.size == 4u);
    CHECK(image.m.size == 3u);
    CHECK(image.v1.size == 5u);
    CHECK(image.v2.size == 2u);
    CHECK(image.c.size == 6u);
    CHECK(image.p.data[0] == 0x00u);
    CHECK(image.p.data[1] == 0x10u);
    CHECK(image.p.data[8] == 0x12u);
    CHECK(image.p.data[9] == 0x34u);
    CHECK(image.s.data[0] == 0x53u);
    CHECK(image.s.data[3] == 0x11u);
    CHECK(image.m.data[0] == 0x4Du);
    CHECK(image.m.data[2] == 0x4Du);
    CHECK(image.v1.data[0] == 0xA1u);
    CHECK(image.v1.data[4] == 0xA5u);
    CHECK(image.v2.data[0] == 0xB1u);
    CHECK(image.v2.data[1] == 0xB2u);
    CHECK(image.c.data[0] == 0xC1u);
    CHECK(image.c.data[5] == 0xC6u);
    ng_neo_rom_image_free(&image);

    CHECK(ng_program_rom_load_neo(&rom, neo_path));
    remove(neo_path);

    CHECK(rom.size == 0x1238u);
    CHECK(ng_program_rom_read16(&rom, 8) == 0x1234u);
    CHECK(ng_program_rom_initial_ssp(&rom) == 0x0010F300u);
    CHECK(ng_program_rom_initial_pc(&rom) == 0x00001234u);
    CHECK(ng_program_rom_initial_pc_is_mapped(&rom));
    CHECK(ng_program_rom_has_cart_header(&rom));
    CHECK(ng_program_rom_cart_entry(&rom, &cart_entry));
    CHECK(cart_entry == 0x00001234u);

    CHECK(ng_address_region(0x00001234u) == NG_REGION_P_ROM_FIXED);
    CHECK(ng_address_region(0x0010F300u) == NG_REGION_WORK_RAM);
    CHECK(ng_address_region(0x00200000u) == NG_REGION_P_ROM_BANK);
    CHECK(ng_address_region(0x00C00402u) == NG_REGION_BIOS);
    CHECK(ng_address_region(0x00D00000u) == NG_REGION_UNKNOWN);

    ng_program_rom_free(&rom);
    return 0;
}
