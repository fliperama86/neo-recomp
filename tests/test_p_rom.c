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
    enum { header_size = 0x1000, p_size = 0x1238 };
    unsigned char *image = (unsigned char *)calloc(header_size + p_size, 1);
    if (!image) {
        return 0;
    }

    image[0] = 'N';
    image[1] = 'E';
    image[2] = 'O';
    image[3] = 1;
    write_le32(image + 4, p_size);

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

    FILE *f = fopen(path, "wb");
    if (!f) {
        free(image);
        return 0;
    }

    int ok = fwrite(image, 1, header_size + p_size, f) == header_size + p_size;
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

    ng_program_rom_free(&rom);

    const char *neo_path = "synthetic.neo";
    CHECK(write_synthetic_neo(neo_path));
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
