#pragma once

#include <stdint.h>

#include "ngrecomp/neogeo_runtime.h"

#ifdef __cplusplus
extern "C" {
#endif

#define NG_NEO_AUDIO_CPU_CLOCK_HZ (NG_NEO_MASTER_CLOCK_HZ / 6u)
#define NG_NEO_YM2610_CLOCK_HZ (NG_NEO_MASTER_CLOCK_HZ / 3u)
#define NG_NEO_AUDIO_WORK_RAM_BYTES 0x0800u
#define NG_NEO_AUDIO_YM_WRITE_LOG_CAPACITY 256u

typedef struct NgNeoAudio NgNeoAudio;

typedef struct NgNeoAudioYmWrite {
    uint64_t z80_cycles;
    uint8_t port;
    uint8_t address;
    uint8_t data;
} NgNeoAudioYmWrite;

typedef struct NgNeoAudioAdpcmAEvent {
    uint32_t keyon_count;
    uint32_t keyoff_count;
    uint32_t start_addr;
    uint32_t end_addr;
    uint8_t channel;
    uint8_t level;
    uint8_t total_level;
    uint8_t pan_left;
    uint8_t pan_right;
} NgNeoAudioAdpcmAEvent;

typedef struct NgNeoAudioAdpcmBEvent {
    uint32_t keyon_count;
    uint32_t reset_count;
    uint32_t start_addr;
    uint32_t end_addr;
    uint16_t delta_n;
    uint8_t level;
    uint8_t control;
    uint8_t pan_left;
    uint8_t pan_right;
    uint8_t repeat;
    uint8_t speaker_off;
} NgNeoAudioAdpcmBEvent;

NgNeoAudio *ng_neogeo_audio_create(void);
void ng_neogeo_audio_destroy(NgNeoAudio *audio);

void ng_neogeo_audio_set_roms(NgNeoAudio *audio,
                              const uint8_t *m_rom,
                              uint32_t m_rom_size,
                              const uint8_t *v1_rom,
                              uint32_t v1_rom_size,
                              const uint8_t *v2_rom,
                              uint32_t v2_rom_size);
void ng_neogeo_audio_reset(NgNeoAudio *audio);
void ng_neogeo_audio_write_command(NgNeoAudio *audio, uint8_t command);
void ng_neogeo_audio_advance_z80_cycles(NgNeoAudio *audio, uint32_t cycles);
void ng_neogeo_audio_generate(NgNeoAudio *audio,
                              int16_t *stereo_out,
                              uint32_t frames,
                              uint32_t sample_rate);
uint32_t ng_neogeo_audio_ym2610_native_sample_rate(const NgNeoAudio *audio);
uint8_t ng_neogeo_audio_ym2610_irq_pending(const NgNeoAudio *audio);

uint8_t ng_neogeo_audio_command_latch(const NgNeoAudio *audio);
uint8_t ng_neogeo_audio_reply_latch(const NgNeoAudio *audio);
uint8_t ng_neogeo_audio_nmi_enabled(const NgNeoAudio *audio);
uint8_t ng_neogeo_audio_nmi_pending(const NgNeoAudio *audio);
uint64_t ng_neogeo_audio_z80_cycles(const NgNeoAudio *audio);
uint16_t ng_neogeo_audio_z80_pc(const NgNeoAudio *audio);
uint32_t ng_neogeo_audio_command_ack_count(const NgNeoAudio *audio);
uint32_t ng_neogeo_audio_command_read_count(const NgNeoAudio *audio);
uint32_t ng_neogeo_audio_command_clear_count(const NgNeoAudio *audio);
uint32_t ng_neogeo_audio_nmi_service_count(const NgNeoAudio *audio);
uint32_t ng_neogeo_audio_ym_write_count(const NgNeoAudio *audio);
uint32_t ng_neogeo_audio_ym_read_count(const NgNeoAudio *audio);
NgNeoAudioYmWrite ng_neogeo_audio_last_ym_write(const NgNeoAudio *audio);
uint32_t ng_neogeo_audio_copy_recent_ym_writes(const NgNeoAudio *audio,
                                               NgNeoAudioYmWrite *out,
                                               uint32_t out_capacity);
NgNeoAudioAdpcmAEvent ng_neogeo_audio_last_adpcm_a_event(const NgNeoAudio *audio);
NgNeoAudioAdpcmBEvent ng_neogeo_audio_last_adpcm_b_event(const NgNeoAudio *audio);

int ng_neogeo_audio_copy_work_ram(const NgNeoAudio *audio,
                                  uint8_t *out,
                                  uint32_t out_size);

/* Test/diagnostic helpers for the Neo Geo Z80-side bus. */
uint8_t ng_neogeo_audio_debug_read_z80(const NgNeoAudio *audio, uint16_t addr);
void ng_neogeo_audio_debug_write_z80(NgNeoAudio *audio,
                                     uint16_t addr,
                                     uint8_t value);
uint8_t ng_neogeo_audio_debug_port_read(NgNeoAudio *audio, uint16_t port_addr);
void ng_neogeo_audio_debug_port_write(NgNeoAudio *audio,
                                      uint16_t port_addr,
                                      uint8_t value);

#ifdef __cplusplus
}
#endif
