#include "ngrecomp/neogeo_audio.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed at %s:%d: %s\n", __FILE__, __LINE__, #expr); \
        return 1; \
    } \
} while (0)

static int test_audio_initial_banks(void) {
    uint8_t *m_rom = (uint8_t *)calloc(0x20000u, 1u);
    CHECK(m_rom != NULL);
    m_rom[0x0000u] = 0x10u;
    m_rom[0x2000u] = 0xB1u;
    m_rom[0x8000u] = 0xA2u;
    m_rom[0xC000u] = 0xB6u;
    m_rom[0xE000u] = 0xCEu;
    m_rom[0xF000u] = 0xDFu;
    m_rom[0x10000u] = 0x50u;
    m_rom[0x18000u] = 0x60u;

    NgNeoAudio *audio = ng_neogeo_audio_create();
    CHECK(audio != NULL);
    ng_neogeo_audio_set_roms(audio, m_rom, 0x20000u, NULL, 0u, NULL, 0u);
    ng_neogeo_audio_reset(audio);

    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0x0000u) == 0x10u);
    /* 128 KiB M1 ROMs use MAME's 0x30000-byte ROM_RELOAD view for bank
       math: initial banks $02/$06/$0e/$1e point at the first half's
       $8000/$c000/$e000/$f000 windows, not the raw second half. */
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0x8000u) == 0xA2u);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0xC000u) == 0xB6u);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0xE000u) == 0xCEu);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0xF000u) == 0xDFu);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0xF7FFu) == 0x00u);

    CHECK(ng_neogeo_audio_debug_port_read(audio, 0x000Bu) == 0x00u);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0x8000u) == 0x10u);
    CHECK(ng_neogeo_audio_debug_port_read(audio, 0x010Au) == 0x00u);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0xC000u) == 0xB1u);
    CHECK(ng_neogeo_audio_debug_port_read(audio, 0x040Bu) == 0x00u);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0x8000u) == 0x50u);

    ng_neogeo_audio_destroy(audio);
    free(m_rom);
    return 0;
}

static int test_audio_ram_window(void) {
    NgNeoAudio *audio = ng_neogeo_audio_create();
    CHECK(audio != NULL);
    ng_neogeo_audio_debug_write_z80(audio, 0xF800u, 0x12u);
    ng_neogeo_audio_debug_write_z80(audio, 0xFFFFu, 0x34u);
    ng_neogeo_audio_debug_write_z80(audio, 0xF7FFu, 0x56u);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0xF800u) == 0x12u);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0xFFFFu) == 0x34u);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0xF7FFu) != 0x56u);
    ng_neogeo_audio_destroy(audio);
    return 0;
}

static int test_audio_z80_polls_command_and_writes_ym(void) {
    uint8_t m_rom[0x20000u];
    memset(m_rom, 0, sizeof(m_rom));

    const uint8_t program[] = {
        0xDBu, 0x00u,       /* in a,($00) */
        0xFEu, 0x00u,       /* cp $00 */
        0x28u, 0xFAu,       /* jr z,$0000 */
        0xD3u, 0x04u,       /* out ($04),a: YM2610 address A */
        0x3Eu, 0x7Fu,       /* ld a,$7f */
        0xD3u, 0x05u,       /* out ($05),a: YM2610 data A */
        0x3Eu, 0x5Au,       /* ld a,$5a */
        0xD3u, 0x0Cu,       /* out ($0c),a: 68k reply latch */
        0x18u, 0xFEu,       /* jr $0010 */
    };
    memcpy(m_rom, program, sizeof(program));

    NgNeoAudio *audio = ng_neogeo_audio_create();
    CHECK(audio != NULL);
    ng_neogeo_audio_set_roms(audio, m_rom, sizeof(m_rom), NULL, 0u, NULL, 0u);
    ng_neogeo_audio_reset(audio);
    ng_neogeo_audio_advance_z80_cycles(audio, 80u);
    CHECK(ng_neogeo_audio_ym_write_count(audio) == 0u);

    ng_neogeo_audio_write_command(audio, 0x22u);
    ng_neogeo_audio_advance_z80_cycles(audio, 220u);

    CHECK(ng_neogeo_audio_ym_write_count(audio) == 1u);
    NgNeoAudioYmWrite last = ng_neogeo_audio_last_ym_write(audio);
    CHECK(last.port == 1u);
    CHECK(last.address == 0x22u);
    CHECK(last.data == 0x7Fu);
    CHECK(ng_neogeo_audio_reply_latch(audio) == 0x5Au);

    ng_neogeo_audio_destroy(audio);
    return 0;
}

