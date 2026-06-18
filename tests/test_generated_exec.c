#include "exec_fixture.h"
#include "ngrecomp/neogeo_runtime.h"

#include <stdio.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

#define BUS_SIZE 0x2000u
#define CCR_C 0x0001u
#define CCR_V 0x0002u
#define CCR_Z 0x0004u
#define CCR_N 0x0008u
#define CCR_X 0x0010u

NgM68kState g_ng_m68k;

static uint8_t g_bus[BUS_SIZE];
static uint32_t g_dispatch_miss_count;
static uint32_t g_last_dispatch_miss;

void ng_generated_call(uint32_t addr);

static void oracle_set_nz16(NgM68kState *state, uint16_t value) {
    state->sr = (uint16_t)(state->sr & 0xFFF0u);
    if (value == 0) {
        state->sr |= CCR_Z;
    }
    if (value & 0x8000u) {
        state->sr |= CCR_N;
    }
}

static void oracle_set_nz8(NgM68kState *state, uint8_t value) {
    state->sr = (uint16_t)(state->sr & 0xFFF0u);
    if (value == 0) {
        state->sr |= CCR_Z;
    }
    if (value & 0x80u) {
        state->sr |= CCR_N;
    }
}

static void oracle_set_nz32(NgM68kState *state, uint32_t value) {
    state->sr = (uint16_t)(state->sr & 0xFFF0u);
    if (value == 0) {
        state->sr |= CCR_Z;
    }
    if (value & 0x80000000u) {
        state->sr |= CCR_N;
    }
}

static uint8_t bus_read8(const uint8_t *bus, uint32_t addr) {
    return addr < BUS_SIZE ? bus[addr] : 0xFFu;
}

static uint16_t bus_read16(const uint8_t *bus, uint32_t addr) {
    uint16_t hi = bus_read8(bus, addr);
    uint16_t lo = bus_read8(bus, addr + 1u);
    return (uint16_t)((hi << 8) | lo);
}

static uint32_t bus_read32(const uint8_t *bus, uint32_t addr) {
    uint32_t hi = bus_read16(bus, addr);
    uint32_t lo = bus_read16(bus, addr + 2u);
    return (hi << 16) | lo;
}

static void bus_write8(uint8_t *bus, uint32_t addr, uint8_t value) {
    if (addr < BUS_SIZE) {
        bus[addr] = value;
    }
}

static void bus_write16(uint8_t *bus, uint32_t addr, uint16_t value) {
    bus_write8(bus, addr, (uint8_t)(value >> 8));
    bus_write8(bus, addr + 1u, (uint8_t)value);
}

static void bus_write32(uint8_t *bus, uint32_t addr, uint32_t value) {
    bus_write16(bus, addr, (uint16_t)(value >> 16));
    bus_write16(bus, addr + 2u, (uint16_t)value);
}

static uint16_t program_read16(const uint8_t *program, uint32_t size, uint32_t addr);
static uint32_t program_read32(const uint8_t *program, uint32_t size, uint32_t addr);

static uint8_t oracle_ea_step_bytes(uint8_t reg, uint8_t bytes) {
    if (reg == 7u && bytes == 1u) {
        return 2u;
    }
    return bytes;
}

static uint32_t oracle_value_mask(uint8_t bytes) {
    if (bytes == 4u) {
        return 0xFFFFFFFFu;
    }
    return bytes == 1u ? 0x000000FFu : 0x0000FFFFu;
}

static uint32_t oracle_sign_extend_abs_w(uint16_t value) {
    return (uint32_t)(int32_t)(int16_t)value;
}

static int32_t oracle_index_value(const NgM68kState *state, uint16_t ext) {
    uint8_t is_addr = (uint8_t)((ext >> 15) & 1u);
    uint8_t reg = (uint8_t)((ext >> 12) & 7u);
    uint8_t is_long = (uint8_t)((ext >> 11) & 1u);
    uint32_t value = is_addr ? state->a[reg] : state->d[reg];

    if (is_long) {
        return (int32_t)value;
    }
    return (int32_t)(int16_t)(value & 0xFFFFu);
}

static uint32_t bus_read_size(const uint8_t *bus, uint32_t addr, uint8_t bytes) {
    if (bytes == 4u) {
        return bus_read32(bus, addr);
    }
    return bytes == 1u ? bus_read8(bus, addr) : bus_read16(bus, addr);
}

