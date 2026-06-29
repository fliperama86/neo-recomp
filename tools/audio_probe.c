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

static int parse_command_list(const char *text,
                              uint8_t *commands,
                              uint32_t capacity,
                              uint32_t *out_count) {
    if (!text || !commands || capacity == 0u || !out_count) {
        return 0;
    }

    const char *cursor = text;
    uint32_t count = 0;
    while (*cursor != '\0') {
        if (count >= capacity) {
            return 0;
        }
        char *end = NULL;
        unsigned long value = strtoul(cursor, &end, 0);
        if (end == cursor || value > 0xFFul) {
            return 0;
        }
        commands[count++] = (uint8_t)value;
        if (*end == '\0') {
            break;
        }
        if (*end != ',') {
            return 0;
        }
        cursor = end + 1;
    }
    if (count == 0u) {
        return 0;
    }
    *out_count = count;
    return 1;
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s <game.neo> [command[,command...]] [boot-cycles] [post-command-cycles]\n"
            "\n"
            "Runs the cartridge M1 Z80 program against the Neo Geo audio bus\n"
            "and YM2610 backend, then renders a short diagnostic sample block.\n"
            "\n"
            "Defaults: command=0xD5 boot-cycles=200000 post-command-cycles=200000\n",
            argv0);
}

