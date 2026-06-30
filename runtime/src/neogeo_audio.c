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
    uint32_t z80_cycle_credit;
    uint32_t command_ack_count;
    uint32_t command_read_count;
    uint32_t command_clear_count;
    uint32_t nmi_service_count;

    uint8_t ym_address[2];
    uint32_t ym_write_count;
    uint32_t ym_read_count;
    NgNeoAudioYmWrite ym_write_log[NG_NEO_AUDIO_YM_WRITE_LOG_CAPACITY];
    uint32_t ym_write_log_head;
    uint8_t adpcm_a_regs[0x30];
    uint8_t adpcm_b_regs[0x11];
    NgNeoAudioAdpcmAEvent last_adpcm_a;
    NgNeoAudioAdpcmBEvent last_adpcm_b;
    NgNeoYm2610 *ym;
};

#define NG_NEO_AUDIO_STATE_MAGIC 0x4E474153u /* NGAS */
#define NG_NEO_AUDIO_STATE_VERSION 1u

typedef struct NgNeoAudioZ80State {
    unsigned long cyc;
    uint16_t pc;
    uint16_t sp;
    uint16_t ix;
    uint16_t iy;
    uint16_t mem_ptr;
    uint8_t a;
    uint8_t b;
    uint8_t c;
    uint8_t d;
    uint8_t e;
    uint8_t h;
    uint8_t l;
    uint8_t a_;
    uint8_t b_;
    uint8_t c_;
    uint8_t d_;
    uint8_t e_;
    uint8_t h_;
    uint8_t l_;
    uint8_t f_;
    uint8_t i;
    uint8_t r;
    uint8_t sf;
    uint8_t zf;
    uint8_t yf;
    uint8_t hf;
    uint8_t xf;
    uint8_t pf;
    uint8_t nf;
    uint8_t cf;
    uint8_t iff_delay;
    uint8_t interrupt_mode;
    uint8_t int_data;
    uint8_t iff1;
    uint8_t iff2;
    uint8_t halted;
    uint8_t int_pending;
    uint8_t nmi_pending;
} NgNeoAudioZ80State;

typedef struct NgNeoAudioStatePrefix {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    uint32_t ym_state_size;
    NgNeoAudioZ80State cpu;
    uint8_t ram[NG_NEO_AUDIO_WORK_RAM_BYTES];
    uint8_t bank_a;
    uint8_t bank_b;
    uint8_t bank_c;
    uint8_t bank_d;
    uint8_t command_latch;
    uint8_t reply_latch;
    uint8_t nmi_enabled;
    uint8_t nmi_pending;
    uint32_t z80_cycle_credit;
    uint32_t command_ack_count;
    uint32_t command_read_count;
    uint32_t command_clear_count;
    uint32_t nmi_service_count;
    uint8_t ym_address[2];
    uint32_t ym_write_count;
    uint32_t ym_read_count;
    NgNeoAudioYmWrite ym_write_log[NG_NEO_AUDIO_YM_WRITE_LOG_CAPACITY];
    uint32_t ym_write_log_head;
    uint8_t adpcm_a_regs[0x30];
    uint8_t adpcm_b_regs[0x11];
    NgNeoAudioAdpcmAEvent last_adpcm_a;
    NgNeoAudioAdpcmBEvent last_adpcm_b;
} NgNeoAudioStatePrefix;

