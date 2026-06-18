#pragma once

#include <stdint.h>
#include "ngrecomp/m68k_state.h"

extern NgM68kState g_ng_m68k;

uint8_t  ng68k_read8(uint32_t addr);
uint16_t ng68k_read16(uint32_t addr);
uint32_t ng68k_read32(uint32_t addr);

void ng68k_write8(uint32_t addr, uint8_t value);
void ng68k_write16(uint32_t addr, uint16_t value);
void ng68k_write32(uint32_t addr, uint32_t value);

void ng_call_by_address(uint32_t addr);
void ng_log_dispatch_miss(uint32_t addr);
void ng_m68k_stop_until_interrupt(uint16_t sr);
