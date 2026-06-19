#pragma once

#include <stdint.h>
#include "ngrecomp/generated_abi.h"

typedef int (*NgExternalDispatchHandler)(uint32_t addr);

#define NG_NEO_IRQACK_RESET  0x0001u
#define NG_NEO_IRQACK_TIMER  0x0002u
#define NG_NEO_IRQACK_VBLANK 0x0004u

#define NG_NEO_REG_P1CNT     0x00300000u
#define NG_NEO_REG_DIPSW     0x00300001u
#define NG_NEO_REG_SOUND     0x00320000u
#define NG_NEO_REG_P2CNT     0x00340000u
#define NG_NEO_REG_STATUS_B  0x00380000u
#define NG_NEO_REG_POUTPUT   0x00380001u
#define NG_NEO_REG_NOSHADOW  0x003A0001u
#define NG_NEO_REG_SWPBIOS   0x003A0003u
#define NG_NEO_REG_BRDFIX    0x003A000Bu
#define NG_NEO_REG_LSPCMODE  0x003C0006u
#define NG_NEO_REG_VRAMADDR  0x003C0000u
#define NG_NEO_REG_VRAMRW    0x003C0002u
#define NG_NEO_REG_VRAMMOD   0x003C0004u
#define NG_NEO_REG_TIMERHIGH 0x003C0008u
#define NG_NEO_REG_TIMERLOW  0x003C000Au
#define NG_NEO_REG_IRQACK    0x003C000Cu
#define NG_NEO_REG_TIMERSTOP 0x003C000Eu
#define NG_NEO_REG_SHADOW    0x003A0011u
#define NG_NEO_REG_SWPROM    0x003A0013u
#define NG_NEO_REG_CRTFIX    0x003A001Bu
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
void ng_neogeo_set_auto_vblank_interval(uint32_t interrupt_polls);
void ng_neogeo_begin_vblank(void);
void ng_neogeo_advance_timer(uint32_t pixel_ticks);
void ng_neogeo_advance_scanline(void);
void ng_neogeo_advance_frame(void);
uint32_t ng_neogeo_watchdog_kicks(void);
uint32_t ng_neogeo_interrupt_polls(void);
uint8_t ng_neogeo_port_output(void);
uint8_t ng_neogeo_sound_command(void);
uint8_t ng_neogeo_sound_reply(void);
uint8_t ng_neogeo_shadow_enabled(void);
uint8_t ng_neogeo_bios_vectors_enabled(void);
uint8_t ng_neogeo_board_fix_enabled(void);
uint16_t ng_neogeo_vram_addr(void);
uint16_t ng_neogeo_vram_mod(void);
uint16_t ng_neogeo_lspc_mode(void);
uint16_t ng_neogeo_timer_stop(void);
uint32_t ng_neogeo_timer_reload(void);
uint32_t ng_neogeo_timer_counter(void);
uint16_t ng_neogeo_current_scanline(void);
uint32_t ng_neogeo_vblank_interrupts(void);
uint32_t ng_neogeo_timer_interrupts(void);
uint32_t ng_neogeo_irq_ack_writes(void);
uint16_t ng_neogeo_irq_pending(void);
uint32_t ng_neogeo_work_ram_nonzero_bytes(void);
uint32_t ng_neogeo_work_ram_checksum(void);
uint32_t ng_neogeo_vram_nonzero_words(void);
uint32_t ng_neogeo_vram_checksum(void);