static void ng_audio_capture_z80_state(const z80 *cpu, NgNeoAudioZ80State *out) {
    if (!cpu || !out) {
        return;
    }
    memset(out, 0, sizeof(*out));
    out->cyc = cpu->cyc;
    out->pc = cpu->pc;
    out->sp = cpu->sp;
    out->ix = cpu->ix;
    out->iy = cpu->iy;
    out->mem_ptr = cpu->mem_ptr;
    out->a = cpu->a;
    out->b = cpu->b;
    out->c = cpu->c;
    out->d = cpu->d;
    out->e = cpu->e;
    out->h = cpu->h;
    out->l = cpu->l;
    out->a_ = cpu->a_;
    out->b_ = cpu->b_;
    out->c_ = cpu->c_;
    out->d_ = cpu->d_;
    out->e_ = cpu->e_;
    out->h_ = cpu->h_;
    out->l_ = cpu->l_;
    out->f_ = cpu->f_;
    out->i = cpu->i;
    out->r = cpu->r;
    out->sf = cpu->sf;
    out->zf = cpu->zf;
    out->yf = cpu->yf;
    out->hf = cpu->hf;
    out->xf = cpu->xf;
    out->pf = cpu->pf;
    out->nf = cpu->nf;
    out->cf = cpu->cf;
    out->iff_delay = cpu->iff_delay;
    out->interrupt_mode = cpu->interrupt_mode;
    out->int_data = cpu->int_data;
    out->iff1 = cpu->iff1;
    out->iff2 = cpu->iff2;
    out->halted = cpu->halted;
    out->int_pending = cpu->int_pending;
    out->nmi_pending = cpu->nmi_pending;
}

static void ng_audio_restore_z80_state(z80 *cpu, const NgNeoAudioZ80State *in) {
    if (!cpu || !in) {
        return;
    }
    cpu->cyc = in->cyc;
    cpu->pc = in->pc;
    cpu->sp = in->sp;
    cpu->ix = in->ix;
    cpu->iy = in->iy;
    cpu->mem_ptr = in->mem_ptr;
    cpu->a = in->a;
    cpu->b = in->b;
    cpu->c = in->c;
    cpu->d = in->d;
    cpu->e = in->e;
    cpu->h = in->h;
    cpu->l = in->l;
    cpu->a_ = in->a_;
    cpu->b_ = in->b_;
    cpu->c_ = in->c_;
    cpu->d_ = in->d_;
    cpu->e_ = in->e_;
    cpu->h_ = in->h_;
    cpu->l_ = in->l_;
    cpu->f_ = in->f_;
    cpu->i = in->i;
    cpu->r = in->r;
    cpu->sf = in->sf != 0u;
    cpu->zf = in->zf != 0u;
    cpu->yf = in->yf != 0u;
    cpu->hf = in->hf != 0u;
    cpu->xf = in->xf != 0u;
    cpu->pf = in->pf != 0u;
    cpu->nf = in->nf != 0u;
    cpu->cf = in->cf != 0u;
    cpu->iff_delay = in->iff_delay;
    cpu->interrupt_mode = in->interrupt_mode;
    cpu->int_data = in->int_data;
    cpu->iff1 = in->iff1 != 0u;
    cpu->iff2 = in->iff2 != 0u;
    cpu->halted = in->halted != 0u;
    cpu->int_pending = in->int_pending != 0u;
    cpu->nmi_pending = in->nmi_pending != 0u;
}

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
    entry.z80_pc = audio->cpu.pc;
    entry.z80_caller = (uint16_t)(ng_audio_debug_read_z80_impl(audio, audio->cpu.sp) |
        ((uint16_t)ng_audio_debug_read_z80_impl(audio, (uint16_t)(audio->cpu.sp + 1u)) << 8));
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

