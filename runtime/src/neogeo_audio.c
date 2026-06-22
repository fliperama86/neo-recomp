#include "ngrecomp/neogeo_audio.h"

#include "z80.h"
#include "neogeo_ym2610.h"

#include <stdlib.h>
#include <string.h>

struct NgNeoAudio {
    z80 cpu;
    uint8_t ram[NG_NEO_AUDIO_WORK_RAM_BYTES];

    const uint8_t *m_rom;
    uint32_t m_rom_size;
    const uint8_t *v1_rom;
    uint32_t v1_rom_size;
    const uint8_t *v2_rom;
    uint32_t v2_rom_size;

    uint8_t bank_a;
    uint8_t bank_b;
    uint8_t bank_c;
    uint8_t bank_d;

    uint8_t command_latch;
    uint8_t reply_latch;
    uint8_t nmi_enabled;
    uint8_t nmi_pending;
    uint32_t nmi_service_count;

    uint8_t ym_address[2];
    uint32_t ym_write_count;
    uint32_t ym_read_count;
    NgNeoAudioYmWrite ym_write_log[NG_NEO_AUDIO_YM_WRITE_LOG_CAPACITY];
    uint32_t ym_write_log_head;
    uint8_t adpcm_a_regs[0x30];
    NgNeoAudioAdpcmAEvent last_adpcm_a;
    NgNeoYm2610 *ym;
};

static uint32_t ng_audio_effective_m_rom_size(const NgNeoAudio *audio) {
    if (!audio) {
        return 0u;
    }
    if (audio->m_rom_size == 0x20000u) {
        /* MAME's NEO_BIOS_AUDIO_128K layout loads the 128 KiB M1 at
           $00000-$1ffff, then ROM_RELOADs it at $10000-$2ffff.  The reload
           overwrites $10000-$1ffff with the first half, leaving an effective
           0x30000-byte audio region:
             $00000-$0ffff -> M1[$00000-$0ffff]
             $10000-$1ffff -> M1[$00000-$0ffff]
             $20000-$2ffff -> M1[$10000-$1ffff]
           Neo Geo bank math is defined against that effective region. */
        return 0x30000u;
    }
    return audio->m_rom_size;
}

static uint8_t ng_audio_read_m(const NgNeoAudio *audio, uint32_t addr) {
    if (!audio || !audio->m_rom || audio->m_rom_size == 0u) {
        return 0xFFu;
    }
    uint32_t effective_size = ng_audio_effective_m_rom_size(audio);
    if (effective_size != 0u && addr >= effective_size) {
        addr %= effective_size;
    }
    if (audio->m_rom_size == 0x20000u && addr >= 0x10000u) {
        return audio->m_rom[(addr - 0x10000u) & 0x1FFFFu];
    }
    if (addr < audio->m_rom_size) {
        return audio->m_rom[addr];
    }
    return audio->m_rom[addr % audio->m_rom_size];
}

static uint32_t ng_audio_m_bank_base(const NgNeoAudio *audio,
                                     uint8_t bank,
                                     uint8_t shift) {
    uint32_t effective_size = ng_audio_effective_m_rom_size(audio);
    if (!audio || effective_size <= 0x10000u) {
        return 0u;
    }

    /* MAME's Neo Geo audio banking configures entries from
       0x10000 + ((bank << shift) & ((len - 0x10000 - 1) & 0x3ffff)). */
    uint32_t mask = (effective_size - 0x10000u - 1u) & 0x3FFFFu;
    uint32_t base = 0x10000u + (((uint32_t)bank << shift) & mask);
    if (base < effective_size) {
        return base;
    }

    uint32_t bank_span = effective_size - 0x10000u;
    if (bank_span == 0u) {
        return 0u;
    }
    return 0x10000u + ((base - 0x10000u) % bank_span);
}

static uint8_t ng_audio_debug_read_z80_impl(const NgNeoAudio *audio,
                                            uint16_t addr) {
    if (!audio) {
        return 0xFFu;
    }
    if (addr <= 0x7FFFu) {
        return ng_audio_read_m(audio, addr);
    }
    if (addr <= 0xBFFFu) {
        return ng_audio_read_m(audio,
                               ng_audio_m_bank_base(audio, audio->bank_a, 14u) +
                                   (uint32_t)(addr & 0x3FFFu));
    }
    if (addr <= 0xDFFFu) {
        return ng_audio_read_m(audio,
                               ng_audio_m_bank_base(audio, audio->bank_b, 13u) +
                                   (uint32_t)(addr & 0x1FFFu));
    }
    if (addr <= 0xEFFFu) {
        return ng_audio_read_m(audio,
                               ng_audio_m_bank_base(audio, audio->bank_c, 12u) +
                                   (uint32_t)(addr & 0x0FFFu));
    }
    if (addr <= 0xF7FFu) {
        return ng_audio_read_m(audio,
                               ng_audio_m_bank_base(audio, audio->bank_d, 11u) +
                                   (uint32_t)(addr & 0x07FFu));
    }
    return audio->ram[addr & (NG_NEO_AUDIO_WORK_RAM_BYTES - 1u)];
}

