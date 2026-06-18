#include "ngrecomp/neogeo_runtime.h"

#include <stdio.h>

NgM68kState g_ng_m68k;

uint8_t ng68k_read8(uint32_t addr) {
    fprintf(stderr, "ng68k_read8 miss at $%06X\n", addr & 0xFFFFFFu);
    return 0xFF;
}

uint16_t ng68k_read16(uint32_t addr) {
    uint16_t hi = ng68k_read8(addr);
    uint16_t lo = ng68k_read8(addr + 1);
    return (uint16_t)((hi << 8) | lo);
}

uint32_t ng68k_read32(uint32_t addr) {
    uint32_t hi = ng68k_read16(addr);
    uint32_t lo = ng68k_read16(addr + 2);
    return (hi << 16) | lo;
}

void ng68k_write8(uint32_t addr, uint8_t value) {
    fprintf(stderr, "ng68k_write8 miss at $%06X value=$%02X\n",
            addr & 0xFFFFFFu, value);
}

void ng68k_write16(uint32_t addr, uint16_t value) {
    ng68k_write8(addr, (uint8_t)(value >> 8));
    ng68k_write8(addr + 1, (uint8_t)value);
}

void ng68k_write32(uint32_t addr, uint32_t value) {
    ng68k_write16(addr, (uint16_t)(value >> 16));
    ng68k_write16(addr + 2, (uint16_t)value);
}

void ng_call_by_address(uint32_t addr) {
    ng_log_dispatch_miss(addr);
}

void ng_log_dispatch_miss(uint32_t addr) {
    fprintf(stderr, "dispatch miss at $%06X\n", addr & 0xFFFFFFu);
}

void ng_m68k_stop_until_interrupt(uint16_t sr) {
    fprintf(stderr, "m68k STOP until interrupt sr=$%04X\n", sr);
}