static void ng_audio_refresh_last_adpcm_b_event(NgNeoAudio *audio) {
    if (!audio || audio->last_adpcm_b.keyon_count == 0u) {
        return;
    }

    audio->last_adpcm_b.start_addr =
        (uint32_t)(audio->adpcm_b_regs[0x02u] |
                   ((uint32_t)audio->adpcm_b_regs[0x03u] << 8)) << 8;
    audio->last_adpcm_b.end_addr =
        (uint32_t)(audio->adpcm_b_regs[0x04u] |
                   ((uint32_t)audio->adpcm_b_regs[0x05u] << 8)) << 8;
    audio->last_adpcm_b.delta_n =
        (uint16_t)(audio->adpcm_b_regs[0x09u] |
                   ((uint16_t)audio->adpcm_b_regs[0x0Au] << 8));
    audio->last_adpcm_b.level = audio->adpcm_b_regs[0x0Bu];
    audio->last_adpcm_b.control = audio->adpcm_b_regs[0x00u];
    audio->last_adpcm_b.pan_left =
        (audio->adpcm_b_regs[0x01u] & 0x80u) ? 1u : 0u;
    audio->last_adpcm_b.pan_right =
        (audio->adpcm_b_regs[0x01u] & 0x40u) ? 1u : 0u;
    audio->last_adpcm_b.repeat =
        (audio->adpcm_b_regs[0x00u] & 0x10u) ? 1u : 0u;
    audio->last_adpcm_b.speaker_off =
        (audio->adpcm_b_regs[0x00u] & 0x08u) ? 1u : 0u;
}

