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
    m_rom[0x10000u] = 0xA0u;
    m_rom[0x12000u] = 0xB1u;
    m_rom[0x18000u] = 0xA2u;
    m_rom[0x1C000u] = 0xB6u;
    m_rom[0x1E000u] = 0xCEu;
    m_rom[0x1F000u] = 0xDFu;

    NgNeoAudio *audio = ng_neogeo_audio_create();
    CHECK(audio != NULL);
    ng_neogeo_audio_set_roms(audio, m_rom, 0x20000u, NULL, 0u, NULL, 0u);
    ng_neogeo_audio_reset(audio);

    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0x0000u) == 0x10u);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0x8000u) == 0xA2u);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0xC000u) == 0xB6u);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0xE000u) == 0xCEu);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0xF000u) == 0xDFu);

    CHECK(ng_neogeo_audio_debug_port_read(audio, 0x000Bu) == 0x00u);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0x8000u) == 0xA0u);
    CHECK(ng_neogeo_audio_debug_port_read(audio, 0x010Au) == 0x00u);
    CHECK(ng_neogeo_audio_debug_read_z80(audio, 0xC000u) == 0xB1u);

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

int main(void) {
    if (test_audio_initial_banks() != 0) return 1;
    if (test_audio_ram_window() != 0) return 1;
    if (test_audio_z80_polls_command_and_writes_ym() != 0) return 1;
    if (test_audio_nmi_command_path() != 0) return 1;
    return 0;
}
