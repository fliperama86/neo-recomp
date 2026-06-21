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
#define NG_NEO_PALETTE_RAM_BYTES (NG_NEO_PALETTE_BANK_BYTES * NG_NEO_PALETTE_BANKS)
#define NG_NEO_BACKUP_RAM_BYTES 0x10000u
#define NG_NEO_SYSTEM_ROM_BYTES 0x20000u
#define NG_NEO_WORK_RAM_BYTES 0x10000u
#define NG_NEO_VRAM_WORDS 0x10000u
#define NG_NEO_VRAM_BYTES (NG_NEO_VRAM_WORDS * 2u)

#define NG_NEO_LSPCMODE_TIMER_ENABLE          0x0010u
#define NG_NEO_LSPCMODE_TIMER_RELOAD_ON_WRITE 0x0020u
#define NG_NEO_LSPCMODE_TIMER_RELOAD_ON_FRAME 0x0040u
#define NG_NEO_LSPCMODE_TIMER_RELOAD_ON_ZERO  0x0080u

#define NG_NEO_MASTER_CLOCK_HZ 24000000u
#define NG_NEO_MAIN_CPU_CLOCK_HZ (NG_NEO_MASTER_CLOCK_HZ / 2u)
#define NG_NEO_PIXEL_CLOCK_HZ (NG_NEO_MASTER_CLOCK_HZ / 4u)
#define NG_NEO_NTSC_PIXELS_PER_SCANLINE 384u
#define NG_NEO_NTSC_SCANLINES_PER_FRAME 264u
#define NG_NEO_NTSC_VBLANK_START_SCANLINE 0x0F0u
#define NG_NEO_CPU_CYCLES_PER_SCANLINE 768u
#define NG_NEO_CPU_CYCLES_PER_FRAME \
    (NG_NEO_CPU_CYCLES_PER_SCANLINE * NG_NEO_NTSC_SCANLINES_PER_FRAME)
