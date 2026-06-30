#include "neogeo_ym2610.h"

#include "ngrecomp/neogeo_audio.h"

#include "ymfm_opn.h"

#include <algorithm>
#include <cstring>
#include <new>
#include <vector>

namespace {

static int16_t clamp_i16(int32_t value) {
    if (value < -32768) {
        return -32768;
    }
    if (value > 32767) {
        return 32767;
    }
    return static_cast<int16_t>(value);
}

static constexpr int kMameSsgRouteGain = 84;
static constexpr int kMameFmAdpcmRouteGain = 98;
static constexpr int kRouteGainDivisor = 100;

struct YmBackend;

struct YmInterface final : public ymfm::ymfm_interface {
    explicit YmInterface(YmBackend &owner_) : owner(owner_) {}

    void ymfm_set_timer(uint32_t timer, int32_t duration) override;
    void ymfm_set_busy_end(uint32_t clocks) override;
    bool ymfm_is_busy() override;
    void ymfm_update_irq(bool asserted) override;
    uint8_t ymfm_external_read(ymfm::access_class type, uint32_t address) override;

    void timer_callback(uint32_t timer) {
        if (m_engine) {
            m_engine->engine_timer_expired(timer);
        }
    }

    YmBackend &owner;
};

struct YmBackend {
    YmBackend() : intf(*this), chip(intf) {
        last_output.clear();
        configure_fidelity();
        reset();
    }

    void configure_fidelity() {
        /* MAME's YM2610 device sets SSG_FIDELITY to OPN_FIDELITY_MED before
           stream allocation.  That yields the hardware/MAME native stream rate
           of clock/144 (~55.6 kHz for Neo Geo) while keeping FM/ADPCM at one
           chip sample per stream sample. */
        chip.set_fidelity(ymfm::OPN_FIDELITY_MED);
        native_rate = chip.sample_rate(NG_NEO_YM2610_CLOCK_HZ);
    }

    void reset() {
        busy_cycles = 0;
        timer_cycles[0] = 0;
        timer_cycles[1] = 0;
        irq_pending = false;
        native_phase = 0.0;
        last_output.clear();
        configure_fidelity();
        chip.reset();
        native_rate = chip.sample_rate(NG_NEO_YM2610_CLOCK_HZ);
    }

    uint32_t combined_v_rom_size() const {
        return v1_rom_size + v2_rom_size;
    }

    uint8_t read_combined_v(uint32_t address) const {
        uint32_t total = combined_v_rom_size();
        if (total == 0) {
            return 0x00;
        }
        address %= total;
        if (v1_rom && address < v1_rom_size) {
            return v1_rom[address];
        }
        if (v2_rom && address >= v1_rom_size) {
            uint32_t v2_offset = address - v1_rom_size;
            if (v2_offset < v2_rom_size) {
                return v2_rom[v2_offset];
            }
        }
        return 0x00;
    }

    uint8_t read_pcm_a(uint32_t address) const {
        return read_combined_v(address);
    }

    uint8_t read_pcm_b(uint32_t address) const {
        /* MAME maps Metal Slug's 201-v1/201-v2 as one contiguous
           ymsnd:adpcma region, and if a separate adpcmb region is absent the
           YM2610 ADPCM-B space falls back to that same region.  The current
           .neo loader passes those V-ROM chunks as v1/v2, not as explicit
           A/B spaces, so present a combined sample address space to both
           engines. */
        return read_combined_v(address);
    }

    void advance_clocks(uint32_t clocks) {
        if (clocks == 0) {
            return;
        }

        if (busy_cycles > 0) {
            busy_cycles -= static_cast<int32_t>(clocks);
            if (busy_cycles < 0) {
                busy_cycles = 0;
            }
        }

        for (uint32_t i = 0; i < 2; ++i) {
            if (timer_cycles[i] <= 0) {
                continue;
            }
            timer_cycles[i] -= static_cast<int32_t>(clocks);
            if (timer_cycles[i] <= 0) {
                timer_cycles[i] = 0;
                intf.timer_callback(i);
            }
        }
    }

    YmInterface intf;
    ymfm::ym2610 chip;
    const uint8_t *v1_rom = nullptr;
    uint32_t v1_rom_size = 0;
    const uint8_t *v2_rom = nullptr;
    uint32_t v2_rom_size = 0;
    int32_t busy_cycles = 0;
    int32_t timer_cycles[2] = {0, 0};
    bool irq_pending = false;
    uint32_t native_rate = 0;
    double native_phase = 0.0;
    ymfm::ym2610::output_data last_output;
};

void YmInterface::ymfm_set_timer(uint32_t timer, int32_t duration) {
    if (timer >= 2) {
        return;
    }
    owner.timer_cycles[timer] = duration < 0 ? 0 : duration;
}

void YmInterface::ymfm_set_busy_end(uint32_t clocks) {
    owner.busy_cycles = static_cast<int32_t>(clocks);
}

bool YmInterface::ymfm_is_busy() {
    return owner.busy_cycles > 0;
}

void YmInterface::ymfm_update_irq(bool asserted) {
    owner.irq_pending = asserted;
}

uint8_t YmInterface::ymfm_external_read(ymfm::access_class type,
                                        uint32_t address) {
    if (type == ymfm::ACCESS_ADPCM_A) {
        return owner.read_pcm_a(address);
    }
    if (type == ymfm::ACCESS_ADPCM_B) {
        return owner.read_pcm_b(address);
    }
    return 0x00;
}

} // namespace