static int test_audio_nmi_command_path(void) {
    uint8_t m_rom[0x20000u];
    memset(m_rom, 0, sizeof(m_rom));

    const uint8_t main_program[] = {
        0xD3u, 0x08u,       /* out ($08),a: enable NMI */
        0x18u, 0xFEu,       /* jr $0002 */
    };
    const uint8_t nmi_handler[] = {
        0xDBu, 0x00u,       /* in a,($00): command */
        0xD3u, 0x04u,       /* out ($04),a: YM2610 address A */
        0x3Eu, 0x33u,       /* ld a,$33 */
        0xD3u, 0x05u,       /* out ($05),a: YM2610 data A */
        0xD3u, 0x00u,       /* out ($00),a: clear sound latch */
        0xC9u,              /* ret */
    };
    memcpy(m_rom, main_program, sizeof(main_program));
    memcpy(m_rom + 0x0066u, nmi_handler, sizeof(nmi_handler));

    NgNeoAudio *audio = ng_neogeo_audio_create();
    CHECK(audio != NULL);
    ng_neogeo_audio_set_roms(audio, m_rom, sizeof(m_rom), NULL, 0u, NULL, 0u);
    ng_neogeo_audio_reset(audio);
    ng_neogeo_audio_advance_z80_cycles(audio, 40u);
    CHECK(ng_neogeo_audio_nmi_enabled(audio) == 1u);

    ng_neogeo_audio_write_command(audio, 0x42u);
    CHECK(ng_neogeo_audio_nmi_pending(audio) == 1u);
    ng_neogeo_audio_advance_z80_cycles(audio, 240u);

    CHECK(ng_neogeo_audio_nmi_service_count(audio) == 1u);
    CHECK(ng_neogeo_audio_ym_write_count(audio) == 1u);
    NgNeoAudioYmWrite last = ng_neogeo_audio_last_ym_write(audio);
    CHECK(last.port == 1u);
    CHECK(last.address == 0x42u);
    CHECK(last.data == 0x33u);
    CHECK(ng_neogeo_audio_command_latch(audio) == 0x00u);

    ng_neogeo_audio_destroy(audio);
    return 0;
}


static int test_audio_pending_command_overwrites_before_nmi_service(void) {
    uint8_t m_rom[0x20000u];
    memset(m_rom, 0, sizeof(m_rom));

    const uint8_t main_program[] = {
        0xD3u, 0x08u,       /* out ($08),a: enable NMI */
        0x18u, 0xFEu,       /* jr $0002 */
    };
    const uint8_t nmi_handler[] = {
        0xDBu, 0x00u,       /* in a,($00): command */
        0xD3u, 0x04u,       /* out ($04),a: YM2610 address A */
        0x3Eu, 0x44u,       /* ld a,$44 */
        0xD3u, 0x05u,       /* out ($05),a: YM2610 data A */
        0xD3u, 0x00u,       /* out ($00),a: clear sound latch */
        0xC9u,              /* ret */
    };
    memcpy(m_rom, main_program, sizeof(main_program));
    memcpy(m_rom + 0x0066u, nmi_handler, sizeof(nmi_handler));

    NgNeoAudio *audio = ng_neogeo_audio_create();
    CHECK(audio != NULL);
    ng_neogeo_audio_set_roms(audio, m_rom, sizeof(m_rom), NULL, 0u, NULL, 0u);
    ng_neogeo_audio_reset(audio);
    ng_neogeo_audio_advance_z80_cycles(audio, 40u);
    CHECK(ng_neogeo_audio_nmi_enabled(audio) == 1u);

    /* MAME's generic_latch_8 is a single pending byte, not a FIFO.  A second
       68000 sound write before the Z80 reads port $00 overwrites the first
       latched command and should still produce only one NMI service. */
    ng_neogeo_audio_write_command(audio, 0x11u);
    ng_neogeo_audio_write_command(audio, 0x22u);
    ng_neogeo_audio_advance_z80_cycles(audio, 240u);

    CHECK(ng_neogeo_audio_nmi_service_count(audio) == 1u);
    CHECK(ng_neogeo_audio_ym_write_count(audio) == 1u);
    NgNeoAudioYmWrite last = ng_neogeo_audio_last_ym_write(audio);
    CHECK(last.port == 1u);
    CHECK(last.address == 0x22u);
    CHECK(last.data == 0x44u);
    CHECK(ng_neogeo_audio_command_latch(audio) == 0x00u);

    ng_neogeo_audio_destroy(audio);
    return 0;
}

