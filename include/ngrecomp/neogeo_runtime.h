#pragma once

#include <stdint.h>
#include "ngrecomp/generated_abi.h"

typedef int (*NgExternalDispatchHandler)(uint32_t addr);

#define NG_NEO_IRQACK_RESET  0x0001u
#define NG_NEO_IRQACK_TIMER  0x0002u
#define NG_NEO_IRQACK_VBLANK 0x0004u

#define NG_NEO_REG_DIPSW     0x00300001u
#define NG_NEO_REG_LSPCMODE  0x003C0006u
#define NG_NEO_REG_TIMERHIGH 0x003C0008u
#define NG_NEO_REG_TIMERLOW  0x003C000Au
#define NG_NEO_REG_IRQACK    0x003C000Cu
#define NG_NEO_REG_TIMERSTOP 0x003C000Eu
#define NG_NEO_REG_PALBANK1  0x003A000Fu
#define NG_NEO_REG_PALBANK0  0x003A001Fu
#define NG_NEO_REG_SRAMLOCK  0x003A000Du
#define NG_NEO_REG_SRAMUNLOCK 0x003A001Du

#define NG_NEO_PALETTE_BANK_BYTES 0x2000u
#define NG_NEO_PALETTE_BANKS 2u
#define NG_NEO_BACKUP_RAM_BYTES 0x10000u
#define NG_NEO_SYSTEM_ROM_BYTES 0x20000u

#define NG_NEO_LSPCMODE_TIMER_ENABLE          0x0010u
#define NG_NEO_LSPCMODE_TIMER_RELOAD_ON_WRITE 0x0020u
#define NG_NEO_LSPCMODE_TIMER_RELOAD_ON_FRAME 0x0040u
#define NG_NEO_LSPCMODE_TIMER_RELOAD_ON_ZERO  0x0080u

#define NG_NEO_NTSC_PIXELS_PER_SCANLINE 384u
#define NG_NEO_NTSC_SCANLINES_PER_FRAME 264u

void ng_neogeo_set_external_dispatch(NgExternalDispatchHandler handler);
void ng_m68k_set_interrupt_level(uint8_t level, uint8_t vector);
void ng_m68k_clear_interrupt_level(void);

void ng_neogeo_request_vblank_interrupt(void);
void ng_neogeo_request_timer_interrupt(void);
void ng_neogeo_request_reset_interrupt(void);
void ng_neogeo_ack_interrupts(uint16_t ack_mask);

void ng_neogeo_set_program_rom(const uint8_t *data, uint32_t size);
void ng_neogeo_set_system_rom(const uint8_t *data, uint32_t size);
void ng_neogeo_reset_runtime(void);
void ng_neogeo_begin_vblank(void);
void ng_neogeo_advance_timer(uint32_t pixel_ticks);
void ng_neogeo_advance_scanline(void);
void ng_neogeo_advance_frame(void);
uint32_t ng_neogeo_watchdog_kicks(void);
uint16_t ng_neogeo_lspc_mode(void);
uint16_t ng_neogeo_timer_stop(void);
uint32_t ng_neogeo_timer_reload(void);
uint32_t ng_neogeo_timer_counter(void);
uint16_t ng_neogeo_current_scanline(void);