static void ng_audio_debug_write_z80_impl(NgNeoAudio *audio,
                                          uint16_t addr,
                                          uint8_t value) {
    if (!audio) {
        return;
    }
    if (addr >= 0xF800u) {
        audio->ram[addr & (NG_NEO_AUDIO_WORK_RAM_BYTES - 1u)] = value;
    }
}

static void ng_audio_record_ym_write(NgNeoAudio *audio,
                                     uint8_t port,
                                     uint8_t address,
                                     uint8_t data) {
    if (!audio) {
        return;
    }
    NgNeoAudioYmWrite entry;
    entry.z80_cycles = audio->cpu.cyc;
    entry.port = port;
    entry.address = address;
    entry.data = data;
    audio->ym_write_log[audio->ym_write_log_head] = entry;
    audio->ym_write_log_head =
        (audio->ym_write_log_head + 1u) % NG_NEO_AUDIO_YM_WRITE_LOG_CAPACITY;
    ++audio->ym_write_count;
}

static void ng_audio_refresh_last_adpcm_a_event(NgNeoAudio *audio) {
    if (!audio || audio->last_adpcm_a.keyon_count == 0u ||
        audio->last_adpcm_a.channel >= 6u) {
        return;
    }

    uint8_t ch = audio->last_adpcm_a.channel;
    audio->last_adpcm_a.start_addr =
        (uint32_t)(audio->adpcm_a_regs[0x10u + ch] |
                   ((uint32_t)audio->adpcm_a_regs[0x18u + ch] << 8)) << 8;
    audio->last_adpcm_a.end_addr =
        (uint32_t)(audio->adpcm_a_regs[0x20u + ch] |
                   ((uint32_t)audio->adpcm_a_regs[0x28u + ch] << 8)) << 8;
    audio->last_adpcm_a.level = audio->adpcm_a_regs[0x08u + ch] & 0x1Fu;
    audio->last_adpcm_a.total_level = audio->adpcm_a_regs[0x01u] & 0x3Fu;
    audio->last_adpcm_a.pan_left =
        (audio->adpcm_a_regs[0x08u + ch] & 0x80u) ? 1u : 0u;
    audio->last_adpcm_a.pan_right =
        (audio->adpcm_a_regs[0x08u + ch] & 0x40u) ? 1u : 0u;
}

static void ng_audio_track_ym_data_write(NgNeoAudio *audio,
                                         uint8_t port,
                                         uint8_t address,
                                         uint8_t data) {
    if (!audio || port != 3u || address >= sizeof(audio->adpcm_a_regs)) {
        return;
    }

    audio->adpcm_a_regs[address] = data;
    if (address == 0x00u) {
        uint8_t channels = data & 0x3Fu;
        uint8_t keyoff = data & 0x80u;
        for (uint8_t ch = 0; ch < 6u; ++ch) {
            if ((channels & (uint8_t)(1u << ch)) == 0u) {
                continue;
            }
            if (keyoff) {
                ++audio->last_adpcm_a.keyoff_count;
            } else {
                ++audio->last_adpcm_a.keyon_count;
                audio->last_adpcm_a.channel = ch;
            }
        }
    }
    ng_audio_refresh_last_adpcm_a_event(audio);
}

static uint8_t ng_audio_ym_read(NgNeoAudio *audio, uint8_t port) {
    if (!audio) {
        return 0xFFu;
    }
    ++audio->ym_read_count;
    return audio->ym ? ng_neogeo_ym2610_read(audio->ym, port) : 0x00u;
}

static void ng_audio_ym_write(NgNeoAudio *audio, uint8_t port, uint8_t data) {
    if (!audio) {
        return;
    }

    if ((port & 1u) == 0u) {
        audio->ym_address[(port >> 1u) & 1u] = data;
        if (audio->ym) {
            ng_neogeo_ym2610_write(audio->ym, port, data);
        }
        return;
    }

    uint8_t address = audio->ym_address[(port >> 1u) & 1u];
    ng_audio_record_ym_write(audio, port, address, data);
    ng_audio_track_ym_data_write(audio, port, address, data);
    if (audio->ym) {
        ng_neogeo_ym2610_write(audio->ym, port, data);
    }
}