static int test_audio_command_ack_counter_tracks_read_and_clear(void) {
    NgNeoAudio *audio = ng_neogeo_audio_create();
    CHECK(audio != NULL);

    CHECK(ng_neogeo_audio_command_ack_count(audio) == 0u);
    ng_neogeo_audio_write_command(audio, 0x77u);
    CHECK(ng_neogeo_audio_nmi_pending(audio) == 1u);
    CHECK(ng_neogeo_audio_command_ack_count(audio) == 0u);

    CHECK(ng_neogeo_audio_debug_port_read(audio, 0x0000u) == 0x77u);
    CHECK(ng_neogeo_audio_nmi_pending(audio) == 0u);
    CHECK(ng_neogeo_audio_command_ack_count(audio) == 1u);

    ng_neogeo_audio_debug_port_write(audio, 0x0000u, 0x00u);
    CHECK(ng_neogeo_audio_command_latch(audio) == 0x00u);
    CHECK(ng_neogeo_audio_command_ack_count(audio) == 2u);

    ng_neogeo_audio_destroy(audio);
    return 0;
}

static int test_audio_advance_until_command_ack_stops_at_latch_read(void) {
    uint8_t m_rom[0x20000u];
    memset(m_rom, 0, sizeof(m_rom));

    const uint8_t main_program[] = {
        0xD3u, 0x08u,       /* out ($08),a: enable NMI */
        0x18u, 0xFEu,       /* jr $0002 */
    };
    const uint8_t nmi_handler[] = {
        0xDBu, 0x00u,       /* in a,($00): acknowledge command */
        0xD3u, 0x04u,       /* out ($04),a: YM2610 address A */
        0x3Eu, 0x55u,       /* ld a,$55 */
        0xD3u, 0x05u,       /* out ($05),a: YM2610 data A */
        0xD3u, 0x00u,       /* out ($00),a: clear sound latch */
        0xC9u,              /* ret */
    };
    memcpy(m_rom, main_program, sizeof(main_program));
    memcpy(m_rom + 0x0066u, nmi_handler, sizeof(nmi_handler));

    NgNeoAudio *audio = ng_neogeo_audio_create();
    CHECK(audio != NULL);
    ng_neogeo_audio_set_roms(audio, m_rom, sizeof(m_rom), NULL, 0u, NULL, 0u);
    ng_neogeo_audio_reset(audio);
    ng_neogeo_audio_advance_z80_cycles(audio, 40u);
    CHECK(ng_neogeo_audio_nmi_enabled(audio) == 1u);

    uint32_t initial_ack = ng_neogeo_audio_command_ack_count(audio);
    ng_neogeo_audio_write_command(audio, 0x33u);
    uint32_t advanced =
        ng_neogeo_audio_advance_z80_cycles_until_command_ack(
            audio,
            240u,
            initial_ack);

    CHECK(advanced > 0u);
    CHECK(ng_neogeo_audio_command_ack_count(audio) == initial_ack + 1u);
    CHECK(ng_neogeo_audio_ym_write_count(audio) == 0u);

    ng_neogeo_audio_advance_z80_cycles(audio, 240u);
    CHECK(ng_neogeo_audio_ym_write_count(audio) == 1u);
    NgNeoAudioYmWrite last = ng_neogeo_audio_last_ym_write(audio);
    CHECK(last.address == 0x33u);
    CHECK(last.data == 0x55u);

    ng_neogeo_audio_destroy(audio);
    return 0;
}

