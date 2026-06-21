#include "ngrecomp/neogeo_audio.h"
#include "p_rom.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int parse_u32(const char *text, uint32_t *out) {
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 0);
    if (!end || *end != '\0' || value > UINT32_MAX) {
        return 0;
    }
    *out = (uint32_t)value;
    return 1;
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s <game.neo> [command] [boot-cycles] [post-command-cycles]\n"
            "\n"
            "Runs the cartridge M1 Z80 program against the Neo Geo audio bus\n"
            "and YM2610 backend, then renders a short diagnostic sample block.\n"
            "\n"
            "Defaults: command=0x04 boot-cycles=200000 post-command-cycles=200000\n",
            argv0);
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 5) {
        usage(argv[0]);
        return 2;
    }

    uint32_t command = 0x04u;
    uint32_t boot_cycles = 200000u;
    uint32_t post_cycles = 200000u;
    if (argc >= 3 && !parse_u32(argv[2], &command)) {
        fprintf(stderr, "invalid command: %s\n", argv[2]);
        return 2;
    }
    if (argc >= 4 && !parse_u32(argv[3], &boot_cycles)) {
        fprintf(stderr, "invalid boot cycles: %s\n", argv[3]);
        return 2;
    }
    if (argc >= 5 && !parse_u32(argv[4], &post_cycles)) {
        fprintf(stderr, "invalid post-command cycles: %s\n", argv[4]);
        return 2;
    }

    NgNeoRomImage image;
    memset(&image, 0, sizeof(image));
    if (!ng_neo_rom_image_load(&image, argv[1])) {
        return 1;
    }

    NgNeoAudio *audio = ng_neogeo_audio_create();
    if (!audio) {
        fprintf(stderr, "failed to create audio state\n");
        ng_neo_rom_image_free(&image);
        return 1;
    }
    ng_neogeo_audio_set_roms(audio,
                             image.m.data,
                             image.m.size,
                             image.v1.data,
                             image.v1.size,
                             image.v2.data,
                             image.v2.size);
    ng_neogeo_audio_reset(audio);

    ng_neogeo_audio_advance_z80_cycles(audio, boot_cycles);
    uint32_t before_writes = ng_neogeo_audio_ym_write_count(audio);
    uint32_t before_reads = ng_neogeo_audio_ym_read_count(audio);
    printf("after boot: cycles=%llu pc=$%04X nmi_enabled=%u ym_writes=%u ym_reads=%u reply=$%02X\n",
           (unsigned long long)ng_neogeo_audio_z80_cycles(audio),
           ng_neogeo_audio_z80_pc(audio),
           ng_neogeo_audio_nmi_enabled(audio),
           before_writes,
           before_reads,
           ng_neogeo_audio_reply_latch(audio));

    ng_neogeo_audio_write_command(audio, (uint8_t)command);
    ng_neogeo_audio_advance_z80_cycles(audio, post_cycles);
    uint32_t after_writes = ng_neogeo_audio_ym_write_count(audio);
    uint32_t after_reads = ng_neogeo_audio_ym_read_count(audio);
    NgNeoAudioYmWrite last = ng_neogeo_audio_last_ym_write(audio);
    printf("after command $%02X: cycles=%llu pc=$%04X nmi_enabled=%u nmi_pending=%u nmi_serviced=%u command=$%02X reply=$%02X\n",
           command & 0xFFu,
           (unsigned long long)ng_neogeo_audio_z80_cycles(audio),
           ng_neogeo_audio_z80_pc(audio),
           ng_neogeo_audio_nmi_enabled(audio),
           ng_neogeo_audio_nmi_pending(audio),
           ng_neogeo_audio_nmi_service_count(audio),
           ng_neogeo_audio_command_latch(audio),
           ng_neogeo_audio_reply_latch(audio));
    printf("ym delta: writes=%u reads=%u last_port=%u last_addr=$%02X last_data=$%02X last_cycle=%llu\n",
           after_writes - before_writes,
           after_reads - before_reads,
           last.port,
           last.address,
           last.data,
           (unsigned long long)last.z80_cycles);
    enum { probe_frames = 4096 };
    int16_t *samples =
        (int16_t *)calloc((size_t)probe_frames * 2u, sizeof(int16_t));
    if (samples) {
        ng_neogeo_audio_generate(audio, samples, probe_frames, 48000u);
        uint32_t nonzero = 0;
        int32_t peak = 0;
        for (uint32_t i = 0; i < (uint32_t)probe_frames * 2u; ++i) {
            int32_t sample = samples[i];
            int32_t abs_sample = sample < 0 ? -sample : sample;
            if (sample != 0) {
                ++nonzero;
            }
            if (abs_sample > peak) {
                peak = abs_sample;
            }
        }
        printf("rendered audio: frames=%u rate=48000 nonzero=%u peak=%d ym_native=%u irq=%u\n",
               (uint32_t)probe_frames,
               nonzero,
               peak,
               ng_neogeo_audio_ym2610_native_sample_rate(audio),
               ng_neogeo_audio_ym2610_irq_pending(audio));
        free(samples);
    }

    ng_neogeo_audio_destroy(audio);
    ng_neo_rom_image_free(&image);
    return 0;
}