static void ng_audio_track_ym_data_write(NgNeoAudio *audio,
                                         uint8_t port,
                                         uint8_t address,
                                         uint8_t data) {
    if (!audio) {
        return;
    }

    if (port == 3u && address < sizeof(audio->adpcm_a_regs)) {
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
    } else if (port == 1u && address >= 0x10u && address <= 0x1Bu) {
        uint8_t reg = (uint8_t)(address - 0x10u);
        uint8_t effective = data;
        if (reg == 0x00u) {
            /* YM2610 ADPCM-B register 0 effectively has external mode forced
               and recording disabled on writes, matching ymfm/MAME. */
            effective = (uint8_t)((data | 0x20u) & (uint8_t)~0x40u);
        }
        audio->adpcm_b_regs[reg] = effective;
        if (reg == 0x00u) {
            if (effective & 0x80u) {
                ++audio->last_adpcm_b.keyon_count;
            }
            if (effective & 0x01u) {
                ++audio->last_adpcm_b.reset_count;
            }
        }
        ng_audio_refresh_last_adpcm_b_event(audio);
    }
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
        ++audio->command_ack_count;
        ++audio->command_read_count;
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
        ++audio->command_ack_count;
        ++audio->command_clear_count;
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

static void ng_audio_step_z80(NgNeoAudio *audio) {
    if (audio->nmi_pending && audio->nmi_enabled && !audio->cpu.nmi_pending) {
        z80_gen_nmi(&audio->cpu);
        audio->nmi_pending = 0u;
        ++audio->nmi_service_count;
    }
    if (audio->ym && ng_neogeo_ym2610_irq_pending(audio->ym)) {
        if (!audio->cpu.int_pending) {
            z80_gen_int(&audio->cpu, 0xFFu);
        }
    } else if (audio->cpu.int_pending) {
        /* The YM2610 IRQ line into Z80 IRQ0 is level-sensitive.  When the M1's
           IRQ handler clears the timer overflow (writing $27) before it RETIs,
           the line deasserts and the Z80 must withdraw the pending interrupt.
           superzazu latches int_pending until taken, so without this an already-
           asserted-but-not-yet-taken INT fires a spurious second time right after
           RETI, double-running the music tick (timer $27 written ~2x/fire), which
           makes the sequence race ahead and then stall.  Mirror MAME's level
           behavior by deasserting when the YM IRQ is no longer pending. */
        audio->cpu.int_pending = 0;
    }
    unsigned long before = audio->cpu.cyc;
    z80_step(&audio->cpu);
    unsigned long after = audio->cpu.cyc;
    if (after > before && audio->ym) {
        uint32_t z80_step_cycles = (uint32_t)(after - before);
        ng_neogeo_ym2610_advance_clocks(audio->ym, z80_step_cycles * 2u);
    }
}

void ng_neogeo_audio_advance_z80_cycles(NgNeoAudio *audio, uint32_t cycles) {
    if (!audio || cycles == 0u) {
        return;
    }

    /* z80_step() can only stop on an instruction boundary.  Live sync often
       asks for very small slices, so a few cycles of per-slice overshoot would
       accumulate into a faster M1 clock unless we carry that excess forward. */
    if (audio->z80_cycle_credit != 0u) {
        if (audio->z80_cycle_credit >= cycles) {
            audio->z80_cycle_credit -= cycles;
            return;
        }
        cycles -= audio->z80_cycle_credit;
        audio->z80_cycle_credit = 0u;
    }

    unsigned long start = audio->cpu.cyc;
    uint32_t instructions = 0;
    uint32_t max_instructions = cycles * 2u + 1024u;
    while ((uint32_t)(audio->cpu.cyc - start) < cycles &&
           instructions < max_instructions) {
        ng_audio_step_z80(audio);
        ++instructions;
    }
    uint32_t advanced = (uint32_t)(audio->cpu.cyc - start);
    if (advanced > cycles) {
        audio->z80_cycle_credit = advanced - cycles;
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

uint32_t ng_neogeo_audio_command_ack_count(const NgNeoAudio *audio) {
    return audio ? audio->command_ack_count : 0u;
}

uint32_t ng_neogeo_audio_command_read_count(const NgNeoAudio *audio) {
    return audio ? audio->command_read_count : 0u;
}

uint32_t ng_neogeo_audio_command_clear_count(const NgNeoAudio *audio) {
    return audio ? audio->command_clear_count : 0u;
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

NgNeoAudioAdpcmBEvent ng_neogeo_audio_last_adpcm_b_event(const NgNeoAudio *audio) {
    NgNeoAudioAdpcmBEvent empty;
    memset(&empty, 0, sizeof(empty));
    return audio ? audio->last_adpcm_b : empty;
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

uint32_t ng_neogeo_audio_state_size(const NgNeoAudio *audio) {
    if (!audio) {
        return 0u;
    }
    uint32_t ym_size = ng_neogeo_ym2610_state_size(audio->ym);
    if (UINT32_MAX - (uint32_t)sizeof(NgNeoAudioStatePrefix) < ym_size) {
        return 0u;
    }
    return (uint32_t)sizeof(NgNeoAudioStatePrefix) + ym_size;
}

int ng_neogeo_audio_save_state(const NgNeoAudio *audio,
                               uint8_t *out,
                               uint32_t out_size,
                               uint32_t *out_written) {
    if (out_written) {
        *out_written = 0u;
    }
    if (!audio || !out) {
        return 0;
    }

    uint32_t total_size = ng_neogeo_audio_state_size(audio);
    uint32_t ym_size = total_size >= (uint32_t)sizeof(NgNeoAudioStatePrefix) ?
        total_size - (uint32_t)sizeof(NgNeoAudioStatePrefix) : 0u;
    if (total_size == 0u || out_size < total_size) {
        return 0;
    }

    NgNeoAudioStatePrefix state;
    memset(&state, 0, sizeof(state));
    state.magic = NG_NEO_AUDIO_STATE_MAGIC;
    state.version = NG_NEO_AUDIO_STATE_VERSION;
    state.size = total_size;
    state.ym_state_size = ym_size;
    ng_audio_capture_z80_state(&audio->cpu, &state.cpu);
    memcpy(state.ram, audio->ram, sizeof(state.ram));
    state.bank_a = audio->bank_a;
    state.bank_b = audio->bank_b;
    state.bank_c = audio->bank_c;
    state.bank_d = audio->bank_d;
    state.command_latch = audio->command_latch;
    state.reply_latch = audio->reply_latch;
    state.nmi_enabled = audio->nmi_enabled;
    state.nmi_pending = audio->nmi_pending;
    state.z80_cycle_credit = audio->z80_cycle_credit;
    state.command_ack_count = audio->command_ack_count;
    state.command_read_count = audio->command_read_count;
    state.command_clear_count = audio->command_clear_count;
    state.nmi_service_count = audio->nmi_service_count;
    state.ym_address[0] = audio->ym_address[0];
    state.ym_address[1] = audio->ym_address[1];
    state.ym_write_count = audio->ym_write_count;
    state.ym_read_count = audio->ym_read_count;
    memcpy(state.ym_write_log, audio->ym_write_log, sizeof(state.ym_write_log));
    state.ym_write_log_head = audio->ym_write_log_head;
    memcpy(state.adpcm_a_regs, audio->adpcm_a_regs, sizeof(state.adpcm_a_regs));
    memcpy(state.adpcm_b_regs, audio->adpcm_b_regs, sizeof(state.adpcm_b_regs));
    state.last_adpcm_a = audio->last_adpcm_a;
    state.last_adpcm_b = audio->last_adpcm_b;

    memcpy(out, &state, sizeof(state));
    if (ym_size != 0u) {
        uint32_t written = 0u;
        if (!ng_neogeo_ym2610_save_state(audio->ym,
                                         out + sizeof(state),
                                         out_size - (uint32_t)sizeof(state),
                                         &written) ||
            written != ym_size) {
            return 0;
        }
    }
    if (out_written) {
        *out_written = total_size;
    }
    return 1;
}

int ng_neogeo_audio_load_state(NgNeoAudio *audio,
                               const uint8_t *data,
                               uint32_t size) {
    if (!audio || !data || size < (uint32_t)sizeof(NgNeoAudioStatePrefix)) {
        return 0;
    }

    NgNeoAudioStatePrefix state;
    memcpy(&state, data, sizeof(state));
    if (state.magic != NG_NEO_AUDIO_STATE_MAGIC ||
        state.version != NG_NEO_AUDIO_STATE_VERSION ||
        state.size != size ||
        state.ym_state_size != size - (uint32_t)sizeof(state)) {
        return 0;
    }
    if (state.ym_write_log_head >= NG_NEO_AUDIO_YM_WRITE_LOG_CAPACITY) {
        return 0;
    }

    ng_audio_restore_z80_state(&audio->cpu, &state.cpu);
    ng_audio_attach_cpu_callbacks(audio);
    memcpy(audio->ram, state.ram, sizeof(audio->ram));
    audio->bank_a = state.bank_a;
    audio->bank_b = state.bank_b;
    audio->bank_c = state.bank_c;
    audio->bank_d = state.bank_d;
    audio->command_latch = state.command_latch;
    audio->reply_latch = state.reply_latch;
    audio->nmi_enabled = state.nmi_enabled;
    audio->nmi_pending = state.nmi_pending;
    audio->z80_cycle_credit = state.z80_cycle_credit;
    audio->command_ack_count = state.command_ack_count;
    audio->command_read_count = state.command_read_count;
    audio->command_clear_count = state.command_clear_count;
    audio->nmi_service_count = state.nmi_service_count;
    audio->ym_address[0] = state.ym_address[0];
    audio->ym_address[1] = state.ym_address[1];
    audio->ym_write_count = state.ym_write_count;
    audio->ym_read_count = state.ym_read_count;
    memcpy(audio->ym_write_log, state.ym_write_log, sizeof(audio->ym_write_log));
    audio->ym_write_log_head = state.ym_write_log_head;
    memcpy(audio->adpcm_a_regs, state.adpcm_a_regs, sizeof(audio->adpcm_a_regs));
    memcpy(audio->adpcm_b_regs, state.adpcm_b_regs, sizeof(audio->adpcm_b_regs));
    audio->last_adpcm_a = state.last_adpcm_a;
    audio->last_adpcm_b = state.last_adpcm_b;

    if (state.ym_state_size != 0u &&
        !ng_neogeo_ym2610_load_state(audio->ym,
                                     data + sizeof(state),
                                     state.ym_state_size)) {
        return 0;
    }
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