static void bus_write_size(uint8_t *bus, uint32_t addr, uint32_t value, uint8_t bytes) {
    if (bytes == 4u) {
        bus_write32(bus, addr, value);
    } else if (bytes == 1u) {
        bus_write8(bus, addr, (uint8_t)value);
    } else {
        bus_write16(bus, addr, (uint16_t)value);
    }
}

static void oracle_set_nz_size(NgM68kState *state, uint32_t value, uint8_t bytes) {
    if (bytes == 4u) {
        oracle_set_nz32(state, value);
    } else if (bytes == 1u) {
        oracle_set_nz8(state, (uint8_t)value);
    } else {
        oracle_set_nz16(state, (uint16_t)value);
    }
}

static int oracle_ea_memory_addr(const uint8_t *program,
                                 uint32_t size,
                                 NgM68kState *state,
                                 uint8_t mode,
                                 uint8_t reg,
                                 uint8_t bytes,
                                 uint32_t *pc_ext,
                                 uint32_t *out_addr) {
    switch (mode) {
    case 2:
        *out_addr = state->a[reg];
        return 1;
    case 3:
        *out_addr = state->a[reg];
        state->a[reg] += oracle_ea_step_bytes(reg, bytes);
        return 1;
    case 4:
        state->a[reg] -= oracle_ea_step_bytes(reg, bytes);
        *out_addr = state->a[reg];
        return 1;
    case 5: {
        int16_t displacement = (int16_t)program_read16(program, size, *pc_ext);
        *pc_ext += 2u;
        *out_addr = (uint32_t)((int32_t)state->a[reg] + (int32_t)displacement);
        return 1;
    }
    case 6: {
        uint16_t ext = program_read16(program, size, *pc_ext);
        int8_t displacement = (int8_t)(ext & 0xFFu);
        *pc_ext += 2u;
        *out_addr = (uint32_t)((int32_t)state->a[reg] +
                               oracle_index_value(state, ext) +
                               (int32_t)displacement);
        return 1;
    }
    case 7:
        if (reg == 0u) {
            *out_addr = oracle_sign_extend_abs_w(program_read16(program, size, *pc_ext));
            *pc_ext += 2u;
            return 1;
        }
        if (reg == 1u) {
            *out_addr = program_read32(program, size, *pc_ext);
            *pc_ext += 4u;
            return 1;
        }
        if (reg == 2u) {
            int16_t displacement = (int16_t)program_read16(program, size, *pc_ext);
            *out_addr = (uint32_t)((int32_t)(*pc_ext) + (int32_t)displacement);
            *pc_ext += 2u;
            return 1;
        }
        if (reg == 3u) {
            uint16_t ext = program_read16(program, size, *pc_ext);
            int8_t displacement = (int8_t)(ext & 0xFFu);
            *out_addr = (uint32_t)((int32_t)(*pc_ext) +
                                   (int32_t)displacement +
                                   oracle_index_value(state, ext));
            *pc_ext += 2u;
            return 1;
        }
        return 0;
    default:
        return 0;
    }
}

static int oracle_read_ea(const uint8_t *program,
                          uint32_t size,
                          NgM68kState *state,
                          uint8_t *bus,
                          uint8_t mode,
                          uint8_t reg,
                          uint8_t bytes,
                          uint32_t *pc_ext,
                          uint32_t *out_value) {
    if (mode == 0u) {
        *out_value = state->d[reg] & oracle_value_mask(bytes);
        return 1;
    }
    if (mode == 1u) {
        *out_value = state->a[reg];
        return 1;
    }
    if (mode == 7u && reg == 4u) {
        if (bytes == 4u) {
            *out_value = program_read32(program, size, *pc_ext);
            *pc_ext += 4u;
        } else {
            *out_value = program_read16(program, size, *pc_ext);
            *pc_ext += 2u;
            if (bytes == 1u) {
                *out_value &= 0xFFu;
            }
        }
        return 1;
    }

    {
        uint32_t addr;
        if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                   bytes, pc_ext, &addr)) {
            return 0;
        }
        *out_value = bus_read_size(bus, addr, bytes);
        return 1;
    }
}