static uint8_t ng_audio_port_read_impl(NgNeoAudio *audio, uint16_t port_addr) {
    if (!audio) {
        return 0xFFu;
    }

    switch (port_addr & 0x000Fu) {
    case 0x00u:
        audio->nmi_pending = 0u;
        return audio->command_latch;
    case 0x04u:
        return ng_audio_ym_read(audio, 0u);
    case 0x05u:
        return ng_audio_ym_read(audio, 1u);
    case 0x06u:
        return ng_audio_ym_read(audio, 2u);
    case 0x07u:
        return ng_audio_ym_read(audio, 3u);
    case 0x08u:
        audio->bank_d = (uint8_t)(port_addr >> 8);
        return 0u;
    case 0x09u:
        audio->bank_c = (uint8_t)(port_addr >> 8);
        return 0u;
    case 0x0Au:
        audio->bank_b = (uint8_t)(port_addr >> 8);
        return 0u;
    case 0x0Bu:
        audio->bank_a = (uint8_t)(port_addr >> 8);
        return 0u;
    case 0x0Cu:
        return 0u;
    default:
        return 0xFFu;
    }
}

static void ng_audio_port_write_impl(NgNeoAudio *audio,
                                     uint16_t port_addr,
                                     uint8_t value) {
    if (!audio) {
        return;
    }

    switch (port_addr & 0x001Fu) {
    case 0x08u:
    case 0x09u:
    case 0x0Au:
    case 0x0Bu:
        (void)value;
        audio->nmi_enabled = 1u;
        return;
    case 0x18u:
        (void)value;
        audio->nmi_enabled = 0u;
        return;
    default:
        break;
    }

    switch (port_addr & 0x000Fu) {
    case 0x00u:
        (void)value;
        audio->command_latch = 0u;
        return;
    case 0x04u:
        ng_audio_ym_write(audio, 0u, value);
        return;
    case 0x05u:
        ng_audio_ym_write(audio, 1u, value);
        return;
    case 0x06u:
        ng_audio_ym_write(audio, 2u, value);
        return;
    case 0x07u:
        ng_audio_ym_write(audio, 3u, value);
        return;
    case 0x0Cu:
        audio->reply_latch = value;
        return;
    default:
        return;
    }
}

static uint8_t ng_audio_z80_read(void *userdata, uint16_t addr) {
    return ng_audio_debug_read_z80_impl((const NgNeoAudio *)userdata, addr);
}

static void ng_audio_z80_write(void *userdata, uint16_t addr, uint8_t value) {
    ng_audio_debug_write_z80_impl((NgNeoAudio *)userdata, addr, value);
}

static uint8_t ng_audio_z80_port_in(z80 *cpu, uint16_t port_addr) {
    NgNeoAudio *audio = (NgNeoAudio *)cpu->userdata;
    return ng_audio_port_read_impl(audio, port_addr);
}

static void ng_audio_z80_port_out(z80 *cpu, uint16_t port_addr, uint8_t value) {
    NgNeoAudio *audio = (NgNeoAudio *)cpu->userdata;
    ng_audio_port_write_impl(audio, port_addr, value);
}

static void ng_audio_attach_cpu_callbacks(NgNeoAudio *audio) {
    audio->cpu.read_byte = ng_audio_z80_read;
    audio->cpu.write_byte = ng_audio_z80_write;
    audio->cpu.port_in = ng_audio_z80_port_in;
    audio->cpu.port_out = ng_audio_z80_port_out;
    audio->cpu.userdata = audio;
}

NgNeoAudio *ng_neogeo_audio_create(void) {
    NgNeoAudio *audio = (NgNeoAudio *)calloc(1u, sizeof(*audio));
    if (!audio) {
        return NULL;
    }
    audio->ym = ng_neogeo_ym2610_create();
    ng_neogeo_audio_reset(audio);
    return audio;
}

void ng_neogeo_audio_destroy(NgNeoAudio *audio) {
    if (!audio) {
        return;
    }
    ng_neogeo_ym2610_destroy(audio->ym);
    free(audio);
}