int main(int argc, char **argv) {
    if (argc < 2 || argc > 5) {
        usage(argv[0]);
        return 2;
    }

    uint8_t commands[64];
    uint32_t command_count = 1u;
    commands[0] = 0xD5u;
    uint32_t boot_cycles = 200000u;
    uint32_t post_cycles = 200000u;
    if (argc >= 3 && !parse_command_list(argv[2],
                                          commands,
                                          (uint32_t)(sizeof(commands) /
                                                     sizeof(commands[0])),
                                          &command_count)) {
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

    enum { command_quantum_us = 50u };
    uint32_t command_quantum_cycles =
        (uint32_t)(((uint64_t)NG_NEO_AUDIO_CPU_CLOCK_HZ *
                        (uint64_t)command_quantum_us +
                    999999ull) /
                   1000000ull);
    if (command_quantum_cycles == 0u) {
        command_quantum_cycles = 1u;
    }
    for (uint32_t i = 0; i < command_count; ++i) {
        ng_neogeo_audio_write_command(audio, commands[i]);
        ng_neogeo_audio_advance_z80_cycles(audio, command_quantum_cycles);
    }
    ng_neogeo_audio_advance_z80_cycles(audio, post_cycles);
    uint32_t after_writes = ng_neogeo_audio_ym_write_count(audio);
    uint32_t after_reads = ng_neogeo_audio_ym_read_count(audio);
    NgNeoAudioYmWrite last = ng_neogeo_audio_last_ym_write(audio);
    printf("after command");
    for (uint32_t i = 0; i < command_count; ++i) {
        printf(" $%02X", commands[i]);
    }
    printf(": cycles=%llu pc=$%04X nmi_enabled=%u nmi_pending=%u nmi_serviced=%u command=$%02X reply=$%02X\n",
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
    /* Optional coupled capture: drive the M1 directly (no 68000) for a fixed
       span of emulated time, generating audio in lockstep, and dump a WAV.
       This isolates whether the M1 sound driver sustains/loops a track on its
       own or whether the live-host stall is a 68000 progression issue. */
    const char *wav_path = getenv("NG_PROBE_WAV");
    if (wav_path && *wav_path) {
        double seconds = 10.0;
        const char *secs_env = getenv("NG_PROBE_SECONDS");
        if (secs_env && *secs_env) {
            seconds = atof(secs_env);
        }
        FILE *wf = fopen(wav_path, "wb");
        if (wf) {
            const uint32_t rate = 48000u;
            uint32_t total_frames = (uint32_t)(seconds * rate);
            uint8_t hdr[44];
            memset(hdr, 0, sizeof(hdr));
            memcpy(hdr, "RIFF", 4);
            memcpy(hdr + 8, "WAVEfmt ", 8);
            uint32_t fmt_len = 16, data_bytes = total_frames * 4u;
            uint32_t riff = 36u + data_bytes, byte_rate = rate * 4u;
            uint16_t pcm = 1, chn = 2, align = 4, bits = 16;
            memcpy(hdr + 4, &riff, 4);
            memcpy(hdr + 16, &fmt_len, 4);
            memcpy(hdr + 20, &pcm, 2);
            memcpy(hdr + 22, &chn, 2);
            memcpy(hdr + 24, &rate, 4);
            memcpy(hdr + 28, &byte_rate, 4);
            memcpy(hdr + 32, &align, 2);
            memcpy(hdr + 34, &bits, 2);
            memcpy(hdr + 36, "data", 4);
            memcpy(hdr + 40, &data_bytes, 4);
            fwrite(hdr, 1, sizeof(hdr), wf);

            /* Optional in-loop command injection at realistic spacing, so the
               M1 NMI handler has time to process each command (the M1 needs
               ~tens of ms between commands; 50us blasting drops them).  When
               NG_PROBE_CMD_GAP_MS is set, the command list is (re)injected
               during the capture loop instead of the pre-capture blast.  The
               first command goes in at NG_PROBE_CMD_START_MS. */
            const char *gap_env = getenv("NG_PROBE_CMD_GAP_MS");
            uint32_t gap_ms = gap_env && *gap_env ? (uint32_t)atoi(gap_env) : 0u;
            uint32_t start_ms = 200u;
            const char *start_env = getenv("NG_PROBE_CMD_START_MS");
            if (start_env && *start_env) {
                start_ms = (uint32_t)atoi(start_env);
            }
            uint32_t next_cmd = 0u;

            const char *ym_trace_path = getenv("NG_PROBE_YM_TRACE");
            FILE *ym_trace = ym_trace_path && *ym_trace_path ?
                fopen(ym_trace_path, "w") : NULL;
            uint32_t ym_trace_seen = ng_neogeo_audio_ym_write_count(audio);

            const uint32_t z80_per_step = 4000u; /* ~1ms of Z80 time */
            const uint32_t frames_per_step =
                (uint32_t)((uint64_t)z80_per_step * rate / NG_NEO_AUDIO_CPU_CLOCK_HZ);
            int16_t buf[4096];
            uint32_t emitted = 0;
            while (emitted < total_frames) {
                if (gap_ms != 0u && next_cmd < command_count) {
                    double now_ms = 1000.0 * (double)emitted / (double)rate;
                    double due_ms = (double)start_ms + (double)next_cmd * gap_ms;
                    if (now_ms >= due_ms) {
                        ng_neogeo_audio_write_command(audio, commands[next_cmd]);
                        ++next_cmd;
                    }
                }
                ng_neogeo_audio_advance_z80_cycles(audio, z80_per_step);
                uint32_t f = frames_per_step;
                if (f > 2048u) f = 2048u;
                if (emitted + f > total_frames) f = total_frames - emitted;
                ng_neogeo_audio_generate(audio, buf, f, rate);
                fwrite(buf, sizeof(int16_t) * 2u, f, wf);
                emitted += f;
                if (ym_trace) {
                    uint32_t total = ng_neogeo_audio_ym_write_count(audio);
                    uint32_t delta = total - ym_trace_seen;
                    if (delta > NG_NEO_AUDIO_YM_WRITE_LOG_CAPACITY) {
                        delta = NG_NEO_AUDIO_YM_WRITE_LOG_CAPACITY;
                    }
                    NgNeoAudioYmWrite wbuf[NG_NEO_AUDIO_YM_WRITE_LOG_CAPACITY];
                    uint32_t got = ng_neogeo_audio_copy_recent_ym_writes(audio, wbuf, delta);
                    for (uint32_t k = 0; k < got; ++k) {
                        fprintf(ym_trace, "%.4f cyc=%llu pc=%04x ret=%04x p%u %02x %02x\n",
                                (double)emitted / (double)rate,
                                (unsigned long long)wbuf[k].z80_cycles,
                                wbuf[k].z80_pc, wbuf[k].z80_caller, wbuf[k].port,
                                wbuf[k].address, wbuf[k].data);
                    }
                    ym_trace_seen = total;
                }
            }
            if (ym_trace) fclose(ym_trace);
            fclose(wf);
            printf("probe WAV: wrote %.2fs to %s\n", seconds, wav_path);
        }
    }

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