static int oracle_write_ea(const uint8_t *program,
                           uint32_t size,
                           NgM68kState *state,
                           uint8_t *bus,
                           uint8_t mode,
                           uint8_t reg,
                           uint8_t bytes,
                           uint32_t *pc_ext,
                           uint32_t value) {
    uint32_t mask = oracle_value_mask(bytes);
    value &= mask;

    if (mode == 0u) {
        state->d[reg] = (state->d[reg] & ~mask) | value;
        return 1;
    }
    if (mode == 1u) {
        state->a[reg] = value;
        return 1;
    }

    {
        uint32_t addr;
        if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                   bytes, pc_ext, &addr)) {
            return 0;
        }
        bus_write_size(bus, addr, value, bytes);
        return 1;
    }
}

static uint8_t program_read8(const uint8_t *program, uint32_t size, uint32_t addr) {
    return addr < size ? program[addr] : 0xFFu;
}

static uint16_t program_read16(const uint8_t *program, uint32_t size, uint32_t addr) {
    uint16_t hi = program_read8(program, size, addr);
    uint16_t lo = program_read8(program, size, addr + 1u);
    return (uint16_t)((hi << 8) | lo);
}

static uint32_t program_read32(const uint8_t *program, uint32_t size, uint32_t addr) {
    uint32_t hi = program_read16(program, size, addr);
    uint32_t lo = program_read16(program, size, addr + 2u);
    return (hi << 16) | lo;
}