static int test_audio_ym2610_generates_samples(void) {
    NgNeoAudio *audio = ng_neogeo_audio_create();
    CHECK(audio != NULL);
    ng_neogeo_audio_reset(audio);

    /* Program a simple SSG tone through the YM2610 A-port. */
    ng_neogeo_audio_debug_port_write(audio, 0x0004u, 0x00u);
    ng_neogeo_audio_debug_port_write(audio, 0x0005u, 0x10u);
    ng_neogeo_audio_debug_port_write(audio, 0x0004u, 0x01u);
    ng_neogeo_audio_debug_port_write(audio, 0x0005u, 0x00u);
    ng_neogeo_audio_debug_port_write(audio, 0x0004u, 0x07u);
    ng_neogeo_audio_debug_port_write(audio, 0x0005u, 0x3Eu);
    ng_neogeo_audio_debug_port_write(audio, 0x0004u, 0x08u);
    ng_neogeo_audio_debug_port_write(audio, 0x0005u, 0x0Fu);

    int16_t samples[1024 * 2];
    memset(samples, 0, sizeof(samples));
    ng_neogeo_audio_generate(audio, samples, 1024u, 48000u);

    uint32_t nonzero = 0;
    for (uint32_t i = 0; i < 1024u * 2u; ++i) {
        if (samples[i] != 0) {
            ++nonzero;
        }
    }
    CHECK(nonzero != 0u);
    CHECK(ng_neogeo_audio_ym2610_native_sample_rate(audio) ==
          NG_NEO_YM2610_CLOCK_HZ / 144u);

    ng_neogeo_audio_destroy(audio);
    return 0;
}

static int test_audio_ym2610_combines_v_rom_chunks_for_adpcm(void) {
    uint8_t v1[0x100u];
    uint8_t v2[0x100u];
    memset(v1, 0, sizeof(v1));
    memset(v2, 0, sizeof(v2));
    v2[0] = 0x77u;
    v2[1] = 0x77u;

    NgNeoAudio *audio = ng_neogeo_audio_create();
    CHECK(audio != NULL);
    ng_neogeo_audio_set_roms(audio, NULL, 0u, v1, sizeof(v1), v2, sizeof(v2));
    ng_neogeo_audio_reset(audio);

    /* Program ADPCM-A channel 0 to start at $0100, i.e. the first byte after
       v1.  If v1/v2 are treated as separate A/B regions this reads silence;
       Metal Slug's MAME map treats 201-v1/201-v2 as one ADPCM region. */
    ng_neogeo_audio_debug_port_write(audio, 0x0006u, 0x01u);
    ng_neogeo_audio_debug_port_write(audio, 0x0007u, 0x3Fu); /* total level */
    ng_neogeo_audio_debug_port_write(audio, 0x0006u, 0x08u);
    ng_neogeo_audio_debug_port_write(audio, 0x0007u, 0xDFu); /* pan L/R, max */
    ng_neogeo_audio_debug_port_write(audio, 0x0006u, 0x10u);
    ng_neogeo_audio_debug_port_write(audio, 0x0007u, 0x01u); /* start low */
    ng_neogeo_audio_debug_port_write(audio, 0x0006u, 0x18u);
    ng_neogeo_audio_debug_port_write(audio, 0x0007u, 0x00u); /* start high */
    ng_neogeo_audio_debug_port_write(audio, 0x0006u, 0x20u);
    ng_neogeo_audio_debug_port_write(audio, 0x0007u, 0x01u); /* end low */
    ng_neogeo_audio_debug_port_write(audio, 0x0006u, 0x28u);
    ng_neogeo_audio_debug_port_write(audio, 0x0007u, 0x00u); /* end high */
    ng_neogeo_audio_debug_port_write(audio, 0x0006u, 0x00u);
    ng_neogeo_audio_debug_port_write(audio, 0x0007u, 0x01u); /* key on ch0 */

    NgNeoAudioAdpcmAEvent event =
        ng_neogeo_audio_last_adpcm_a_event(audio);
    CHECK(event.keyon_count == 1u);
    CHECK(event.keyoff_count == 0u);
    CHECK(event.channel == 0u);
    CHECK(event.start_addr == 0x000100u);
    CHECK(event.end_addr == 0x000100u);
    CHECK(event.level == 0x1Fu);
    CHECK(event.total_level == 0x3Fu);
    CHECK(event.pan_left == 1u);
    CHECK(event.pan_right == 1u);

    int16_t samples[512 * 2];
    memset(samples, 0, sizeof(samples));
    ng_neogeo_audio_generate(audio, samples, 512u, 48000u);

    uint32_t nonzero = 0;
    for (uint32_t i = 0; i < 512u * 2u; ++i) {
        if (samples[i] != 0) {
            ++nonzero;
        }
    }
    CHECK(nonzero != 0u);

    ng_neogeo_audio_debug_port_write(audio, 0x0006u, 0x08u);
    ng_neogeo_audio_debug_port_write(audio, 0x0007u, 0x80u);
    event = ng_neogeo_audio_last_adpcm_a_event(audio);
    CHECK(event.keyon_count == 1u);
    CHECK(event.level == 0x00u);
    CHECK(event.pan_left == 1u);
    CHECK(event.pan_right == 0u);

    ng_neogeo_audio_debug_port_write(audio, 0x0006u, 0x00u);
    ng_neogeo_audio_debug_port_write(audio, 0x0007u, 0x81u); /* key off ch0 */
    event = ng_neogeo_audio_last_adpcm_a_event(audio);
    CHECK(event.keyoff_count == 1u);

    ng_neogeo_audio_destroy(audio);
    return 0;
}

