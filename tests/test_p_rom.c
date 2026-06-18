#include "p_rom.h"

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

    ng_program_rom_free(&rom);
    return 0;
}
