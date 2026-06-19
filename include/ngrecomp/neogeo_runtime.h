#pragma once

#include <stdint.h>
#include "ngrecomp/m68k_state.h"

extern NgM68kState g_ng_m68k;

#define NG_NEO_IRQACK_RESET  0x0001u
#define NG_NEO_IRQACK_TIMER  0x0002u
#define NG_NEO_IRQACK_VBLANK 0x0004u

#define NG_NEO_REG_LSPCMODE  0x003C0006u
#define NG_NEO_REG_TIMERHIGH 0x003C0008u
#define NG_NEO_REG_TIMERLOW  0x003C000Au
#define NG_NEO_REG_IRQACK    0x003C000Cu
#define NG_NEO_REG_TIMERSTOP 0x003C000Eu

#define NG_NEO_LSPCMODE_TIMER_ENABLE          0x0010u
#define NG_NEO_LSPCMODE_TIMER_RELOAD_ON_WRITE 0x0020u
#define NG_NEO_LSPCMODE_TIMER_RELOAD_ON_FRAME 0x0040u
#define NG_NEO_LSPCMODE_TIMER_RELOAD_ON_ZERO  0x0080u

uint8_t  ng68k_read8(uint32_t addr);
uint16_t ng68k_read16(uint32_t addr);
uint32_t ng68k_read32(uint32_t addr);

void ng68k_write8(uint32_t addr, uint8_t value);
void ng68k_write16(uint32_t addr, uint16_t value);
void ng68k_write32(uint32_t addr, uint32_t value);

void ng_call_by_address(uint32_t addr);
void ng_log_dispatch_miss(uint32_t addr);
void ng_m68k_stop_until_interrupt(uint16_t sr);
void ng_m68k_set_interrupt_level(uint8_t level, uint8_t vector);
void ng_m68k_clear_interrupt_level(void);
int ng_m68k_take_interrupt(uint8_t current_mask, uint8_t *level, uint8_t *vector);

void ng_neogeo_request_vblank_interrupt(void);
void ng_neogeo_request_timer_interrupt(void);
void ng_neogeo_request_reset_interrupt(void);
void ng_neogeo_ack_interrupts(uint16_t ack_mask);

void ng_neogeo_reset_runtime(void);
void ng_neogeo_begin_vblank(void);
void ng_neogeo_advance_timer(uint32_t pixel_ticks);
uint16_t ng_neogeo_lspc_mode(void);
uint16_t ng_neogeo_timer_stop(void);
uint32_t ng_neogeo_timer_reload(void);
uint32_t ng_neogeo_timer_counter(void);