static int test_audio_tracks_adpcm_b_keyon_diagnostics(void) {
    NgNeoAudio *audio = ng_neogeo_audio_create();
    CHECK(audio != NULL);
    ng_neogeo_audio_reset(audio);

    ng_neogeo_audio_debug_port_write(audio, 0x0004u, 0x11u);
    ng_neogeo_audio_debug_port_write(audio, 0x0005u, 0xC0u);
    ng_neogeo_audio_debug_port_write(audio, 0x0004u, 0x12u);
    ng_neogeo_audio_debug_port_write(audio, 0x0005u, 0x43u);
    ng_neogeo_audio_debug_port_write(audio, 0x0004u, 0x13u);
    ng_neogeo_audio_debug_port_write(audio, 0x0005u, 0x59u);
    ng_neogeo_audio_debug_port_write(audio, 0x0004u, 0x14u);
    ng_neogeo_audio_debug_port_write(audio, 0x0005u, 0xA8u);
    ng_neogeo_audio_debug_port_write(audio, 0x0004u, 0x15u);
    ng_neogeo_audio_debug_port_write(audio, 0x0005u, 0x59u);
    ng_neogeo_audio_debug_port_write(audio, 0x0004u, 0x19u);
    ng_neogeo_audio_debug_port_write(audio, 0x0005u, 0x83u);
    ng_neogeo_audio_debug_port_write(audio, 0x0004u, 0x1Au);
    ng_neogeo_audio_debug_port_write(audio, 0x0005u, 0xA0u);
    ng_neogeo_audio_debug_port_write(audio, 0x0004u, 0x1Bu);
    ng_neogeo_audio_debug_port_write(audio, 0x0005u, 0x8Fu);
    ng_neogeo_audio_debug_port_write(audio, 0x0004u, 0x10u);
    ng_neogeo_audio_debug_port_write(audio, 0x0005u, 0x80u);

    NgNeoAudioAdpcmBEvent event =
        ng_neogeo_audio_last_adpcm_b_event(audio);
    CHECK(event.keyon_count == 1u);
    CHECK(event.start_addr == 0x594300u);
    CHECK(event.end_addr == 0x59A800u);
    CHECK(event.delta_n == 0xA083u);
    CHECK(event.level == 0x8Fu);
    CHECK(event.control == 0xA0u);
    CHECK(event.pan_left == 1u);
    CHECK(event.pan_right == 1u);
    CHECK(event.repeat == 0u);

    ng_neogeo_audio_debug_port_write(audio, 0x0004u, 0x10u);
    ng_neogeo_audio_debug_port_write(audio, 0x0005u, 0x01u);
    event = ng_neogeo_audio_last_adpcm_b_event(audio);
    CHECK(event.reset_count == 1u);

    ng_neogeo_audio_destroy(audio);
    return 0;
}

int main(void) {
    if (test_audio_initial_banks() != 0) return 1;
    if (test_audio_ram_window() != 0) return 1;
    if (test_audio_z80_polls_command_and_writes_ym() != 0) return 1;
    if (test_audio_nmi_command_path() != 0) return 1;
    if (test_audio_pending_command_overwrites_before_nmi_service() != 0) return 1;
    if (test_audio_command_ack_counter_tracks_read_and_clear() != 0) return 1;
    if (test_audio_advance_until_command_ack_stops_at_latch_read() != 0) return 1;
    if (test_audio_ym2610_generates_samples() != 0) return 1;
    if (test_audio_ym2610_combines_v_rom_chunks_for_adpcm() != 0) return 1;
    if (test_audio_tracks_adpcm_b_keyon_diagnostics() != 0) return 1;
    return 0;
}