extern "C" {

struct NgNeoYm2610 {
    YmBackend backend;
};

static constexpr uint32_t kYm2610StateMagic = 0x4E475953; /* NGYS */
static constexpr uint32_t kYm2610StateVersion = 1;

struct Ym2610StatePrefix {
    uint32_t magic;
    uint32_t version;
    uint32_t size;
    int32_t busy_cycles;
    int32_t timer_cycles[2];
    uint8_t irq_pending;
    uint8_t reserved[7];
    uint32_t native_rate;
    uint64_t native_phase_bits;
    int32_t last_output_data[ymfm::ym2610::OUTPUTS];
    uint32_t ymfm_size;
};

static void append_bytes(std::vector<uint8_t> &buffer,
                         const void *data,
                         size_t size) {
    const uint8_t *bytes = static_cast<const uint8_t *>(data);
    buffer.insert(buffer.end(), bytes, bytes + size);
}

static std::vector<uint8_t> save_ym2610_state_vector(NgNeoYm2610 *ym) {
    std::vector<uint8_t> out;
    if (!ym) {
        return out;
    }

    std::vector<uint8_t> ymfm_data;
    ymfm::ymfm_saved_state ymfm_state(ymfm_data, true);
    ym->backend.chip.save_restore(ymfm_state);

    Ym2610StatePrefix prefix;
    std::memset(&prefix, 0, sizeof(prefix));
    prefix.magic = kYm2610StateMagic;
    prefix.version = kYm2610StateVersion;
    prefix.size = static_cast<uint32_t>(sizeof(prefix) + ymfm_data.size());
    prefix.busy_cycles = ym->backend.busy_cycles;
    prefix.timer_cycles[0] = ym->backend.timer_cycles[0];
    prefix.timer_cycles[1] = ym->backend.timer_cycles[1];
    prefix.irq_pending = ym->backend.irq_pending ? 1u : 0u;
    prefix.native_rate = ym->backend.native_rate;
    static_assert(sizeof(prefix.native_phase_bits) == sizeof(ym->backend.native_phase),
                  "unexpected double size");
    std::memcpy(&prefix.native_phase_bits,
                &ym->backend.native_phase,
                sizeof(prefix.native_phase_bits));
    for (uint32_t i = 0; i < ymfm::ym2610::OUTPUTS; ++i) {
        prefix.last_output_data[i] = ym->backend.last_output.data[i];
    }
    prefix.ymfm_size = static_cast<uint32_t>(ymfm_data.size());

    append_bytes(out, &prefix, sizeof(prefix));
    if (!ymfm_data.empty()) {
        append_bytes(out, ymfm_data.data(), ymfm_data.size());
    }
    return out;
}

NgNeoYm2610 *ng_neogeo_ym2610_create(void) {
    try {
        return new NgNeoYm2610();
    } catch (...) {
        return nullptr;
    }
}

void ng_neogeo_ym2610_destroy(NgNeoYm2610 *ym) {
    delete ym;
}

void ng_neogeo_ym2610_set_roms(NgNeoYm2610 *ym,
                               const uint8_t *v1_rom,
                               uint32_t v1_rom_size,
                               const uint8_t *v2_rom,
                               uint32_t v2_rom_size) {
    if (!ym) {
        return;
    }
    ym->backend.v1_rom = v1_rom;
    ym->backend.v1_rom_size = v1_rom_size;
    ym->backend.v2_rom = v2_rom;
    ym->backend.v2_rom_size = v2_rom_size;
}

void ng_neogeo_ym2610_reset(NgNeoYm2610 *ym) {
    if (!ym) {
        return;
    }
    ym->backend.reset();
}

uint8_t ng_neogeo_ym2610_read(NgNeoYm2610 *ym, uint8_t port) {
    if (!ym) {
        return 0x00;
    }
    return ym->backend.chip.read(port & 3u);
}

void ng_neogeo_ym2610_write(NgNeoYm2610 *ym, uint8_t port, uint8_t data) {
    if (!ym) {
        return;
    }
    ym->backend.chip.write(port & 3u, data);
}

void ng_neogeo_ym2610_advance_clocks(NgNeoYm2610 *ym, uint32_t clocks) {
    if (!ym) {
        return;
    }
    ym->backend.advance_clocks(clocks);
}

void ng_neogeo_ym2610_generate(NgNeoYm2610 *ym,
                               int16_t *stereo_out,
                               uint32_t frames,
                               uint32_t sample_rate) {
    if (!stereo_out || frames == 0) {
        return;
    }
    if (!ym || sample_rate == 0) {
        std::memset(stereo_out, 0, sizeof(int16_t) * static_cast<size_t>(frames) * 2u);
        return;
    }

    YmBackend &backend = ym->backend;
    if (backend.native_rate == 0) {
        backend.native_rate = backend.chip.sample_rate(NG_NEO_YM2610_CLOCK_HZ);
    }
    const double native_per_output =
        static_cast<double>(backend.native_rate) / static_cast<double>(sample_rate);

    for (uint32_t i = 0; i < frames; ++i) {
        backend.native_phase += native_per_output;
        if (backend.native_phase < 1.0) {
            backend.native_phase = 1.0;
        }
        int64_t accum[3] = {0, 0, 0};
        uint32_t generated = 0;
        while (backend.native_phase >= 1.0) {
            backend.chip.generate(&backend.last_output);
            accum[0] += backend.last_output.data[0];
            accum[1] += backend.last_output.data[1];
            accum[2] += backend.last_output.data[2];
            ++generated;
            backend.native_phase -= 1.0;
        }
        if (generated == 0) {
            accum[0] = backend.last_output.data[0];
            accum[1] = backend.last_output.data[1];
            accum[2] = backend.last_output.data[2];
            generated = 1;
        }

        int32_t fm_adpcm_left = static_cast<int32_t>(accum[0] / generated);
        int32_t fm_adpcm_right = static_cast<int32_t>(accum[1] / generated);
        int32_t ssg_mono = static_cast<int32_t>(accum[2] / generated);
        /* Match MAME's Neo Geo stereo routes after ymfm's SSG-output rotation:
             SSG mono -> left/right at 0.84
             FM + ADPCM left/right -> respective speaker at 0.98 */
        int32_t left =
            (fm_adpcm_left * kMameFmAdpcmRouteGain +
             ssg_mono * kMameSsgRouteGain) /
            kRouteGainDivisor;
        int32_t right =
            (fm_adpcm_right * kMameFmAdpcmRouteGain +
             ssg_mono * kMameSsgRouteGain) /
            kRouteGainDivisor;
        stereo_out[i * 2u + 0u] = clamp_i16(left);
        stereo_out[i * 2u + 1u] = clamp_i16(right);
    }
}

uint32_t ng_neogeo_ym2610_native_sample_rate(NgNeoYm2610 *ym) {
    if (!ym) {
        return 0;
    }
    if (ym->backend.native_rate == 0) {
        ym->backend.native_rate = ym->backend.chip.sample_rate(NG_NEO_YM2610_CLOCK_HZ);
    }
    return ym->backend.native_rate;
}

uint8_t ng_neogeo_ym2610_irq_pending(NgNeoYm2610 *ym) {
    return ym && ym->backend.irq_pending ? 1u : 0u;
}

uint32_t ng_neogeo_ym2610_state_size(NgNeoYm2610 *ym) {
    std::vector<uint8_t> state = save_ym2610_state_vector(ym);
    return static_cast<uint32_t>(state.size());
}

int ng_neogeo_ym2610_save_state(NgNeoYm2610 *ym,
                                uint8_t *out,
                                uint32_t out_size,
                                uint32_t *out_written) {
    if (out_written) {
        *out_written = 0;
    }
    if (!ym || !out) {
        return 0;
    }

    std::vector<uint8_t> state = save_ym2610_state_vector(ym);
    if (state.empty() || out_size < state.size()) {
        return 0;
    }
    std::memcpy(out, state.data(), state.size());
    if (out_written) {
        *out_written = static_cast<uint32_t>(state.size());
    }
    return 1;
}

int ng_neogeo_ym2610_load_state(NgNeoYm2610 *ym,
                                const uint8_t *data,
                                uint32_t size) {
    if (!ym || !data || size < sizeof(Ym2610StatePrefix)) {
        return 0;
    }

    Ym2610StatePrefix prefix;
    std::memcpy(&prefix, data, sizeof(prefix));
    if (prefix.magic != kYm2610StateMagic ||
        prefix.version != kYm2610StateVersion ||
        prefix.size != size ||
        prefix.ymfm_size != size - sizeof(prefix)) {
        return 0;
    }

    ym->backend.busy_cycles = prefix.busy_cycles;
    ym->backend.timer_cycles[0] = prefix.timer_cycles[0];
    ym->backend.timer_cycles[1] = prefix.timer_cycles[1];
    ym->backend.irq_pending = prefix.irq_pending != 0;
    ym->backend.native_rate = prefix.native_rate;
    std::memcpy(&ym->backend.native_phase,
                &prefix.native_phase_bits,
                sizeof(ym->backend.native_phase));
    for (uint32_t i = 0; i < ymfm::ym2610::OUTPUTS; ++i) {
        ym->backend.last_output.data[i] = prefix.last_output_data[i];
    }

    std::vector<uint8_t> ymfm_data;
    ymfm_data.resize(prefix.ymfm_size);
    if (prefix.ymfm_size != 0) {
        std::memcpy(ymfm_data.data(), data + sizeof(prefix), prefix.ymfm_size);
    }
    ymfm::ymfm_saved_state ymfm_state(ymfm_data, false);
    ym->backend.chip.save_restore(ymfm_state);
    ym->backend.chip.invalidate_caches();
    return 1;
}

} // extern "C"