void ng_neogeo_audio_set_roms(NgNeoAudio *audio,
                              const uint8_t *m_rom,
                              uint32_t m_rom_size,
                              const uint8_t *v1_rom,
                              uint32_t v1_rom_size,
                              const uint8_t *v2_rom,
                              uint32_t v2_rom_size) {
    if (!audio) {
        return;
    }
    audio->m_rom = m_rom;
    audio->m_rom_size = m_rom_size;
    audio->v1_rom = v1_rom;
    audio->v1_rom_size = v1_rom_size;
    audio->v2_rom = v2_rom;
    audio->v2_rom_size = v2_rom_size;
    if (audio->ym) {
        ng_neogeo_ym2610_set_roms(audio->ym, v1_rom, v1_rom_size, v2_rom, v2_rom_size);
    }
}

void ng_neogeo_audio_reset(NgNeoAudio *audio) {
    if (!audio) {
        return;
    }

    const uint8_t *m_rom = audio->m_rom;
    uint32_t m_rom_size = audio->m_rom_size;
    const uint8_t *v1_rom = audio->v1_rom;
    uint32_t v1_rom_size = audio->v1_rom_size;
    const uint8_t *v2_rom = audio->v2_rom;
    uint32_t v2_rom_size = audio->v2_rom_size;
    NgNeoYm2610 *ym = audio->ym;

    memset(audio, 0, sizeof(*audio));
    audio->m_rom = m_rom;
    audio->m_rom_size = m_rom_size;
    audio->v1_rom = v1_rom;
    audio->v1_rom_size = v1_rom_size;
    audio->v2_rom = v2_rom;
    audio->v2_rom_size = v2_rom_size;
    audio->ym = ym;

    z80_init(&audio->cpu);
    ng_audio_attach_cpu_callbacks(audio);

    audio->bank_a = 0x02u;
    audio->bank_b = 0x06u;
    audio->bank_c = 0x0Eu;
    audio->bank_d = 0x1Eu;
    audio->reply_latch = 0xFFu;
    audio->last_adpcm_a.channel = 0xFFu;
    if (audio->ym) {
        ng_neogeo_ym2610_set_roms(audio->ym, v1_rom, v1_rom_size, v2_rom, v2_rom_size);
        ng_neogeo_ym2610_reset(audio->ym);
    }
}

void ng_neogeo_audio_write_command(NgNeoAudio *audio, uint8_t command) {
    if (!audio) {
        return;
    }
    audio->command_latch = command;
    audio->nmi_pending = 1u;
}

void ng_neogeo_audio_advance_z80_cycles(NgNeoAudio *audio, uint32_t cycles) {
    if (!audio || cycles == 0u) {
        return;
    }

    unsigned long start = audio->cpu.cyc;
    uint32_t instructions = 0;
    uint32_t max_instructions = cycles * 2u + 1024u;
    while ((uint32_t)(audio->cpu.cyc - start) < cycles &&
           instructions < max_instructions) {
        if (audio->nmi_pending && audio->nmi_enabled && !audio->cpu.nmi_pending) {
            z80_gen_nmi(&audio->cpu);
            audio->nmi_pending = 0u;
            ++audio->nmi_service_count;
        }
        if (audio->ym && ng_neogeo_ym2610_irq_pending(audio->ym) &&
            !audio->cpu.int_pending) {
            z80_gen_int(&audio->cpu, 0xFFu);
        }
        unsigned long before = audio->cpu.cyc;
        z80_step(&audio->cpu);
        unsigned long after = audio->cpu.cyc;
        if (after > before && audio->ym) {
            uint32_t z80_step_cycles = (uint32_t)(after - before);
            ng_neogeo_ym2610_advance_clocks(audio->ym, z80_step_cycles * 2u);
        }
        ++instructions;
    }
}


void ng_neogeo_audio_generate(NgNeoAudio *audio,
                              int16_t *stereo_out,
                              uint32_t frames,
                              uint32_t sample_rate) {
    if (!stereo_out || frames == 0u) {
        return;
    }
    if (!audio || !audio->ym) {
        memset(stereo_out, 0, sizeof(int16_t) * (size_t)frames * 2u);
        return;
    }
    ng_neogeo_ym2610_generate(audio->ym, stereo_out, frames, sample_rate);
}

uint32_t ng_neogeo_audio_ym2610_native_sample_rate(const NgNeoAudio *audio) {
    return audio && audio->ym ? ng_neogeo_ym2610_native_sample_rate(audio->ym) : 0u;
}

uint8_t ng_neogeo_audio_ym2610_irq_pending(const NgNeoAudio *audio) {
    return audio && audio->ym ? ng_neogeo_ym2610_irq_pending(audio->ym) : 0u;
}

