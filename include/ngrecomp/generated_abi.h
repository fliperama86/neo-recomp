#pragma once

#include <stdint.h>

#include "ngrecomp/m68k_state.h"

/* ABI surface required by C emitted from the recompiler.
 *
 * Platform/runtime implementations provide these symbols. Generated code should
 * not depend on Neo Geo-specific device constants or configuration APIs.
 */

extern NgM68kState g_ng_m68k;

uint8_t  ng68k_read8(uint32_t addr);
uint16_t ng68k_read16(uint32_t addr);
uint32_t ng68k_read32(uint32_t addr);

void ng68k_write8(uint32_t addr, uint8_t value);
void ng68k_write16(uint32_t addr, uint16_t value);
void ng68k_write32(uint32_t addr, uint32_t value);

void ng_call_by_address(uint32_t addr);
void ng_log_dispatch_miss(uint32_t addr);
void ng_generated_instruction_hook(uint32_t addr);
void ng_generated_cycle_hook(uint32_t addr, uint32_t cycles);
int ng_generated_should_yield(uint32_t addr);

void ng_m68k_reset_devices(void);
void ng_m68k_stop_until_interrupt(uint16_t sr);
int ng_m68k_take_reset(uint32_t *pc, uint32_t *ssp);
int ng_m68k_take_interrupt(uint8_t current_mask, uint8_t *level, uint8_t *vector);
