#include "ngrecomp/neogeo_runtime.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

int ng_generated_smoke_run(const char *neo_path);
int ng_generated_smoke_run_with_bios(const char *neo_path,
                                     const char *bios_path);

static uint32_t g_called_addr;
static uint16_t g_seen_cart_jump;
static uint32_t g_seen_bios_long;
static uint32_t g_seen_ssp;
static uint32_t g_seen_sp;
static uint16_t g_seen_sr;

void ng_generated_call(uint32_t addr) {
    g_called_addr = addr & 0x00FFFFFFu;
    g_seen_cart_jump = ng68k_read16(0x00000122u);
    g_seen_bios_long = ng68k_read32(0x00C00000u);
    g_seen_ssp = g_ng_m68k.ssp;
    g_seen_sp = g_ng_m68k.a[7];
    g_seen_sr = g_ng_m68k.sr;
}

static void put32be(uint8_t *data, uint32_t offset, uint32_t value) {
    data[offset] = (uint8_t)(value >> 24);
    data[offset + 1u] = (uint8_t)(value >> 16);
    data[offset + 2u] = (uint8_t)(value >> 8);
    data[offset + 3u] = (uint8_t)value;
}

static int write_synthetic_neo(const char *path) {
    enum { NEO_HEADER_SIZE = 0x1000u, P_SIZE = 0x400u };
    uint8_t header[NEO_HEADER_SIZE];
    uint8_t program[P_SIZE];
    memset(header, 0, sizeof(header));
    memset(program, 0xFF, sizeof(program));

    header[0] = 'N';
    header[1] = 'E';
    header[2] = 'O';
    header[3] = 1u;
    header[4] = (uint8_t)P_SIZE;
    header[5] = (uint8_t)(P_SIZE >> 8);
    header[6] = (uint8_t)(P_SIZE >> 16);
    header[7] = (uint8_t)(P_SIZE >> 24);

    put32be(program, 0x000u, 0x0010F300u);
    memcpy(program + 0x100u, "NEO-GEO", 7u);
    program[0x107u] = 0;
    program[0x122u] = 0x4Eu;
    program[0x123u] = 0xF9u;
    put32be(program, 0x124u, 0x00000200u);

    FILE *out = fopen(path, "wb");
    if (!out) {
        return 0;
    }
    if (fwrite(header, 1, sizeof(header), out) != sizeof(header)) {
        fclose(out);
        return 0;
    }
    for (uint32_t i = 0; i < P_SIZE; i += 2u) {
        uint8_t swapped[2] = { program[i + 1u], program[i] };
        if (fwrite(swapped, 1, sizeof(swapped), out) != sizeof(swapped)) {
            fclose(out);
            return 0;
        }
    }
    return fclose(out) == 0;
}

static int write_synthetic_bios(const char *path) {
    const uint8_t bios[] = {
        0xB2u, 0xA1u, 0xD4u, 0xC3u,
        0xF6u, 0xE5u, 0x18u, 0x07u,
    };
    FILE *out = fopen(path, "wb");
    if (!out) {
        return 0;
    }
    if (fwrite(bios, 1, sizeof(bios), out) != sizeof(bios)) {
        fclose(out);
        return 0;
    }
    return fclose(out) == 0;
}

int main(void) {
    const char *path = "generated-smoke-harness-test.neo";
    const char *bios_path = "generated-smoke-harness-test.sp1";
    remove(path);
    remove(bios_path);
    CHECK(write_synthetic_neo(path));
    CHECK(write_synthetic_bios(bios_path));

    CHECK(ng_generated_smoke_run(path) == 0);
    CHECK(g_called_addr == 0x00000200u);
    CHECK(g_seen_cart_jump == 0x4EF9u);
    CHECK(g_seen_bios_long == 0xFFFFFFFFu);
    CHECK(g_seen_ssp == 0x0010F300u);
    CHECK(g_seen_sp == 0x0010F300u);
    CHECK(g_seen_sr == 0x2700u);

    CHECK(ng_generated_smoke_run_with_bios(path, bios_path) == 0);
    CHECK(g_called_addr == 0x00000200u);
    CHECK(g_seen_cart_jump == 0x4EF9u);
    CHECK(g_seen_bios_long == 0xA1B2C3D4u);
    CHECK(g_seen_ssp == 0x0010F300u);
    CHECK(g_seen_sp == 0x0010F300u);
    CHECK(g_seen_sr == 0x2700u);
    CHECK(ng68k_read32(0x00C00000u) == 0xFFFFFFFFu);

    remove(path);
    remove(bios_path);
    return 0;
}