uint8_t ng_neogeo_audio_command_latch(const NgNeoAudio *audio) {
    return audio ? audio->command_latch : 0xFFu;
}

uint8_t ng_neogeo_audio_reply_latch(const NgNeoAudio *audio) {
    return audio ? audio->reply_latch : 0xFFu;
}

uint8_t ng_neogeo_audio_nmi_enabled(const NgNeoAudio *audio) {
    return audio ? audio->nmi_enabled : 0u;
}

uint8_t ng_neogeo_audio_nmi_pending(const NgNeoAudio *audio) {
    return audio ? audio->nmi_pending : 0u;
}

uint64_t ng_neogeo_audio_z80_cycles(const NgNeoAudio *audio) {
    return audio ? (uint64_t)audio->cpu.cyc : 0u;
}

uint16_t ng_neogeo_audio_z80_pc(const NgNeoAudio *audio) {
    return audio ? audio->cpu.pc : 0u;
}

uint32_t ng_neogeo_audio_nmi_service_count(const NgNeoAudio *audio) {
    return audio ? audio->nmi_service_count : 0u;
}

uint32_t ng_neogeo_audio_ym_write_count(const NgNeoAudio *audio) {
    return audio ? audio->ym_write_count : 0u;
}

uint32_t ng_neogeo_audio_ym_read_count(const NgNeoAudio *audio) {
    return audio ? audio->ym_read_count : 0u;
}

NgNeoAudioYmWrite ng_neogeo_audio_last_ym_write(const NgNeoAudio *audio) {
    NgNeoAudioYmWrite empty;
    memset(&empty, 0, sizeof(empty));
    if (!audio || audio->ym_write_count == 0u) {
        return empty;
    }
    uint32_t last = audio->ym_write_log_head == 0u ?
        NG_NEO_AUDIO_YM_WRITE_LOG_CAPACITY - 1u : audio->ym_write_log_head - 1u;
    return audio->ym_write_log[last];
}

uint32_t ng_neogeo_audio_copy_recent_ym_writes(const NgNeoAudio *audio,
                                               NgNeoAudioYmWrite *out,
                                               uint32_t out_capacity) {
    if (!audio || !out || out_capacity == 0u || audio->ym_write_count == 0u) {
        return 0u;
    }

    uint32_t available = audio->ym_write_count;
    if (available > NG_NEO_AUDIO_YM_WRITE_LOG_CAPACITY) {
        available = NG_NEO_AUDIO_YM_WRITE_LOG_CAPACITY;
    }
    uint32_t count = available < out_capacity ? available : out_capacity;
    uint32_t start = (audio->ym_write_log_head +
                      NG_NEO_AUDIO_YM_WRITE_LOG_CAPACITY - count) %
                     NG_NEO_AUDIO_YM_WRITE_LOG_CAPACITY;
    for (uint32_t i = 0; i < count; ++i) {
        out[i] = audio->ym_write_log[(start + i) %
                                     NG_NEO_AUDIO_YM_WRITE_LOG_CAPACITY];
    }
    return count;
}

NgNeoAudioAdpcmAEvent ng_neogeo_audio_last_adpcm_a_event(const NgNeoAudio *audio) {
    NgNeoAudioAdpcmAEvent empty;
    memset(&empty, 0, sizeof(empty));
    empty.channel = 0xFFu;
    return audio ? audio->last_adpcm_a : empty;
}

int ng_neogeo_audio_copy_work_ram(const NgNeoAudio *audio,
                                  uint8_t *out,
                                  uint32_t out_size) {
    if (!audio || !out || out_size < NG_NEO_AUDIO_WORK_RAM_BYTES) {
        return 0;
    }
    memcpy(out, audio->ram, NG_NEO_AUDIO_WORK_RAM_BYTES);
    return 1;
}

uint8_t ng_neogeo_audio_debug_read_z80(const NgNeoAudio *audio, uint16_t addr) {
    return ng_audio_debug_read_z80_impl(audio, addr);
}

void ng_neogeo_audio_debug_write_z80(NgNeoAudio *audio,
                                     uint16_t addr,
                                     uint8_t value) {
    ng_audio_debug_write_z80_impl(audio, addr, value);
}

uint8_t ng_neogeo_audio_debug_port_read(NgNeoAudio *audio, uint16_t port_addr) {
    return ng_audio_port_read_impl(audio, port_addr);
}

void ng_neogeo_audio_debug_port_write(NgNeoAudio *audio,
                                      uint16_t port_addr,
                                      uint8_t value) {
    ng_audio_port_write_impl(audio, port_addr, value);
}