#define NG_NEO_WATCHDOG_TIMEOUT_MASTER_TICKS 3244030u
#define NG_NEO_WATCHDOG_TIMEOUT_CPU_CYCLES \
    (NG_NEO_WATCHDOG_TIMEOUT_MASTER_TICKS / 2u)

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
void ng_neogeo_set_auto_scanline_interval(uint32_t interrupt_polls);
void ng_neogeo_set_watchdog_timeout_polls(uint32_t interrupt_polls);
void ng_neogeo_set_watchdog_timeout_cycles(uint32_t cpu_cycles);
void ng_neogeo_set_watchdog_reset_vector(uint32_t pc, uint32_t ssp);
void ng_neogeo_begin_vblank(void);
void ng_neogeo_advance_timer(uint32_t pixel_ticks);
void ng_neogeo_advance_scanline(void);
void ng_neogeo_advance_frame(void);
void ng_neogeo_advance_cpu_cycles(uint32_t cpu_cycles);
uint64_t ng_neogeo_cpu_cycles(void);
uint32_t ng_neogeo_scanline_cycle(void);
uint8_t ng_neogeo_cycle_timing_active(void);
uint32_t ng_neogeo_watchdog_kicks(void);
uint32_t ng_neogeo_watchdog_timeout_polls(void);
uint32_t ng_neogeo_watchdog_timeout_cycles(void);
uint32_t ng_neogeo_watchdog_last_kick_poll(void);
uint32_t ng_neogeo_watchdog_max_gap_polls(void);
uint64_t ng_neogeo_watchdog_last_kick_cycle(void);
uint64_t ng_neogeo_watchdog_max_gap_cycles(void);
uint32_t ng_neogeo_watchdog_resets(void);
uint8_t ng_neogeo_watchdog_reset_pending(void);
uint32_t ng_neogeo_watchdog_last_reset_pc(void);
uint32_t ng_neogeo_watchdog_last_reset_poll(void);
uint64_t ng_neogeo_watchdog_last_reset_cycle(void);
uint32_t ng_neogeo_last_watchdog_pc(void);
uint32_t ng_neogeo_last_watchdog_addr(void);
uint8_t ng_neogeo_last_watchdog_value(void);
uint32_t ng_neogeo_interrupt_polls(void);
uint8_t ng_neogeo_port_output(void);
uint32_t ng_neogeo_last_port_output_pc(void);
uint32_t ng_neogeo_last_port_output_addr(void);
uint8_t ng_neogeo_sound_command(void);
uint32_t ng_neogeo_last_sound_pc(void);
uint32_t ng_neogeo_last_sound_addr(void);
uint8_t ng_neogeo_sound_reply(void);
uint8_t ng_neogeo_shadow_enabled(void);
uint8_t ng_neogeo_bios_vectors_enabled(void);
uint8_t ng_neogeo_palette_bank(void);
uint32_t ng_neogeo_system_latch_writes(void);
uint32_t ng_neogeo_last_system_latch_pc(void);
uint32_t ng_neogeo_last_system_latch_addr(void);
uint8_t ng_neogeo_last_system_latch_value(void);
uint32_t ng_neogeo_bios_vector_enable_writes(void);
uint32_t ng_neogeo_bios_vector_disable_writes(void);
uint32_t ng_neogeo_last_bios_vector_pc(void);
uint32_t ng_neogeo_last_bios_vector_addr(void);
uint8_t ng_neogeo_last_bios_vector_value(void);
uint8_t ng_neogeo_board_fix_enabled(void);
uint16_t ng_neogeo_vram_addr(void);
uint16_t ng_neogeo_vram_mod(void);
uint16_t ng_neogeo_lspc_mode(void);
uint32_t ng_neogeo_last_lspc_write_pc(void);
uint32_t ng_neogeo_last_lspc_write_addr(void);
uint16_t ng_neogeo_last_lspc_write_value(void);
uint16_t ng_neogeo_timer_stop(void);
uint32_t ng_neogeo_timer_reload(void);
uint32_t ng_neogeo_timer_counter(void);
uint16_t ng_neogeo_current_scanline(void);
uint32_t ng_neogeo_frame_count(void);
uint32_t ng_neogeo_vblank_interrupts(void);
uint32_t ng_neogeo_timer_interrupts(void);
uint32_t ng_neogeo_irq_ack_writes(void);
uint32_t ng_neogeo_last_irq_ack_pc(void);
uint16_t ng_neogeo_last_irq_ack_value(void);
uint16_t ng_neogeo_irq_pending(void);
uint32_t ng_neogeo_last_interrupt_return_pc(void);
uint8_t ng_neogeo_last_interrupt_level(void);
uint8_t ng_neogeo_last_interrupt_vector(void);
uint32_t ng_neogeo_work_ram_nonzero_bytes(void);
uint32_t ng_neogeo_work_ram_checksum(void);
uint32_t ng_neogeo_palette_ram_nonzero_bytes(void);
uint32_t ng_neogeo_palette_ram_checksum(void);
uint32_t ng_neogeo_palette_write_count(void);
uint32_t ng_neogeo_palette_nonzero_write_count(void);
uint32_t ng_neogeo_palette_last_addr(void);
uint16_t ng_neogeo_palette_last_value(void);
uint8_t ng_neogeo_palette_last_bank(void);
uint32_t ng_neogeo_palette_peak_nonzero_bytes(void);
uint32_t ng_neogeo_palette_peak_checksum(void);
uint8_t ng_neogeo_backup_ram_unlocked(void);
uint32_t ng_neogeo_backup_ram_write_count(void);
uint32_t ng_neogeo_backup_ram_last_addr(void);
uint8_t ng_neogeo_backup_ram_last_value(void);
uint32_t ng_neogeo_backup_ram_nonzero_bytes(void);
uint32_t ng_neogeo_backup_ram_checksum(void);
uint32_t ng_neogeo_vram_nonzero_words(void);
uint32_t ng_neogeo_vram_checksum(void);
int ng_neogeo_copy_work_ram(uint8_t *out, uint32_t out_size);
int ng_neogeo_copy_palette_ram(uint8_t *out, uint32_t out_size);
int ng_neogeo_copy_vram(uint16_t *out_words, uint32_t out_word_count);
