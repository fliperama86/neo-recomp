#include "ngrecomp/neogeo_runtime.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

#define BUS_SIZE 0x2000u

NgM68kState g_ng_m68k;

static uint8_t g_bus[BUS_SIZE];
static uint32_t g_dispatch_miss_count;
static uint32_t g_last_dispatch_miss;

void ng_generated_call(uint32_t addr);

uint8_t ng68k_read8(uint32_t addr) {
    return addr < BUS_SIZE ? g_bus[addr] : 0xFFu;
}

uint16_t ng68k_read16(uint32_t addr) {
    uint16_t hi = ng68k_read8(addr);
    uint16_t lo = ng68k_read8(addr + 1u);
    return (uint16_t)((hi << 8) | lo);
}

uint32_t ng68k_read32(uint32_t addr) {
    uint32_t hi = ng68k_read16(addr);
    uint32_t lo = ng68k_read16(addr + 2u);
    return (hi << 16) | lo;
}

void ng68k_write8(uint32_t addr, uint8_t value) {
    if (addr < BUS_SIZE) {
        g_bus[addr] = value;
    }
}

void ng68k_write16(uint32_t addr, uint16_t value) {
    ng68k_write8(addr, (uint8_t)(value >> 8));
    ng68k_write8(addr + 1u, (uint8_t)value);
}

void ng68k_write32(uint32_t addr, uint32_t value) {
    ng68k_write16(addr, (uint16_t)(value >> 16));
    ng68k_write16(addr + 2u, (uint16_t)value);
}

void ng_call_by_address(uint32_t addr) {
    ng_generated_call(addr);
}

void ng_log_dispatch_miss(uint32_t addr) {
    ++g_dispatch_miss_count;
    g_last_dispatch_miss = addr & 0x00FFFFFFu;
}

int main(void) {
    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_dispatch_miss_count = 0;
    g_last_dispatch_miss = 0;

    ng_generated_call(0x00000000u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.d[0] == 10u);
    CHECK(ng68k_read16(0x1000u) == 0x1234u);
    CHECK(ng68k_read32(0x1004u) == 0x00000038u);

    ng_generated_call(0x00DEADu);
    CHECK(g_dispatch_miss_count == 1);
    CHECK(g_last_dispatch_miss == 0x00DEADu);

    return 0;
}