static int oracle_exec(const uint8_t *program,
                       uint32_t size,
                       uint32_t start_addr,
                       NgM68kState *state,
                       uint8_t *bus,
                       uint32_t depth) {
    uint32_t pc = start_addr;

    if (depth > 8u) {
        return 0;
    }

    for (uint32_t step = 0; step < 64u; ++step) {
        uint16_t op = program_read16(program, size, pc);

        if (op == 0x4E75u) {
            return 1;
        }
        if ((op & 0xF100u) == 0x7000u) {
            uint8_t reg = (uint8_t)((op >> 9) & 7u);
            state->d[reg] = (uint32_t)(int32_t)(int8_t)(op & 0xFFu);
            oracle_set_nz32(state, state->d[reg]);
            pc += 2u;
            continue;
        }
        if ((op & 0xF1FFu) == 0x103Cu) {
            uint8_t reg = (uint8_t)((op >> 9) & 7u);
            uint8_t value = (uint8_t)program_read16(program, size, pc + 2u);
            state->d[reg] = (state->d[reg] & 0xFFFFFF00u) | value;
            oracle_set_nz8(state, value);
            pc += 4u;
            continue;
        }
        if ((op & 0xF1F8u) == 0x1028u) {
            uint8_t reg = (uint8_t)((op >> 9) & 7u);
            uint8_t src_reg = (uint8_t)(op & 7u);
            int16_t displacement = (int16_t)program_read16(program, size, pc + 2u);
            uint32_t addr = (uint32_t)((int32_t)state->a[src_reg] + (int32_t)displacement);
            uint8_t value = bus_read8(bus, addr);
            state->d[reg] = (state->d[reg] & 0xFFFFFF00u) | value;
            oracle_set_nz8(state, value);
            pc += 4u;
            continue;
        }
        if ((op & 0xFFF8u) == 0x0C00u) {
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t value = (uint8_t)program_read16(program, size, pc + 2u);
            uint8_t dst = (uint8_t)(state->d[reg] & 0xFFu);
            uint8_t result = (uint8_t)(dst - value);
            oracle_set_nz8(state, result);
            if (dst < value) {
                state->sr |= CCR_C;
            }
            pc += 4u;
            continue;
        }
        if ((op & 0xFFF8u) == 0x0228u) {
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t value = (uint8_t)program_read16(program, size, pc + 2u);
            int16_t displacement = (int16_t)program_read16(program, size, pc + 4u);
            uint32_t addr = (uint32_t)((int32_t)state->a[reg] + (int32_t)displacement);
            uint8_t result = (uint8_t)(bus_read8(bus, addr) & value);
            bus_write8(bus, addr, result);
            oracle_set_nz8(state, result);
            pc += 6u;
            continue;
        }
        if (((op & 0xF000u) == 0xD000u ||
             (op & 0xF000u) == 0x9000u ||
             (op & 0xF000u) == 0xB000u) &&
            ((op >> 8) & 1u) == 0u) {
            uint8_t size_code = (uint8_t)((op >> 6) & 3u);
            uint8_t src_mode = (uint8_t)((op >> 3) & 7u);
            uint8_t src_reg = (uint8_t)(op & 7u);
            uint8_t dst_reg = (uint8_t)((op >> 9) & 7u);
            uint32_t value_mask;
            uint32_t sign_mask;
            uint32_t src;
            uint32_t dst;
            uint32_t result;
            uint8_t is_add = (uint8_t)((op & 0xF000u) == 0xD000u);
            uint8_t is_sub = (uint8_t)((op & 0xF000u) == 0x9000u);

            if (size_code == 3u || src_mode != 0u) {
                return 0;
            }
            if (size_code == 2u) {
                value_mask = 0xFFFFFFFFu;
                sign_mask = 0x80000000u;
            } else if (size_code == 1u) {
                value_mask = 0x0000FFFFu;
                sign_mask = 0x00008000u;
            } else {
                value_mask = 0x000000FFu;
                sign_mask = 0x00000080u;
            }

            src = state->d[src_reg] & value_mask;
            dst = state->d[dst_reg] & value_mask;
            if (is_add) {
                uint64_t full = (uint64_t)dst + (uint64_t)src;
                result = (uint32_t)full & value_mask;
                state->d[dst_reg] = (state->d[dst_reg] & ~value_mask) | result;
                state->sr = (uint16_t)(state->sr & 0xFFE0u);
                if (result == 0) state->sr |= CCR_Z;
                if (result & sign_mask) state->sr |= CCR_N;
                if (full > value_mask) state->sr |= CCR_C | CCR_X;
                if (((~(dst ^ src) & (dst ^ result)) & sign_mask) != 0) state->sr |= CCR_V;
            } else {
                result = (dst - src) & value_mask;
                if (is_sub) {
                    state->d[dst_reg] = (state->d[dst_reg] & ~value_mask) | result;
                    state->sr = (uint16_t)(state->sr & 0xFFE0u);
                } else {
                    state->sr = (uint16_t)(state->sr & 0xFFF0u);
                }
                if (result == 0) state->sr |= CCR_Z;
                if (result & sign_mask) state->sr |= CCR_N;
                if (src > dst) state->sr |= (uint16_t)(CCR_C | (is_sub ? CCR_X : 0u));
                if (((dst ^ src) & (dst ^ result) & sign_mask) != 0) state->sr |= CCR_V;
            }
            pc += 2u;
            continue;
        }
        if (op == 0xD040u) {
            uint16_t result = (uint16_t)(state->d[0] + state->d[0]);
            state->d[0] = (state->d[0] & 0xFFFF0000u) | result;
            oracle_set_nz16(state, result);
            pc += 2u;
            continue;
        }
        if ((op & 0xF138u) == 0xD100u) {
            uint8_t size_code = (uint8_t)((op >> 6) & 3u);
            uint8_t src_reg = (uint8_t)(op & 7u);
            uint8_t dst_reg = (uint8_t)((op >> 9) & 7u);
            uint8_t src = (uint8_t)(state->d[src_reg] & 0xFFu);
            uint8_t dst = (uint8_t)(state->d[dst_reg] & 0xFFu);
            uint8_t x = (state->sr & CCR_X) ? 1u : 0u;
            uint16_t sum = (uint16_t)dst + (uint16_t)src + (uint16_t)x;
            uint8_t result = (uint8_t)sum;
            uint16_t sr = (uint16_t)(state->sr & 0xFFE4u);
            if (size_code != 0) {
                return 0;
            }
            state->d[dst_reg] = (state->d[dst_reg] & 0xFFFFFF00u) | result;
            if (result != 0) {
                sr = (uint16_t)(sr & (uint16_t)~CCR_Z);
            }
            if (result & 0x80u) {
                sr |= CCR_N;
            }
            if (sum & 0x0100u) {
                sr |= CCR_C | CCR_X;
            }
            if (((~(dst ^ src) & (dst ^ result)) & 0x80u) != 0) {
                sr |= CCR_V;
            }
            state->sr = sr;
            pc += 2u;
            continue;
        }
        if ((op & 0xF138u) == 0x5100u) {
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t size_code = (uint8_t)((op >> 6) & 3u);
            uint8_t immediate = (uint8_t)((op >> 9) & 7u);
            if (immediate == 0) {
                immediate = 8;
            }
            if (size_code != 0) {
                return 0;
            }
            uint8_t old_value = (uint8_t)(state->d[reg] & 0xFFu);
            uint8_t result = (uint8_t)(old_value - immediate);
            state->d[reg] = (state->d[reg] & 0xFFFFFF00u) | result;
            oracle_set_nz8(state, result);
            if (old_value < immediate) {
                state->sr |= CCR_C | CCR_X;
            } else {
                state->sr = (uint16_t)(state->sr & (uint16_t)~CCR_X);
            }
            pc += 2u;
            continue;
        }
        if ((op & 0xF000u) == 0x6000u && (op & 0xFF00u) != 0x6000u &&
            (op & 0xFF00u) != 0x6100u) {
            uint8_t condition = (uint8_t)((op >> 8) & 0xFu);
            int8_t displacement = (int8_t)(op & 0xFFu);
            uint32_t next_pc = pc + 2u;
            int take;
            if (condition == 4u) {
                take = (state->sr & CCR_C) == 0;
            } else if (condition == 5u) {
                take = (state->sr & CCR_C) != 0;
            } else if (condition == 6u) {
                take = (state->sr & CCR_Z) == 0;
            } else if (condition == 7u) {
                take = (state->sr & CCR_Z) != 0;
            } else {
                return 0;
            }
            pc = take ? (uint32_t)((int32_t)next_pc + (int32_t)displacement) : next_pc;
            continue;
        }
        if (op == 0x4EB9u) {
            uint32_t target = program_read32(program, size, pc + 2u);
            if (!oracle_exec(program, size, target, state, bus, depth + 1u)) {
                return 0;
            }
            pc += 6u;
            continue;
        }
        if (op == 0x4EF9u) {
            uint32_t target = program_read32(program, size, pc + 2u);
            return oracle_exec(program, size, target, state, bus, depth + 1u);
        }
        if (op == 0x33FCu) {
            uint16_t value = program_read16(program, size, pc + 2u);
            uint32_t addr = program_read32(program, size, pc + 4u);
            bus_write16(bus, addr, value);
            oracle_set_nz16(state, value);
            pc += 8u;
            continue;
        }
        if (op == 0x13FCu) {
            uint8_t value = (uint8_t)program_read16(program, size, pc + 2u);
            uint32_t addr = program_read32(program, size, pc + 4u);
            bus_write8(bus, addr, value);
            oracle_set_nz8(state, value);
            pc += 8u;
            continue;
        }
        if ((op & 0xFFF8u) == 0x13C0u) {
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t value = (uint8_t)(state->d[reg] & 0xFFu);
            uint32_t addr = program_read32(program, size, pc + 2u);
            bus_write8(bus, addr, value);
            oracle_set_nz8(state, value);
            pc += 6u;
            continue;
        }
        if ((op & 0xFF00u) == 0x4200u &&
            (((op >> 3) & 7u) == 0u ||
             ((op >> 3) & 7u) == 3u ||
             ((op >> 3) & 7u) == 5u)) {
            uint8_t size_code = (uint8_t)((op >> 6) & 3u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint32_t pc_next = pc + 2u;
            uint32_t addr = 0;
            uint8_t bytes = size_code == 2u ? 4u : (size_code == 1u ? 2u : 1u);

            if (size_code == 3u) {
                return 0;
            }
            if (mode == 0u) {
                if (size_code == 2u) {
                    state->d[reg] = 0;
                    oracle_set_nz32(state, 0);
                } else if (size_code == 1u) {
                    state->d[reg] &= 0xFFFF0000u;
                    oracle_set_nz16(state, 0);
                } else {
                    state->d[reg] &= 0xFFFFFF00u;
                    oracle_set_nz8(state, 0);
                }
                pc = pc_next;
                continue;
            }
            if (mode == 3u) {
                addr = state->a[reg];
                state->a[reg] += (reg == 7u && size_code == 0u) ? 2u : bytes;
            } else if (mode == 5u) {
                int16_t displacement = (int16_t)program_read16(program, size, pc_next);
                pc_next += 2u;
                addr = (uint32_t)((int32_t)state->a[reg] + (int32_t)displacement);
            } else {
                return 0;
            }

            if (size_code == 2u) {
                bus_write32(bus, addr, 0);
                oracle_set_nz32(state, 0);
            } else if (size_code == 1u) {
                bus_write16(bus, addr, 0);
                oracle_set_nz16(state, 0);
            } else {
                bus_write8(bus, addr, 0);
                oracle_set_nz8(state, 0);
            }
            pc = pc_next;
            continue;
        }
        if (op == 0x4239u) {
            uint32_t addr = program_read32(program, size, pc + 2u);
            bus_write8(bus, addr, 0);
            oracle_set_nz8(state, 0);
            pc += 6u;
            continue;
        }
        if ((op & 0xFF38u) == 0x4200u) {
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t size_code = (uint8_t)((op >> 6) & 3u);
            if (size_code == 2u) {
                state->d[reg] = 0;
                oracle_set_nz32(state, 0);
            } else if (size_code == 1u) {
                state->d[reg] &= 0xFFFF0000u;
                oracle_set_nz16(state, 0);
            } else {
                state->d[reg] &= 0xFFFFFF00u;
                oracle_set_nz8(state, 0);
            }
            pc += 2u;
            continue;
        }
        if (op == 0x42B9u) {
            uint32_t addr = program_read32(program, size, pc + 2u);
            bus_write32(bus, addr, 0);
            oracle_set_nz32(state, 0);
            pc += 6u;
            continue;
        }
        if (op == 0x4A39u) {
            uint32_t addr = program_read32(program, size, pc + 2u);
            oracle_set_nz8(state, bus_read8(bus, addr));
            pc += 6u;
            continue;
        }
        if ((op & 0xFFF8u) == 0x4A28u) {
            uint8_t reg = (uint8_t)(op & 7u);
            int16_t displacement = (int16_t)program_read16(program, size, pc + 2u);
            uint32_t addr = (uint32_t)((int32_t)state->a[reg] + (int32_t)displacement);
            oracle_set_nz8(state, bus_read8(bus, addr));
            pc += 4u;
            continue;
        }
        if ((op & 0xF1FFu) == 0x41FAu) {
            uint8_t reg = (uint8_t)((op >> 9) & 7u);
            int16_t disp = (int16_t)program_read16(program, size, pc + 2u);
            state->a[reg] = (uint32_t)((int32_t)(pc + 4u) + (int32_t)disp);
            pc += 4u;
            continue;
        }
        if ((op & 0xFFF8u) == 0x23C8u) {
            uint8_t reg = (uint8_t)(op & 7u);
            uint32_t addr = program_read32(program, size, pc + 2u);
            bus_write32(bus, addr, state->a[reg]);
            oracle_set_nz32(state, state->a[reg]);
            pc += 6u;
            continue;
        }
        if ((op & 0xF000u) == 0x1000u ||
            (op & 0xF000u) == 0x2000u ||
            (op & 0xF000u) == 0x3000u) {
            uint8_t op_class = (uint8_t)((op >> 12) & 0xFu);
            uint8_t move_size = op_class == 1u ? 1u : (op_class == 2u ? 4u : 2u);
            uint8_t src_mode = (uint8_t)((op >> 3) & 7u);
            uint8_t src_reg = (uint8_t)(op & 7u);
            uint8_t dst_mode = (uint8_t)((op >> 6) & 7u);
            uint8_t dst_reg = (uint8_t)((op >> 9) & 7u);
            uint32_t next_pc = pc + 2u;
            uint32_t value = 0;
            uint8_t is_movea = (uint8_t)(dst_mode == 1u);

            if (!oracle_read_ea(program, size, state, bus, src_mode, src_reg,
                                move_size, &next_pc, &value)) {
                return 0;
            }

            if (is_movea) {
                if (move_size == 1u) {
                    return 0;
                }
                state->a[dst_reg] = (move_size == 2u) ?
                    (uint32_t)(int32_t)(int16_t)value : value;
            } else {
                if (!oracle_write_ea(program, size, state, bus, dst_mode, dst_reg,
                                     move_size, &next_pc, value)) {
                    return 0;
                }
                oracle_set_nz_size(state, value, move_size);
            }

            pc = next_pc;
            continue;
        }

        return 0;
    }

    return 0;
}

uint8_t ng68k_read8(uint32_t addr) {
    return bus_read8(g_bus, addr);
}

uint16_t ng68k_read16(uint32_t addr) {
    return bus_read16(g_bus, addr);
}

uint32_t ng68k_read32(uint32_t addr) {
    return bus_read32(g_bus, addr);
}

void ng68k_write8(uint32_t addr, uint8_t value) {
    bus_write8(g_bus, addr, value);
}

void ng68k_write16(uint32_t addr, uint16_t value) {
    ng68k_write8(addr, (uint8_t)(value >> 8));
    ng68k_write8(addr + 1u, (uint8_t)value);
}

void ng68k_write32(uint32_t addr, uint32_t value) {
    ng68k_write16(addr, (uint16_t)(value >> 16));
    ng68k_write16(addr + 2u, (uint16_t)value);
}

void ng_call_by_address(uint32_t addr) {
    ng_generated_call(addr);
}

void ng_log_dispatch_miss(uint32_t addr) {
    ++g_dispatch_miss_count;
    g_last_dispatch_miss = addr & 0x00FFFFFFu;
}

int main(void) {
    uint8_t program[NG_EXEC_FIXTURE_SIZE];
    NgM68kState expected_state;
    uint8_t expected_bus[BUS_SIZE];

    ng_exec_fixture_fill(program, (uint32_t)sizeof(program));
    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    bus_write8(expected_bus, 0x100Cu, 0xAAu);
    bus_write32(expected_bus, 0x1010u, 0x12345678u);
    bus_write8(expected_bus, 0x101Au, 0xAFu);
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0, &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    ng68k_write8(0x100Cu, 0xAAu);
    ng68k_write32(0x1010u, 0x12345678u);
    ng68k_write8(0x101Au, 0xAFu);
    g_dispatch_miss_count = 0;
    g_last_dispatch_miss = 0;

    ng_generated_call(0x00000000u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_ng_m68k.d, expected_state.d, sizeof(g_ng_m68k.d)) == 0);
    CHECK(memcmp(g_ng_m68k.a, expected_state.a, sizeof(g_ng_m68k.a)) == 0);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.d[0] == 0u);
    CHECK((g_ng_m68k.d[2] & 0xFFu) == 0x7Fu);
    CHECK((g_ng_m68k.d[3] & 0xFFu) == 0x00u);
    CHECK((g_ng_m68k.d[4] & 0xFFu) == 0x0Eu);
    CHECK((g_ng_m68k.d[5] & 0xFFFFu) == 0x1357u);
    CHECK((g_ng_m68k.d[6] & 0xFFFFu) == 0x1357u);
    CHECK(g_ng_m68k.a[0] == 0x00000121u);
    CHECK(g_ng_m68k.a[1] == 0x00000125u);
    CHECK(ng68k_read16(0x0068u) == 0x0000u);
    CHECK(ng68k_read16(0x00ACu) == 0x0000u);
    CHECK(ng68k_read8(0x0120u) == 0x5Au);
    CHECK(ng68k_read8(0x0124u) == 0x5Au);
    CHECK(ng68k_read16(0x1000u) == 0x1234u);
    CHECK(ng68k_read32(0x1004u) == 0x00000068u);
    CHECK(ng68k_read16(0x1008u) == 0x2222u);
    CHECK(ng68k_read16(0x100Au) == 0x0000u);
    CHECK(ng68k_read8(0x100Cu) == 0x00u);
    CHECK(ng68k_read8(0x100Eu) == 0x80u);
    CHECK(ng68k_read8(0x100Fu) == 0x00u);
    CHECK(ng68k_read32(0x1010u) == 0x00000000u);
    CHECK(ng68k_read32(0x1014u) == 0x000000A0u);
    CHECK(ng68k_read8(0x1018u) == 0x7Fu);
    CHECK(ng68k_read8(0x1019u) == 0x00u);
    CHECK(ng68k_read8(0x101Au) == 0x0Fu);
    CHECK(ng68k_read8(0x101Bu) == 0x00u);
    CHECK(ng68k_read8(0x101Cu) == 0x00u);
    CHECK(ng68k_read8(0x101Du) == 0x05u);
    CHECK((g_ng_m68k.sr & CCR_X) != 0);
    CHECK((g_ng_m68k.sr & CCR_N) != 0);

    ng_generated_call(0x00DEADu);
    CHECK(g_dispatch_miss_count == 1);
    CHECK(g_last_dispatch_miss == 0x00DEADu);

    return 0;
}
