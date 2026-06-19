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
#define SR_S  0x2000u
#define SR_T  0x8000u

NgM68kState g_ng_m68k;

static uint8_t g_bus[BUS_SIZE];
static uint32_t g_dispatch_miss_count;
static uint32_t g_last_dispatch_miss;
static uint8_t g_pending_interrupt_level;
static uint8_t g_pending_interrupt_vector;
static uint32_t g_pending_interrupt_after_poll;
static uint32_t g_interrupt_poll_count;
static uint8_t g_arm_stop_interrupt_level;
static uint8_t g_arm_stop_interrupt_vector;
static uint8_t g_oracle_pending_interrupt_level;
static uint8_t g_oracle_pending_interrupt_vector;
static uint32_t g_oracle_pending_interrupt_after_poll;
static uint32_t g_oracle_interrupt_poll_count;
static uint8_t g_oracle_stop_interrupt_level;
static uint8_t g_oracle_stop_interrupt_vector;
static uint32_t g_reset_device_count;
static uint32_t g_oracle_reset_device_count;

void ng_generated_call(uint32_t addr);

static uint16_t bus_read16(const uint8_t *bus, uint32_t addr);
static uint32_t bus_read32(const uint8_t *bus, uint32_t addr);
static void bus_write16(uint8_t *bus, uint32_t addr, uint16_t value);
static void bus_write32(uint8_t *bus, uint32_t addr, uint32_t value);

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

static void oracle_set_sr(NgM68kState *state, uint16_t value) {
    uint16_t old_sr = state->sr;
    if (((old_sr ^ value) & SR_S) != 0) {
        if ((old_sr & SR_S) != 0) {
            state->ssp = state->a[7];
        } else {
            state->usp = state->a[7];
        }
        state->a[7] = (value & SR_S) != 0 ? state->ssp : state->usp;
    }
    state->sr = value;
}

static int oracle_is_supervisor(const NgM68kState *state) {
    return (state->sr & SR_S) != 0;
}

static void oracle_push_exception_frame_with_sr(NgM68kState *state,
                                                uint8_t *bus,
                                                uint32_t return_pc,
                                                uint16_t saved_sr,
                                                uint16_t next_sr) {
    oracle_set_sr(state, next_sr);
    state->a[7] -= 4u;
    bus_write32(bus, state->a[7], return_pc);
    state->a[7] -= 2u;
    bus_write16(bus, state->a[7], saved_sr);
}

static uint32_t oracle_exception(NgM68kState *state,
                                 uint8_t *bus,
                                 uint32_t return_pc,
                                 uint8_t vector) {
    uint16_t saved_sr = state->sr;
    oracle_push_exception_frame_with_sr(
        state,
        bus,
        return_pc,
        saved_sr,
        (uint16_t)((saved_sr | SR_S) & (uint16_t)~SR_T));
    return bus_read32(bus, (uint32_t)vector * 4u);
}

static uint32_t oracle_privilege_violation(NgM68kState *state,
                                           uint8_t *bus,
                                           uint32_t fault_pc) {
    return oracle_exception(state, bus, fault_pc, 8u);
}

static uint32_t oracle_interrupt(NgM68kState *state,
                                 uint8_t *bus,
                                 uint32_t return_pc,
                                 uint8_t level,
                                 uint8_t vector) {
    uint16_t saved_sr = state->sr;
    uint16_t new_sr = (uint16_t)((saved_sr | SR_S) &
                                 (uint16_t)~(SR_T | 0x0700u));
    new_sr = (uint16_t)(new_sr | (uint16_t)((level & 7u) << 8));
    oracle_push_exception_frame_with_sr(state, bus, return_pc, saved_sr, new_sr);
    return bus_read32(bus, (uint32_t)vector * 4u);
}

static int oracle_take_interrupt(const NgM68kState *state,
                                 uint8_t *level,
                                 uint8_t *vector) {
    uint8_t mask;
    if (g_oracle_pending_interrupt_level == 0u) {
        return 0;
    }
    ++g_oracle_interrupt_poll_count;
    if (g_oracle_pending_interrupt_after_poll != 0u &&
        g_oracle_interrupt_poll_count < g_oracle_pending_interrupt_after_poll) {
        return 0;
    }
    mask = (uint8_t)((state->sr >> 8) & 7u);
    if (g_oracle_pending_interrupt_level <= mask) {
        return 0;
    }
    *level = g_oracle_pending_interrupt_level;
    *vector = g_oracle_pending_interrupt_vector;
    g_oracle_pending_interrupt_level = 0;
    g_oracle_pending_interrupt_vector = 0;
    return 1;
}

static int oracle_condition_true(uint16_t sr, uint8_t condition) {
    int c = (sr & CCR_C) != 0;
    int v = (sr & CCR_V) != 0;
    int z = (sr & CCR_Z) != 0;
    int n = (sr & CCR_N) != 0;

    switch (condition) {
    case 0: return 1;
    case 1: return 0;
    case 2: return !c && !z;
    case 3: return c || z;
    case 4: return !c;
    case 5: return c;
    case 6: return !z;
    case 7: return z;
    case 8: return !v;
    case 9: return v;
    case 10: return !n;
    case 11: return n;
    case 12: return n == v;
    case 13: return n != v;
    case 14: return !z && (n == v);
    case 15: return z || (n != v);
    default: return 0;
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

static uint32_t oracle_sign_mask(uint8_t bytes) {
    if (bytes == 4u) {
        return 0x80000000u;
    }
    return bytes == 1u ? 0x00000080u : 0x00008000u;
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

    for (uint32_t step = 0; step < 192u; ++step) {
        uint8_t pending_level;
        uint8_t pending_vector;
        uint16_t op;

        if (oracle_take_interrupt(state, &pending_level, &pending_vector)) {
            pc = oracle_interrupt(state, bus, pc, pending_level, pending_vector);
            continue;
        }

        op = program_read16(program, size, pc);

        if (op == 0x4E71u) {
            pc += 2u;
            continue;
        }
        if (op == 0x4E75u) {
            pc = bus_read32(bus, state->a[7]);
            state->a[7] += 4u;
            continue;
        }
        if (op == 0x4E70u) {
            if (!oracle_is_supervisor(state)) {
                pc = oracle_privilege_violation(state, bus, pc);
                continue;
            }
            ++g_oracle_reset_device_count;
            pc += 2u;
            continue;
        }
        if (op == 0x4E72u) {
            uint16_t new_sr;
            uint8_t mask;
            if (!oracle_is_supervisor(state)) {
                pc = oracle_privilege_violation(state, bus, pc);
                continue;
            }
            new_sr = program_read16(program, size, pc + 2u);
            oracle_set_sr(state, new_sr);
            mask = (uint8_t)((state->sr >> 8) & 7u);
            if (g_oracle_stop_interrupt_level != 0u &&
                g_oracle_stop_interrupt_level > mask) {
                uint8_t level = g_oracle_stop_interrupt_level;
                uint8_t vector = g_oracle_stop_interrupt_vector;
                g_oracle_stop_interrupt_level = 0;
                g_oracle_stop_interrupt_vector = 0;
                pc = oracle_interrupt(state, bus, pc + 4u, level, vector);
                continue;
            }
            return 1;
        }
        if (op == 0x4E73u) {
            uint16_t sr = bus_read16(bus, state->a[7]);
            uint32_t target;
            if (!oracle_is_supervisor(state)) {
                pc = oracle_privilege_violation(state, bus, pc);
                continue;
            }
            state->a[7] += 2u;
            target = bus_read32(bus, state->a[7]);
            state->a[7] += 4u;
            oracle_set_sr(state, sr);
            pc = target;
            continue;
        }
        if (op == 0x4E77u) {
            uint16_t ccr = bus_read16(bus, state->a[7]);
            uint32_t target;
            state->a[7] += 2u;
            target = bus_read32(bus, state->a[7]);
            state->a[7] += 4u;
            state->sr = (uint16_t)((state->sr & 0xFFE0u) | (ccr & 0x001Fu));
            pc = target;
            continue;
        }
        if ((op & 0xFFF0u) == 0x4E40u) {
            uint8_t vector = (uint8_t)(32u + (op & 0x000Fu));
            pc = oracle_exception(state, bus, pc + 2u, vector);
            continue;
        }
        if (op == 0x4E76u) {
            if ((state->sr & CCR_V) != 0u) {
                pc = oracle_exception(state, bus, pc + 2u, 7u);
            } else {
                pc += 2u;
            }
            continue;
        }
        if (op == 0x4AFCu || (op & 0xF000u) == 0xA000u ||
            (op & 0xF000u) == 0xF000u) {
            uint8_t vector = op == 0x4AFCu ? 4u :
                (uint8_t)(((op & 0xF000u) == 0xA000u) ? 10u : 11u);
            pc = oracle_exception(state, bus, pc + 2u, vector);
            continue;
        }
        if ((op & 0xFFF8u) == 0x4E50u) {
            uint8_t reg = (uint8_t)(op & 7u);
            int16_t displacement = (int16_t)program_read16(program, size, pc + 2u);
            state->a[7] -= 4u;
            bus_write32(bus, state->a[7], state->a[reg]);
            state->a[reg] = state->a[7];
            state->a[7] = (uint32_t)((int32_t)state->a[7] + (int32_t)displacement);
            pc += 4u;
            continue;
        }
        if ((op & 0xFFF8u) == 0x4E58u) {
            uint8_t reg = (uint8_t)(op & 7u);
            state->a[7] = state->a[reg];
            state->a[reg] = bus_read32(bus, state->a[7]);
            state->a[7] += 4u;
            pc += 2u;
            continue;
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
        if ((op & 0xFF00u) == 0x0C00u) {
            uint8_t size_code = (uint8_t)((op >> 6) & 3u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t bytes = size_code == 2u ? 4u : (size_code == 1u ? 2u : 1u);
            uint32_t mask = oracle_value_mask(bytes);
            uint32_t sign_mask = oracle_sign_mask(bytes);
            uint32_t next_pc = pc + 2u;
            uint32_t src;
            uint32_t dst;
            uint32_t result;

            if (size_code == 3u || mode == 1u ||
                (mode == 7u && reg >= 2u)) {
                return 0;
            }
            if (bytes == 4u) {
                src = program_read32(program, size, next_pc);
                next_pc += 4u;
            } else {
                src = program_read16(program, size, next_pc);
                next_pc += 2u;
            }
            src &= mask;
            if (!oracle_read_ea(program, size, state, bus, mode, reg,
                                bytes, &next_pc, &dst)) {
                return 0;
            }
            dst &= mask;
            result = (dst - src) & mask;
            state->sr = (uint16_t)(state->sr & 0xFFF0u);
            if (result == 0) state->sr |= CCR_Z;
            if (result & sign_mask) state->sr |= CCR_N;
            if (src > dst) state->sr |= CCR_C;
            if (((dst ^ src) & (dst ^ result) & sign_mask) != 0) state->sr |= CCR_V;
            pc = next_pc;
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
        if (op == 0x003Cu || op == 0x023Cu || op == 0x0A3Cu ||
            op == 0x007Cu || op == 0x027Cu || op == 0x0A7Cu) {
            uint16_t imm = program_read16(program, size, pc + 2u);
            uint8_t is_ccr = (uint8_t)((op & 0x0040u) == 0u);
            uint8_t op_class = (uint8_t)((op >> 8) & 0x0Fu);
            uint16_t value = state->sr;

            if (!is_ccr && !oracle_is_supervisor(state)) {
                pc = oracle_privilege_violation(state, bus, pc);
                continue;
            }

            if (op_class == 0x0u) {
                value = (uint16_t)(value | imm);
            } else if (op_class == 0x2u) {
                value = (uint16_t)(value & imm);
            } else {
                value = (uint16_t)(value ^ imm);
            }

            if (is_ccr) {
                state->sr = (uint16_t)((state->sr & 0xFFE0u) | (value & 0x001Fu));
            } else {
                oracle_set_sr(state, value);
            }
            pc += 4u;
            continue;
        }
        if ((op & 0xFF00u) == 0x0000u ||
            (op & 0xFF00u) == 0x0200u ||
            (op & 0xFF00u) == 0x0A00u) {
            uint8_t size_code = (uint8_t)((op >> 6) & 3u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t op_class = (uint8_t)((op >> 8) & 0xFFu);
            uint8_t bytes = size_code == 2u ? 4u : (size_code == 1u ? 2u : 1u);
            uint32_t mask = oracle_value_mask(bytes);
            uint32_t next_pc = pc + 2u;
            uint32_t value;
            uint32_t result;

            if (size_code == 3u || mode == 1u ||
                (mode == 7u && reg >= 2u)) {
                return 0;
            }
            if (bytes == 4u) {
                value = program_read32(program, size, next_pc);
                next_pc += 4u;
            } else {
                value = program_read16(program, size, next_pc);
                next_pc += 2u;
            }
            value &= mask;
            if (mode == 0u) {
                uint32_t dst = state->d[reg] & mask;
                if (op_class == 0x00u) {
                    result = dst | value;
                } else if (op_class == 0x0Au) {
                    result = dst ^ value;
                } else {
                    result = dst & value;
                }
                state->d[reg] = (state->d[reg] & ~mask) | result;
            } else {
                uint32_t addr;
                uint32_t dst;
                if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                           bytes, &next_pc, &addr)) {
                    return 0;
                }
                dst = bus_read_size(bus, addr, bytes) & mask;
                if (op_class == 0x00u) {
                    result = dst | value;
                } else if (op_class == 0x0Au) {
                    result = dst ^ value;
                } else {
                    result = dst & value;
                }
                bus_write_size(bus, addr, result, bytes);
            }
            oracle_set_nz_size(state, result, bytes);
            pc = next_pc;
            continue;
        }
        if ((op & 0xFF00u) == 0x0800u) {
            uint8_t op_kind = (uint8_t)((op >> 6) & 3u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t bit = (uint8_t)(program_read16(program, size, pc + 2u) & 0xFFu);
            uint32_t next_pc = pc + 4u;

            if (mode == 1u || (mode == 7u && reg >= 2u)) {
                return 0;
            }

            if (mode == 0u) {
                uint32_t mask = (uint32_t)(1u << (bit & 31u));
                uint32_t value = state->d[reg];
                if ((value & mask) == 0) {
                    state->sr |= CCR_Z;
                } else {
                    state->sr = (uint16_t)(state->sr & (uint16_t)~CCR_Z);
                }
                if (op_kind == 1u) {
                    state->d[reg] = value ^ mask;
                } else if (op_kind == 2u) {
                    state->d[reg] = value & ~mask;
                } else if (op_kind == 3u) {
                    state->d[reg] = value | mask;
                }
            } else {
                uint32_t addr;
                uint8_t mask = (uint8_t)(1u << (bit & 7u));
                uint8_t value;
                if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                           1u, &next_pc, &addr)) {
                    return 0;
                }
                value = bus_read8(bus, addr);
                if ((value & mask) == 0) {
                    state->sr |= CCR_Z;
                } else {
                    state->sr = (uint16_t)(state->sr & (uint16_t)~CCR_Z);
                }
                if (op_kind == 1u) {
                    bus_write8(bus, addr, (uint8_t)(value ^ mask));
                } else if (op_kind == 2u) {
                    bus_write8(bus, addr, (uint8_t)(value & (uint8_t)~mask));
                } else if (op_kind == 3u) {
                    bus_write8(bus, addr, (uint8_t)(value | mask));
                }
            }
            pc = next_pc;
            continue;
        }
        if ((op & 0xF138u) == 0x0108u) {
            uint8_t data_reg = (uint8_t)((op >> 9) & 7u);
            uint8_t addr_reg = (uint8_t)(op & 7u);
            uint8_t dreg_to_mem = (uint8_t)((op >> 7) & 1u);
            uint8_t is_long = (uint8_t)((op & 0x0040u) != 0u);
            int16_t displacement = (int16_t)program_read16(program, size, pc + 2u);
            uint32_t addr = (uint32_t)((int32_t)state->a[addr_reg] +
                                       (int32_t)displacement);
            if (dreg_to_mem) {
                uint32_t value = state->d[data_reg];
                if (is_long) {
                    bus_write8(bus, addr, (uint8_t)(value >> 24));
                    bus_write8(bus, addr + 2u, (uint8_t)(value >> 16));
                    bus_write8(bus, addr + 4u, (uint8_t)(value >> 8));
                    bus_write8(bus, addr + 6u, (uint8_t)value);
                } else {
                    bus_write8(bus, addr, (uint8_t)(value >> 8));
                    bus_write8(bus, addr + 2u, (uint8_t)value);
                }
            } else if (is_long) {
                state->d[data_reg] =
                    ((uint32_t)bus_read8(bus, addr) << 24) |
                    ((uint32_t)bus_read8(bus, addr + 2u) << 16) |
                    ((uint32_t)bus_read8(bus, addr + 4u) << 8) |
                    (uint32_t)bus_read8(bus, addr + 6u);
            } else {
                state->d[data_reg] =
                    (state->d[data_reg] & 0xFFFF0000u) |
                    ((uint32_t)bus_read8(bus, addr) << 8) |
                    (uint32_t)bus_read8(bus, addr + 2u);
            }
            pc += 4u;
            continue;
        }
        if ((op & 0xF100u) == 0x0100u) {
            uint8_t op_kind = (uint8_t)((op >> 6) & 3u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t src_reg = (uint8_t)((op >> 9) & 7u);
            uint32_t next_pc = pc + 2u;

            if (mode == 1u || (mode == 7u && reg >= 2u)) {
                return 0;
            }

            if (mode == 0u) {
                uint32_t mask = (uint32_t)(1u << (state->d[src_reg] & 31u));
                uint32_t value = state->d[reg];
                if ((value & mask) == 0) {
                    state->sr |= CCR_Z;
                } else {
                    state->sr = (uint16_t)(state->sr & (uint16_t)~CCR_Z);
                }
                if (op_kind == 1u) {
                    state->d[reg] = value ^ mask;
                } else if (op_kind == 2u) {
                    state->d[reg] = value & ~mask;
                } else if (op_kind == 3u) {
                    state->d[reg] = value | mask;
                }
            } else {
                uint32_t addr;
                uint8_t mask = (uint8_t)(1u << (state->d[src_reg] & 7u));
                uint8_t value;
                if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                           1u, &next_pc, &addr)) {
                    return 0;
                }
                value = bus_read8(bus, addr);
                if ((value & mask) == 0) {
                    state->sr |= CCR_Z;
                } else {
                    state->sr = (uint16_t)(state->sr & (uint16_t)~CCR_Z);
                }
                if (op_kind == 1u) {
                    bus_write8(bus, addr, (uint8_t)(value ^ mask));
                } else if (op_kind == 2u) {
                    bus_write8(bus, addr, (uint8_t)(value & (uint8_t)~mask));
                } else if (op_kind == 3u) {
                    bus_write8(bus, addr, (uint8_t)(value | mask));
                }
            }
            pc = next_pc;
            continue;
        }
        if ((op & 0xFF00u) == 0x0400u ||
            (op & 0xFF00u) == 0x0600u) {
            uint8_t size_code = (uint8_t)((op >> 6) & 3u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t bytes = size_code == 2u ? 4u : (size_code == 1u ? 2u : 1u);
            uint32_t mask = oracle_value_mask(bytes);
            uint32_t sign_mask = oracle_sign_mask(bytes);
            uint32_t next_pc = pc + 2u;
            uint32_t src;
            uint32_t dst;
            uint32_t result;
            uint8_t is_add = (uint8_t)((op & 0xFF00u) == 0x0600u);

            if (size_code == 3u || mode == 1u ||
                (mode == 7u && reg >= 2u)) {
                return 0;
            }
            if (bytes == 4u) {
                src = program_read32(program, size, next_pc);
                next_pc += 4u;
            } else {
                src = program_read16(program, size, next_pc);
                next_pc += 2u;
            }
            src &= mask;
            if (mode == 0u) {
                dst = state->d[reg] & mask;
                result = is_add ? ((dst + src) & mask) : ((dst - src) & mask);
                state->d[reg] = (state->d[reg] & ~mask) | result;
            } else {
                uint32_t addr;
                if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                           bytes, &next_pc, &addr)) {
                    return 0;
                }
                dst = bus_read_size(bus, addr, bytes) & mask;
                result = is_add ? ((dst + src) & mask) : ((dst - src) & mask);
                bus_write_size(bus, addr, result, bytes);
            }

            state->sr = (uint16_t)(state->sr & 0xFFE0u);
            if (result == 0) state->sr |= CCR_Z;
            if (result & sign_mask) state->sr |= CCR_N;
            if (is_add) {
                uint64_t full = (uint64_t)dst + (uint64_t)src;
                if (full > mask) state->sr |= CCR_C | CCR_X;
                if (((~(dst ^ src) & (dst ^ result)) & sign_mask) != 0) state->sr |= CCR_V;
            } else {
                if (src > dst) state->sr |= CCR_C | CCR_X;
                if (((dst ^ src) & (dst ^ result) & sign_mask) != 0) state->sr |= CCR_V;
            }
            pc = next_pc;
            continue;
        }
        if ((op & 0xF1F8u) == 0xC140u) {
            uint8_t left_reg = (uint8_t)((op >> 9) & 7u);
            uint8_t right_reg = (uint8_t)(op & 7u);
            uint32_t tmp = state->d[left_reg];
            state->d[left_reg] = state->d[right_reg];
            state->d[right_reg] = tmp;
            pc += 2u;
            continue;
        }
        if ((op & 0xF1F8u) == 0xC148u) {
            uint8_t left_reg = (uint8_t)((op >> 9) & 7u);
            uint8_t right_reg = (uint8_t)(op & 7u);
            uint32_t tmp = state->a[left_reg];
            state->a[left_reg] = state->a[right_reg];
            state->a[right_reg] = tmp;
            pc += 2u;
            continue;
        }
        if ((op & 0xF1F8u) == 0xC188u) {
            uint8_t left_reg = (uint8_t)((op >> 9) & 7u);
            uint8_t right_reg = (uint8_t)(op & 7u);
            uint32_t tmp = state->d[left_reg];
            state->d[left_reg] = state->a[right_reg];
            state->a[right_reg] = tmp;
            pc += 2u;
            continue;
        }
        if ((op & 0xF000u) == 0x8000u &&
            (((op >> 6) & 7u) == 3u || ((op >> 6) & 7u) == 7u)) {
            uint8_t opmode = (uint8_t)((op >> 6) & 7u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t dst_reg = (uint8_t)((op >> 9) & 7u);
            uint32_t next_pc = pc + 2u;
            uint32_t src;

            if (mode == 1u ||
                !oracle_read_ea(program, size, state, bus, mode, reg,
                                2u, &next_pc, &src)) {
                return 0;
            }
            if (src == 0u) {
                pc = oracle_exception(state, bus, next_pc, 5u);
                continue;
            }
            state->sr = (uint16_t)(state->sr & 0xFFF0u);
            if (opmode == 3u) {
                uint32_t quotient = state->d[dst_reg] / (uint16_t)src;
                uint16_t remainder = (uint16_t)(state->d[dst_reg] % (uint16_t)src);
                if (quotient > 0xFFFFu) {
                    state->sr |= CCR_V;
                } else {
                    state->d[dst_reg] = ((uint32_t)remainder << 16) | (uint32_t)(uint16_t)quotient;
                    if ((uint16_t)quotient == 0) state->sr |= CCR_Z;
                    if (quotient & 0x8000u) state->sr |= CCR_N;
                }
            } else {
                int64_t dividend = (int32_t)state->d[dst_reg];
                int64_t divisor = (int16_t)src;
                int64_t quotient = dividend / divisor;
                int16_t remainder = (int16_t)(dividend % divisor);
                if (quotient < -32768 || quotient > 32767) {
                    state->sr |= CCR_V;
                } else {
                    state->d[dst_reg] = ((uint32_t)(uint16_t)remainder << 16) | (uint32_t)(uint16_t)quotient;
                    if ((int16_t)quotient == 0) state->sr |= CCR_Z;
                    if (quotient < 0) state->sr |= CCR_N;
                }
            }
            pc = next_pc;
            continue;
        }
        if ((op & 0xF000u) == 0x8000u ||
            (op & 0xF000u) == 0xB000u ||
            (op & 0xF000u) == 0xC000u) {
            if (((op & 0xF1F0u) == 0xC100u ||
                 (op & 0xF1F0u) == 0x8100u) &&
                (((op >> 3) & 7u) == 0u || ((op >> 3) & 7u) == 1u)) {
                uint8_t src_reg = (uint8_t)(op & 7u);
                uint8_t dst_reg = (uint8_t)((op >> 9) & 7u);
                uint8_t mem_form = (uint8_t)(((op >> 3) & 7u) == 1u);
                uint8_t is_sub = (uint8_t)((op & 0xF000u) == 0x8000u);
                uint8_t x = (state->sr & CCR_X) ? 1u : 0u;
                uint8_t src;
                uint8_t dst;
                uint8_t src_dec;
                uint8_t dst_dec;
                uint8_t result_dec;
                uint8_t result;
                uint8_t carry;

                if (mem_form) {
                    uint8_t src_step = src_reg == 7u ? 2u : 1u;
                    uint8_t dst_step = dst_reg == 7u ? 2u : 1u;
                    state->a[src_reg] -= src_step;
                    state->a[dst_reg] -= dst_step;
                    src = bus_read8(bus, state->a[src_reg]);
                    dst = bus_read8(bus, state->a[dst_reg]);
                } else {
                    src = (uint8_t)(state->d[src_reg] & 0xFFu);
                    dst = (uint8_t)(state->d[dst_reg] & 0xFFu);
                }
                src_dec = (uint8_t)(((src >> 4) & 0x0Fu) * 10u + (src & 0x0Fu));
                dst_dec = (uint8_t)(((dst >> 4) & 0x0Fu) * 10u + (dst & 0x0Fu));
                if (is_sub) {
                    uint16_t src_full = (uint16_t)src_dec + (uint16_t)x;
                    carry = (uint8_t)(src_full > dst_dec);
                    result_dec = (uint8_t)((100u + dst_dec - src_full) % 100u);
                } else {
                    uint16_t sum = (uint16_t)dst_dec + (uint16_t)src_dec + (uint16_t)x;
                    carry = (uint8_t)(sum > 99u);
                    result_dec = (uint8_t)(sum % 100u);
                }
                result = (uint8_t)(((result_dec / 10u) << 4) | (result_dec % 10u));
                if (mem_form) {
                    bus_write8(bus, state->a[dst_reg], result);
                } else {
                    state->d[dst_reg] = (state->d[dst_reg] & 0xFFFFFF00u) | result;
                }
                state->sr = (uint16_t)(state->sr & 0xFFE4u);
                if (result != 0) state->sr = (uint16_t)(state->sr & (uint16_t)~CCR_Z);
                if (result & 0x80u) state->sr |= CCR_N;
                if (carry) state->sr |= CCR_C | CCR_X;
                pc += 2u;
                continue;
            }
            if ((op & 0xF138u) == 0xB108u && ((op >> 6) & 3u) != 3u) {
                uint8_t size_code = (uint8_t)((op >> 6) & 3u);
                uint8_t bytes = size_code == 2u ? 4u : (size_code == 1u ? 2u : 1u);
                uint8_t src_reg = (uint8_t)(op & 7u);
                uint8_t dst_reg = (uint8_t)((op >> 9) & 7u);
                uint32_t mask = oracle_value_mask(bytes);
                uint32_t sign_mask = oracle_sign_mask(bytes);
                uint32_t src = bus_read_size(bus, state->a[src_reg], bytes) & mask;
                uint32_t dst = bus_read_size(bus, state->a[dst_reg], bytes) & mask;
                uint32_t result = (dst - src) & mask;
                state->a[src_reg] += (src_reg == 7u && bytes == 1u) ? 2u : bytes;
                state->a[dst_reg] += (dst_reg == 7u && bytes == 1u) ? 2u : bytes;
                state->sr = (uint16_t)(state->sr & 0xFFF0u);
                if (result == 0) state->sr |= CCR_Z;
                if (result & sign_mask) state->sr |= CCR_N;
                if (src > dst) state->sr |= CCR_C;
                if (((dst ^ src) & (dst ^ result) & sign_mask) != 0) state->sr |= CCR_V;
                pc += 2u;
                continue;
            }
            uint8_t top = (uint8_t)((op >> 12) & 0xFu);
            uint8_t opmode = (uint8_t)((op >> 6) & 7u);
            uint8_t size_code = (uint8_t)(opmode & 3u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t reg_field = (uint8_t)((op >> 9) & 7u);
            uint8_t bytes = size_code == 2u ? 4u : (size_code == 1u ? 2u : 1u);
            uint32_t mask = oracle_value_mask(bytes);
            uint32_t next_pc = pc + 2u;
            uint32_t src;
            uint32_t dst;
            uint32_t result;
            uint8_t is_and = (uint8_t)(top == 0xCu);
            uint8_t is_eor = (uint8_t)(top == 0xBu);

            if (size_code != 3u) {
                if ((top == 0x8u || top == 0xCu) && opmode <= 2u) {
                    if (mode == 1u ||
                        !oracle_read_ea(program, size, state, bus, mode, reg,
                                        bytes, &next_pc, &src)) {
                        return 0;
                    }
                    src &= mask;
                    dst = state->d[reg_field] & mask;
                    if (is_and) {
                        result = dst & src;
                    } else {
                        result = dst | src;
                    }
                    state->d[reg_field] = (state->d[reg_field] & ~mask) | result;
                    oracle_set_nz_size(state, result, bytes);
                    pc = next_pc;
                    continue;
                }
                if ((top == 0x8u || top == 0xCu) && opmode >= 4u && opmode <= 6u) {
                    uint32_t addr;
                    if (mode < 2u || (mode == 7u && reg >= 2u) ||
                        !oracle_ea_memory_addr(program, size, state, mode, reg,
                                               bytes, &next_pc, &addr)) {
                        return 0;
                    }
                    src = state->d[reg_field] & mask;
                    dst = bus_read_size(bus, addr, bytes) & mask;
                    result = is_and ? (dst & src) : (dst | src);
                    bus_write_size(bus, addr, result, bytes);
                    oracle_set_nz_size(state, result, bytes);
                    pc = next_pc;
                    continue;
                }
                if (is_eor && opmode >= 4u && opmode <= 6u) {
                    src = state->d[reg_field] & mask;
                    if (mode == 0u) {
                        dst = state->d[reg] & mask;
                        result = dst ^ src;
                        state->d[reg] = (state->d[reg] & ~mask) | result;
                    } else {
                        uint32_t addr;
                        if (mode == 1u || (mode == 7u && reg >= 2u) ||
                            !oracle_ea_memory_addr(program, size, state, mode, reg,
                                                   bytes, &next_pc, &addr)) {
                            return 0;
                        }
                        dst = bus_read_size(bus, addr, bytes) & mask;
                        result = dst ^ src;
                        bus_write_size(bus, addr, result, bytes);
                    }
                    oracle_set_nz_size(state, result, bytes);
                    pc = next_pc;
                    continue;
                }
            }
        }
        if ((op & 0xF000u) == 0xC000u &&
            (((op >> 6) & 7u) == 3u || ((op >> 6) & 7u) == 7u)) {
            uint8_t opmode = (uint8_t)((op >> 6) & 7u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t dst_reg = (uint8_t)((op >> 9) & 7u);
            uint32_t next_pc = pc + 2u;
            uint32_t src;
            uint32_t result;

            if (mode == 1u ||
                !oracle_read_ea(program, size, state, bus, mode, reg,
                                2u, &next_pc, &src)) {
                return 0;
            }
            if (opmode == 3u) {
                result = (uint32_t)((uint32_t)(uint16_t)(state->d[dst_reg] & 0xFFFFu) *
                                    (uint32_t)(uint16_t)src);
            } else {
                result = (uint32_t)((int32_t)(int16_t)(state->d[dst_reg] & 0xFFFFu) *
                                    (int32_t)(int16_t)src);
            }
            state->d[dst_reg] = result;
            oracle_set_nz32(state, result);
            pc = next_pc;
            continue;
        }
        if (((op & 0xF000u) == 0xD000u ||
             (op & 0xF000u) == 0x9000u ||
             (op & 0xF000u) == 0xB000u) &&
            (((op >> 6) & 7u) == 3u || ((op >> 6) & 7u) == 7u)) {
            uint8_t opmode = (uint8_t)((op >> 6) & 7u);
            uint8_t src_mode = (uint8_t)((op >> 3) & 7u);
            uint8_t src_reg = (uint8_t)(op & 7u);
            uint8_t dst_reg = (uint8_t)((op >> 9) & 7u);
            uint8_t bytes = opmode == 7u ? 4u : 2u;
            uint32_t next_pc = pc + 2u;
            uint32_t src;

            if (!oracle_read_ea(program, size, state, bus, src_mode, src_reg,
                                bytes, &next_pc, &src)) {
                return 0;
            }
            if (bytes == 2u) {
                src = (uint32_t)(int32_t)(int16_t)src;
            }

            if ((op & 0xF000u) == 0xD000u) {
                state->a[dst_reg] += src;
            } else if ((op & 0xF000u) == 0x9000u) {
                state->a[dst_reg] -= src;
            } else {
                uint32_t dst = state->a[dst_reg];
                uint32_t result = dst - src;
                state->sr = (uint16_t)(state->sr & 0xFFF0u);
                if (result == 0) state->sr |= CCR_Z;
                if (result & 0x80000000u) state->sr |= CCR_N;
                if (src > dst) state->sr |= CCR_C;
                if (((dst ^ src) & (dst ^ result) & 0x80000000u) != 0) state->sr |= CCR_V;
            }
            pc = next_pc;
            continue;
        }
        if (((op & 0xF000u) == 0xD000u ||
             (op & 0xF000u) == 0x9000u) &&
            ((op >> 6) & 7u) >= 4u && ((op >> 6) & 7u) <= 6u &&
            ((op >> 3) & 7u) >= 2u &&
            !(((op >> 3) & 7u) == 7u && (op & 7u) >= 2u)) {
            uint8_t opmode = (uint8_t)((op >> 6) & 7u);
            uint8_t size_code = (uint8_t)(opmode & 3u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t src_reg = (uint8_t)((op >> 9) & 7u);
            uint8_t bytes = size_code == 2u ? 4u : (size_code == 1u ? 2u : 1u);
            uint32_t mask = oracle_value_mask(bytes);
            uint32_t sign_mask = oracle_sign_mask(bytes);
            uint32_t next_pc = pc + 2u;
            uint32_t addr;
            uint32_t src;
            uint32_t dst;
            uint32_t result;
            uint8_t is_add = (uint8_t)((op & 0xF000u) == 0xD000u);

            if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                       bytes, &next_pc, &addr)) {
                return 0;
            }
            src = state->d[src_reg] & mask;
            dst = bus_read_size(bus, addr, bytes) & mask;
            result = is_add ? ((dst + src) & mask) : ((dst - src) & mask);
            bus_write_size(bus, addr, result, bytes);

            state->sr = (uint16_t)(state->sr & 0xFFE0u);
            if (result == 0) state->sr |= CCR_Z;
            if (result & sign_mask) state->sr |= CCR_N;
            if (is_add) {
                uint64_t full = (uint64_t)dst + (uint64_t)src;
                if (full > mask) state->sr |= CCR_C | CCR_X;
                if (((~(dst ^ src) & (dst ^ result)) & sign_mask) != 0) state->sr |= CCR_V;
            } else {
                if (src > dst) state->sr |= CCR_C | CCR_X;
                if (((dst ^ src) & (dst ^ result) & sign_mask) != 0) state->sr |= CCR_V;
            }
            pc = next_pc;
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
        if (((op & 0xF100u) == 0xD100u ||
             (op & 0xF100u) == 0x9100u) &&
            (((op >> 3) & 7u) == 0u || ((op >> 3) & 7u) == 1u)) {
            uint8_t size_code = (uint8_t)((op >> 6) & 3u);
            uint8_t src_reg = (uint8_t)(op & 7u);
            uint8_t dst_reg = (uint8_t)((op >> 9) & 7u);
            uint8_t mem_form = (uint8_t)(((op >> 3) & 7u) == 1u);
            uint8_t is_sub = (uint8_t)((op & 0xF000u) == 0x9000u);
            uint8_t x = (state->sr & CCR_X) ? 1u : 0u;
            uint8_t bytes = size_code == 2u ? 4u : (size_code == 1u ? 2u : 1u);
            uint32_t mask = oracle_value_mask(bytes);
            uint32_t sign_mask = oracle_sign_mask(bytes);
            uint32_t src;
            uint32_t dst;
            uint32_t result;
            uint64_t full;
            uint16_t sr = (uint16_t)(state->sr & 0xFFE4u);
            if (size_code == 3u) {
                return 0;
            }

            if (mem_form) {
                uint8_t src_step = (src_reg == 7u && bytes == 1u) ? 2u : bytes;
                uint8_t dst_step = (dst_reg == 7u && bytes == 1u) ? 2u : bytes;
                state->a[src_reg] -= src_step;
                state->a[dst_reg] -= dst_step;
                src = bus_read_size(bus, state->a[src_reg], bytes) & mask;
                dst = bus_read_size(bus, state->a[dst_reg], bytes) & mask;
            } else {
                src = state->d[src_reg] & mask;
                dst = state->d[dst_reg] & mask;
            }

            if (is_sub) {
                full = (uint64_t)src + (uint64_t)x;
                result = (uint32_t)((uint64_t)dst - full) & mask;
            } else {
                full = (uint64_t)dst + (uint64_t)src + (uint64_t)x;
                result = (uint32_t)full & mask;
            }

            if (mem_form) {
                bus_write_size(bus, state->a[dst_reg], result, bytes);
            } else {
                state->d[dst_reg] = (state->d[dst_reg] & ~mask) | result;
            }
            if (result != 0) {
                sr = (uint16_t)(sr & (uint16_t)~CCR_Z);
            }
            if (result & sign_mask) {
                sr |= CCR_N;
            }
            if (is_sub) {
                uint32_t src_v = (uint32_t)full & mask;
                if (full > (uint64_t)dst) {
                    sr |= CCR_C | CCR_X;
                }
                if (((dst ^ src_v) & (dst ^ result) & sign_mask) != 0) {
                    sr |= CCR_V;
                }
            } else {
                if (full > mask) {
                    sr |= CCR_C | CCR_X;
                }
                if (((~(dst ^ src) & (dst ^ result)) & sign_mask) != 0) {
                    sr |= CCR_V;
                }
            }
            state->sr = sr;
            pc += 2u;
            continue;
        }
        if ((op & 0xF000u) == 0x5000u && ((op >> 6) & 3u) == 3u) {
            uint8_t condition = (uint8_t)((op >> 8) & 0xFu);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);

            if (mode == 1u) {
                int16_t displacement = (int16_t)program_read16(program, size, pc + 2u);
                uint32_t next_pc = pc + 4u;
                if (!oracle_condition_true(state->sr, condition)) {
                    uint16_t counter = (uint16_t)((state->d[reg] & 0xFFFFu) - 1u);
                    state->d[reg] = (state->d[reg] & 0xFFFF0000u) | (uint32_t)counter;
                    if (counter != 0xFFFFu) {
                        next_pc = (uint32_t)((int32_t)(pc + 2u) + (int32_t)displacement);
                    }
                }
                pc = next_pc;
                continue;
            }
            if (mode != 1u && !(mode == 7u && reg >= 2u)) {
                uint8_t value = oracle_condition_true(state->sr, condition) ? 0xFFu : 0x00u;
                uint32_t next_pc = pc + 2u;
                if (!oracle_write_ea(program, size, state, bus, mode, reg,
                                     1u, &next_pc, value)) {
                    return 0;
                }
                pc = next_pc;
                continue;
            }
        }
        if ((op & 0xF000u) == 0x5000u) {
            uint8_t size_code = (uint8_t)((op >> 6) & 3u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t bytes = size_code == 2u ? 4u : (size_code == 1u ? 2u : 1u);
            uint32_t mask = oracle_value_mask(bytes);
            uint32_t sign_mask = oracle_sign_mask(bytes);
            uint32_t src;
            uint32_t dst;
            uint32_t result;
            uint32_t next_pc = pc + 2u;
            uint8_t is_sub = (uint8_t)((op & 0x0100u) != 0);
            uint8_t immediate = (uint8_t)((op >> 9) & 7u);
            if (immediate == 0) {
                immediate = 8;
            }
            if (size_code == 3u || (mode == 7u && reg >= 2u)) {
                return 0;
            }
            src = immediate;
            if (mode == 1u) {
                if (size_code == 0u) {
                    return 0;
                }
                if (is_sub) {
                    state->a[reg] -= src;
                } else {
                    state->a[reg] += src;
                }
                pc = next_pc;
                continue;
            }
            if (mode == 0u) {
                dst = state->d[reg] & mask;
                if (is_sub) {
                    result = (dst - src) & mask;
                } else {
                    result = (dst + src) & mask;
                }
                state->d[reg] = (state->d[reg] & ~mask) | result;
            } else {
                uint32_t addr;
                if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                           bytes, &next_pc, &addr)) {
                    return 0;
                }
                dst = bus_read_size(bus, addr, bytes) & mask;
                if (is_sub) {
                    result = (dst - src) & mask;
                } else {
                    result = (dst + src) & mask;
                }
                bus_write_size(bus, addr, result, bytes);
            }

            state->sr = (uint16_t)(state->sr & 0xFFE0u);
            if (result == 0) state->sr |= CCR_Z;
            if (result & sign_mask) state->sr |= CCR_N;
            if (is_sub) {
                if (src > dst) state->sr |= CCR_C | CCR_X;
                if (((dst ^ src) & (dst ^ result) & sign_mask) != 0) state->sr |= CCR_V;
            } else {
                uint64_t full = (uint64_t)dst + (uint64_t)src;
                if (full > mask) state->sr |= CCR_C | CCR_X;
                if (((~(dst ^ src) & (dst ^ result)) & sign_mask) != 0) state->sr |= CCR_V;
            }
            pc = next_pc;
            continue;
        }
        if ((op & 0xF000u) == 0x6000u && (op & 0xFF00u) != 0x6000u &&
            (op & 0xFF00u) != 0x6100u) {
            uint8_t condition = (uint8_t)((op >> 8) & 0xFu);
            int8_t displacement = (int8_t)(op & 0xFFu);
            uint32_t next_pc = pc + 2u;
            int take = oracle_condition_true(state->sr, condition);
            pc = take ? (uint32_t)((int32_t)next_pc + (int32_t)displacement) : next_pc;
            continue;
        }
        if ((op & 0xF000u) == 0xE000u && ((op >> 6) & 3u) != 3u) {
            uint8_t count_field = (uint8_t)((op >> 9) & 7u);
            uint8_t dir_left = (uint8_t)((op >> 8) & 1u);
            uint8_t size_code = (uint8_t)((op >> 6) & 3u);
            uint8_t count_is_reg = (uint8_t)((op >> 5) & 1u);
            uint8_t kind = (uint8_t)((op >> 3) & 3u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t bytes = size_code == 2u ? 4u : (size_code == 1u ? 2u : 1u);
            uint8_t bits = (uint8_t)(bytes * 8u);
            uint32_t mask = oracle_value_mask(bytes);
            uint32_t sign_mask = oracle_sign_mask(bytes);
            uint8_t count = count_is_reg ? (uint8_t)(state->d[count_field] & 0x3Fu) :
                                           (uint8_t)(count_field == 0u ? 8u : count_field);
            uint32_t result = state->d[reg] & mask;

            if (count == 0u) {
                state->sr = (uint16_t)(state->sr & 0xFFF0u);
                if ((result & mask) == 0) state->sr |= CCR_Z;
                if (result & sign_mask) state->sr |= CCR_N;
                if (kind == 2u && (state->sr & CCR_X)) state->sr |= CCR_C;
                state->d[reg] = (state->d[reg] & ~mask) | (result & mask);
            } else {
                uint8_t last = 0;
                uint8_t v = 0;
                uint8_t x = (state->sr & CCR_X) ? 1u : 0u;
                for (uint8_t i = 0; i < count; ++i) {
                    if (kind == 0u && dir_left) {
                        uint32_t old_sign = result & sign_mask;
                        last = (uint8_t)(old_sign != 0);
                        result = (result << 1) & mask;
                        if ((result & sign_mask) != old_sign) v = 1;
                    } else if (kind == 0u) {
                        last = (uint8_t)(result & 1u);
                        result = (result >> 1) | (result & sign_mask);
                    } else if (kind == 1u && dir_left) {
                        last = (uint8_t)((result & sign_mask) != 0);
                        result = (result << 1) & mask;
                    } else if (kind == 1u) {
                        last = (uint8_t)(result & 1u);
                        result >>= 1;
                    } else if (kind == 2u && dir_left) {
                        last = (uint8_t)((result & sign_mask) != 0);
                        result = ((result << 1) & mask) | x;
                        x = last;
                    } else if (kind == 2u) {
                        last = (uint8_t)(result & 1u);
                        result = (result >> 1) | (x ? sign_mask : 0u);
                        x = last;
                    } else if (dir_left) {
                        last = (uint8_t)((result & sign_mask) != 0);
                        result = ((result << 1) & mask) | last;
                    } else {
                        last = (uint8_t)(result & 1u);
                        result = (result >> 1) | (last ? sign_mask : 0u);
                    }
                }

                state->sr = (uint16_t)(state->sr & (kind == 3u ? 0xFFF0u : 0xFFE0u));
                if ((result & mask) == 0) state->sr |= CCR_Z;
                if (result & sign_mask) state->sr |= CCR_N;
                if (kind == 2u) {
                    if (x) state->sr |= CCR_C | CCR_X;
                } else if (last) {
                    state->sr |= (uint16_t)(CCR_C | (kind == 3u ? 0u : CCR_X));
                }
                if (kind == 0u && dir_left && v) state->sr |= CCR_V;
                state->d[reg] = (state->d[reg] & ~mask) | (result & mask);
            }
            (void)bits;
            pc += 2u;
            continue;
        }
        if ((op & 0xF000u) == 0xE000u && ((op >> 6) & 3u) == 3u) {
            uint8_t dir_left = (uint8_t)((op >> 8) & 1u);
            uint8_t kind = (uint8_t)((op >> 9) & 3u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint32_t next_pc = pc + 2u;
            uint32_t addr;
            uint32_t result;
            uint32_t mask = 0x0000FFFFu;
            uint32_t sign_mask = 0x00008000u;
            uint8_t last = 0;
            uint8_t v = 0;
            uint8_t x = (state->sr & CCR_X) ? 1u : 0u;

            if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                       2u, &next_pc, &addr)) {
                return 0;
            }
            result = bus_read16(bus, addr);
            if (kind == 0u && dir_left) {
                uint32_t old_sign = result & sign_mask;
                last = (uint8_t)(old_sign != 0);
                result = (result << 1) & mask;
                if ((result & sign_mask) != old_sign) v = 1;
            } else if (kind == 0u) {
                last = (uint8_t)(result & 1u);
                result = (result >> 1) | (result & sign_mask);
            } else if (kind == 1u && dir_left) {
                last = (uint8_t)((result & sign_mask) != 0);
                result = (result << 1) & mask;
            } else if (kind == 1u) {
                last = (uint8_t)(result & 1u);
                result >>= 1;
            } else if (kind == 2u && dir_left) {
                last = (uint8_t)((result & sign_mask) != 0);
                result = ((result << 1) & mask) | x;
                x = last;
            } else if (kind == 2u) {
                last = (uint8_t)(result & 1u);
                result = (result >> 1) | (x ? sign_mask : 0u);
                x = last;
            } else if (dir_left) {
                last = (uint8_t)((result & sign_mask) != 0);
                result = ((result << 1) & mask) | last;
            } else {
                last = (uint8_t)(result & 1u);
                result = (result >> 1) | (last ? sign_mask : 0u);
            }

            bus_write16(bus, addr, (uint16_t)(result & mask));
            state->sr = (uint16_t)(state->sr & (kind == 3u ? 0xFFF0u : 0xFFE0u));
            if ((result & mask) == 0) state->sr |= CCR_Z;
            if (result & sign_mask) state->sr |= CCR_N;
            if (kind == 2u) {
                if (x) state->sr |= CCR_C | CCR_X;
            } else if (last) {
                state->sr |= (uint16_t)(CCR_C | (kind == 3u ? 0u : CCR_X));
            }
            if (kind == 0u && dir_left && v) state->sr |= CCR_V;
            pc = next_pc;
            continue;
        }
        if ((op & 0xFFF8u) == 0x4E60u) {
            uint8_t reg = (uint8_t)(op & 7u);
            if (!oracle_is_supervisor(state)) {
                pc = oracle_privilege_violation(state, bus, pc);
                continue;
            }
            state->usp = state->a[reg];
            pc += 2u;
            continue;
        }
        if ((op & 0xFFF8u) == 0x4E68u) {
            uint8_t reg = (uint8_t)(op & 7u);
            if (!oracle_is_supervisor(state)) {
                pc = oracle_privilege_violation(state, bus, pc);
                continue;
            }
            state->a[reg] = state->usp;
            pc += 2u;
            continue;
        }
        if (op == 0x4EB9u) {
            uint32_t target = program_read32(program, size, pc + 2u);
            state->a[7] -= 4u;
            bus_write32(bus, state->a[7], pc + 6u);
            pc = target;
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
            state->a[reg] = (uint32_t)((int32_t)(pc + 2u) + (int32_t)disp);
            pc += 4u;
            continue;
        }
        if ((op & 0xF1C0u) == 0x4180u) {
            uint8_t dst_reg = (uint8_t)((op >> 9) & 7u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint32_t next_pc = pc + 2u;
            uint32_t bound;
            int16_t value = (int16_t)(state->d[dst_reg] & 0xFFFFu);
            if (!oracle_read_ea(program, size, state, bus, mode, reg,
                                2u, &next_pc, &bound)) {
                return 0;
            }
            if (value < 0 || value > (int16_t)bound) {
                uint16_t saved_sr;
                if (value < 0) {
                    state->sr = (uint16_t)(state->sr | CCR_N);
                } else {
                    state->sr = (uint16_t)(state->sr & (uint16_t)~CCR_N);
                }
                saved_sr = state->sr;
                oracle_push_exception_frame_with_sr(
                    state,
                    bus,
                    next_pc,
                    saved_sr,
                    (uint16_t)((saved_sr | SR_S) & (uint16_t)~SR_T));
                pc = bus_read32(bus, 6u * 4u);
                continue;
            }
            pc = next_pc;
            continue;
        }
        if ((op & 0xF1C0u) == 0x41C0u) {
            uint8_t dst_reg = (uint8_t)((op >> 9) & 7u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint32_t next_pc = pc + 2u;
            uint32_t addr;
            if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                       4u, &next_pc, &addr)) {
                return 0;
            }
            state->a[dst_reg] = addr;
            pc = next_pc;
            continue;
        }
        if ((op & 0xF1FFu) == 0x41FAu) {
            uint8_t reg = (uint8_t)((op >> 9) & 7u);
            int16_t disp = (int16_t)program_read16(program, size, pc + 2u);
            state->a[reg] = (uint32_t)((int32_t)(pc + 2u) + (int32_t)disp);
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
        if ((op & 0xFFC0u) == 0x40C0u ||
            (op & 0xFFC0u) == 0x42C0u) {
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint32_t next_pc = pc + 2u;
            uint32_t value = ((op & 0xFFC0u) == 0x40C0u) ?
                state->sr : (state->sr & 0x001Fu);
            if (!oracle_write_ea(program, size, state, bus, mode, reg,
                                 2u, &next_pc, value)) {
                return 0;
            }
            pc = next_pc;
            continue;
        }
        if ((op & 0xFFC0u) == 0x44C0u ||
            (op & 0xFFC0u) == 0x46C0u) {
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint32_t next_pc = pc + 2u;
            uint32_t value;
            if ((op & 0xFFC0u) == 0x46C0u && !oracle_is_supervisor(state)) {
                pc = oracle_privilege_violation(state, bus, pc);
                continue;
            }
            if (!oracle_read_ea(program, size, state, bus, mode, reg,
                                2u, &next_pc, &value)) {
                return 0;
            }
            if ((op & 0xFFC0u) == 0x46C0u) {
                oracle_set_sr(state, (uint16_t)value);
            } else {
                state->sr = (uint16_t)((state->sr & 0xFFE0u) | (value & 0x001Fu));
            }
            pc = next_pc;
            continue;
        }
        if ((op & 0xFF00u) == 0x4000u ||
            (op & 0xFF00u) == 0x4400u ||
            (op & 0xFF00u) == 0x4600u) {
            uint8_t size_code = (uint8_t)((op >> 6) & 3u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t bytes = size_code == 2u ? 4u : (size_code == 1u ? 2u : 1u);
            uint32_t mask = oracle_value_mask(bytes);
            uint32_t sign_mask = oracle_sign_mask(bytes);
            uint32_t next_pc = pc + 2u;
            uint32_t value;
            uint32_t result;
            uint32_t src;
            uint8_t is_not = (uint8_t)((op & 0xFF00u) == 0x4600u);
            uint8_t is_negx = (uint8_t)((op & 0xFF00u) == 0x4000u);
            uint8_t x = (state->sr & CCR_X) ? 1u : 0u;

            if (size_code == 3u || mode == 1u ||
                (mode == 7u && reg >= 2u)) {
                return 0;
            }
            if (mode == 0u) {
                value = state->d[reg] & mask;
                src = (value + (is_negx ? x : 0u)) & mask;
                result = is_not ? ((~value) & mask) :
                    ((0u - value - (is_negx ? x : 0u)) & mask);
                state->d[reg] = (state->d[reg] & ~mask) | result;
            } else {
                uint32_t addr;
                if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                           bytes, &next_pc, &addr)) {
                    return 0;
                }
                value = bus_read_size(bus, addr, bytes) & mask;
                src = (value + (is_negx ? x : 0u)) & mask;
                result = is_not ? ((~value) & mask) :
                    ((0u - value - (is_negx ? x : 0u)) & mask);
                bus_write_size(bus, addr, result, bytes);
            }

            if (is_not) {
                oracle_set_nz_size(state, result, bytes);
            } else if (is_negx) {
                uint16_t sr = (uint16_t)(state->sr & 0xFFE4u);
                if (result != 0) sr = (uint16_t)(sr & (uint16_t)~CCR_Z);
                if (result & sign_mask) sr |= CCR_N;
                if (value != 0 || x) sr |= CCR_C | CCR_X;
                if ((src & result & sign_mask) != 0) sr |= CCR_V;
                state->sr = sr;
            } else {
                state->sr = (uint16_t)(state->sr & 0xFFE0u);
                if (result == 0) state->sr |= CCR_Z;
                if (result & sign_mask) state->sr |= CCR_N;
                if (value != 0) state->sr |= CCR_C | CCR_X;
                if (value == sign_mask) state->sr |= CCR_V;
            }
            pc = next_pc;
            continue;
        }
        if ((op & 0xFFC0u) == 0x4800u) {
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint32_t next_pc = pc + 2u;
            uint8_t value;
            uint8_t x = (state->sr & CCR_X) ? 1u : 0u;
            uint8_t decimal;
            uint8_t borrow;
            uint8_t result_decimal;
            uint8_t result;
            uint32_t addr = 0;

            if (mode == 0u) {
                value = (uint8_t)(state->d[reg] & 0xFFu);
            } else {
                if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                           1u, &next_pc, &addr)) {
                    return 0;
                }
                value = bus_read8(bus, addr);
            }

            decimal = (uint8_t)(((value >> 4) & 0x0Fu) * 10u + (value & 0x0Fu));
            borrow = (uint8_t)((decimal + x) != 0u);
            result_decimal = (uint8_t)((100u - decimal - x) % 100u);
            result = (uint8_t)(((result_decimal / 10u) << 4) | (result_decimal % 10u));

            if (mode == 0u) {
                state->d[reg] = (state->d[reg] & 0xFFFFFF00u) | (uint32_t)result;
            } else {
                bus_write8(bus, addr, result);
            }
            state->sr = (uint16_t)(state->sr & 0xFFE4u);
            if (result != 0) state->sr = (uint16_t)(state->sr & (uint16_t)~CCR_Z);
            if (result & 0x80u) state->sr |= CCR_N;
            if (borrow) state->sr |= CCR_C | CCR_X;
            pc = next_pc;
            continue;
        }
        if ((op & 0xFFF8u) == 0x4880u) {
            uint8_t reg = (uint8_t)(op & 7u);
            uint16_t result = (uint16_t)(int16_t)(int8_t)(state->d[reg] & 0xFFu);
            state->d[reg] = (state->d[reg] & 0xFFFF0000u) | (uint32_t)result;
            oracle_set_nz16(state, result);
            pc += 2u;
            continue;
        }
        if ((op & 0xFFF8u) == 0x48C0u) {
            uint8_t reg = (uint8_t)(op & 7u);
            state->d[reg] = (uint32_t)(int32_t)(int16_t)(state->d[reg] & 0xFFFFu);
            oracle_set_nz32(state, state->d[reg]);
            pc += 2u;
            continue;
        }
        if ((op & 0xFFF8u) == 0x4840u) {
            uint8_t reg = (uint8_t)(op & 7u);
            state->d[reg] = (state->d[reg] << 16) | (state->d[reg] >> 16);
            oracle_set_nz32(state, state->d[reg]);
            pc += 2u;
            continue;
        }
        if ((op & 0xFF80u) == 0x4880u && ((op >> 3) & 7u) != 0u) {
            uint8_t bytes = (op & 0x0040u) ? 4u : 2u;
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint16_t mask = program_read16(program, size, pc + 2u);
            uint32_t next_pc = pc + 4u;
            uint32_t addr;

            if (mode == 4u) {
                addr = state->a[reg];
                for (uint8_t bit = 0; bit < 16u; ++bit) {
                    if ((mask & (uint16_t)(1u << bit)) != 0) {
                        uint8_t is_addr = (uint8_t)(bit < 8u);
                        uint8_t movem_reg = is_addr ? (uint8_t)(7u - bit) :
                                                       (uint8_t)(15u - bit);
                        uint32_t value = is_addr ? state->a[movem_reg] :
                                                   state->d[movem_reg];
                        addr -= bytes;
                        bus_write_size(bus, addr, value, bytes);
                    }
                }
                state->a[reg] = addr;
            } else {
                if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                           bytes, &next_pc, &addr)) {
                    return 0;
                }
                for (uint8_t i = 0; i < 16u; ++i) {
                    if ((mask & (uint16_t)(1u << i)) != 0) {
                        uint32_t value = i < 8u ? state->d[i] : state->a[i - 8u];
                        bus_write_size(bus, addr, value, bytes);
                        addr += bytes;
                    }
                }
            }
            pc = next_pc;
            continue;
        }
        if ((op & 0xFF80u) == 0x4C80u && ((op >> 3) & 7u) != 0u) {
            uint8_t bytes = (op & 0x0040u) ? 4u : 2u;
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint16_t mask = program_read16(program, size, pc + 2u);
            uint32_t next_pc = pc + 4u;
            uint32_t addr;

            if (mode == 3u) {
                addr = state->a[reg];
            } else if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                              bytes, &next_pc, &addr)) {
                return 0;
            }
            for (uint8_t i = 0; i < 16u; ++i) {
                if ((mask & (uint16_t)(1u << i)) != 0) {
                    uint32_t value = bus_read_size(bus, addr, bytes);
                    if (bytes == 2u) {
                        value = (uint32_t)(int32_t)(int16_t)value;
                    }
                    if (i < 8u) {
                        state->d[i] = value;
                    } else {
                        state->a[i - 8u] = value;
                    }
                    addr += bytes;
                }
            }
            if (mode == 3u) {
                state->a[reg] = addr;
            }
            pc = next_pc;
            continue;
        }
        if ((op & 0xFFC0u) == 0x4840u) {
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint32_t next_pc = pc + 2u;
            uint32_t addr;
            if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                       4u, &next_pc, &addr)) {
                return 0;
            }
            state->a[7] -= 4u;
            bus_write32(bus, state->a[7], addr);
            pc = next_pc;
            continue;
        }
        if ((op & 0xFFC0u) == 0x4AC0u) {
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint32_t next_pc = pc + 2u;
            uint32_t value;

            if (mode == 0u) {
                value = state->d[reg] & 0xFFu;
                oracle_set_nz8(state, (uint8_t)value);
                state->d[reg] = (state->d[reg] & 0xFFFFFF00u) |
                    (uint32_t)(uint8_t)(value | 0x80u);
            } else {
                uint32_t addr;
                if (!oracle_ea_memory_addr(program, size, state, mode, reg,
                                           1u, &next_pc, &addr)) {
                    return 0;
                }
                value = bus_read8(bus, addr);
                oracle_set_nz8(state, (uint8_t)value);
                bus_write8(bus, addr, (uint8_t)(value | 0x80u));
            }
            pc = next_pc;
            continue;
        }
        if ((op & 0xFF00u) == 0x4A00u) {
            uint8_t size_code = (uint8_t)((op >> 6) & 3u);
            uint8_t mode = (uint8_t)((op >> 3) & 7u);
            uint8_t reg = (uint8_t)(op & 7u);
            uint8_t bytes = size_code == 2u ? 4u : (size_code == 1u ? 2u : 1u);
            uint32_t next_pc = pc + 2u;
            uint32_t value;

            if (size_code == 3u || mode == 1u ||
                (mode == 7u && reg >= 2u)) {
                return 0;
            }
            if (!oracle_read_ea(program, size, state, bus, mode, reg,
                                bytes, &next_pc, &value)) {
                return 0;
            }
            oracle_set_nz_size(state, value, bytes);
            pc = next_pc;
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

int ng_m68k_take_interrupt(uint8_t current_mask, uint8_t *level, uint8_t *vector) {
    ++g_interrupt_poll_count;
    if (g_pending_interrupt_after_poll != 0u &&
        g_interrupt_poll_count < g_pending_interrupt_after_poll) {
        return 0;
    }
    if (g_pending_interrupt_level == 0u ||
        g_pending_interrupt_level <= current_mask) {
        return 0;
    }
    *level = g_pending_interrupt_level;
    *vector = g_pending_interrupt_vector;
    g_pending_interrupt_level = 0;
    g_pending_interrupt_vector = 0;
    g_pending_interrupt_after_poll = 0;
    return 1;
}

void ng_m68k_stop_until_interrupt(uint16_t sr) {
    (void)sr;
    if (g_arm_stop_interrupt_level != 0u) {
        g_pending_interrupt_level = g_arm_stop_interrupt_level;
        g_pending_interrupt_vector = g_arm_stop_interrupt_vector;
        g_arm_stop_interrupt_level = 0;
        g_arm_stop_interrupt_vector = 0;
    }
}

void ng_m68k_reset_devices(void) {
    ++g_reset_device_count;
}

int main(void) {
    uint8_t program[NG_EXEC_FIXTURE_SIZE];
    NgM68kState expected_state;
    uint8_t expected_bus[BUS_SIZE];

    ng_exec_fixture_fill(program, (uint32_t)sizeof(program));
    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    expected_state.sr = SR_S;
    bus_write8(expected_bus, 0x100Cu, 0xAAu);
    bus_write32(expected_bus, 0x1010u, 0x12345678u);
    bus_write8(expected_bus, 0x101Au, 0xAFu);
    bus_write8(expected_bus, 0x0125u, 0x3Fu);
    bus_write8(expected_bus, 0x0126u, 0x10u);
    bus_write8(expected_bus, 0x0127u, 0x30u);
    bus_write8(expected_bus, 0x0128u, 0x20u);
    bus_write8(expected_bus, 0x012Bu, 0x55u);
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0, &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_ng_m68k.sr = SR_S;
    ng68k_write8(0x100Cu, 0xAAu);
    ng68k_write32(0x1010u, 0x12345678u);
    ng68k_write8(0x101Au, 0xAFu);
    ng68k_write8(0x0125u, 0x3Fu);
    ng68k_write8(0x0126u, 0x10u);
    ng68k_write8(0x0127u, 0x30u);
    ng68k_write8(0x0128u, 0x20u);
    ng68k_write8(0x012Bu, 0x55u);
    g_dispatch_miss_count = 0;
    g_last_dispatch_miss = 0;

    ng_generated_call(0x00000000u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_ng_m68k.d, expected_state.d, sizeof(g_ng_m68k.d)) == 0);
    CHECK(memcmp(g_ng_m68k.a, expected_state.a, sizeof(g_ng_m68k.a)) == 0);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.d[0] == 0u);
    CHECK(g_ng_m68k.d[2] == 0xFEFF0000u);
    CHECK(g_ng_m68k.d[3] == 0xFFFFFF00u);
    CHECK((g_ng_m68k.d[4] & 0xFFu) == 0x0Eu);
    CHECK((g_ng_m68k.d[5] & 0xFFFFu) == 0x1357u);
    CHECK((g_ng_m68k.d[6] & 0xFFFFu) == 0x1357u);
    CHECK(g_ng_m68k.d[7] == 0x00000005u);
    CHECK(g_ng_m68k.a[0] == 0x00000132u);
    CHECK(g_ng_m68k.a[1] == 0x00000125u);
    CHECK(g_ng_m68k.a[2] == 0x00000108u);
    CHECK(g_ng_m68k.a[3] == 0x00000142u);
    CHECK(g_ng_m68k.a[4] == 0x00000190u);
    CHECK(g_ng_m68k.a[5] == 0x00000190u);
    CHECK(g_ng_m68k.a[6] == 0x00000192u);
    CHECK(g_ng_m68k.a[7] == 0x0000013Cu);
    CHECK(g_ng_m68k.usp == 0x00000190u);
    CHECK(ng68k_read16(0x0068u) == 0x0000u);
    CHECK(ng68k_read16(0x00ACu) == 0x0000u);
    CHECK(ng68k_read8(0x0120u) == 0x5Au);
    CHECK(ng68k_read8(0x0124u) == 0x5Au);
    CHECK(ng68k_read8(0x0125u) == 0x0Fu);
    CHECK(ng68k_read8(0x0126u) == 0x0Fu);
    CHECK(ng68k_read8(0x0127u) == 0x3Fu);
    CHECK(ng68k_read8(0x0128u) == 0x1Fu);
    CHECK(ng68k_read8(0x0129u) == 0x01u);
    CHECK(ng68k_read8(0x012Au) == 0x04u);
    CHECK(ng68k_read8(0x012Bu) == 0xAAu);
    CHECK(ng68k_read8(0x012Cu) == 0x0Fu);
    CHECK(ng68k_read8(0x012Du) == 0x00u);
    CHECK(ng68k_read8(0x012Eu) == 0x0Fu);
    CHECK(ng68k_read8(0x012Fu) == 0x0Fu);
    CHECK(ng68k_read8(0x0130u) == 0xF1u);
    CHECK(ng68k_read8(0x0131u) == 0xFFu);
    CHECK(ng68k_read32(0x0138u) == 0x00000160u);
    CHECK(ng68k_read32(0x013Cu) == 0x00000141u);
    CHECK(ng68k_read32(0x0180u) == 0x00000034u);
    CHECK(ng68k_read32(0x0184u) == 0x00000012u);
    CHECK(ng68k_read16(0x0188u) == 0x201Bu);
    CHECK(ng68k_read16(0x018Au) == 0x0006u);
    CHECK(ng68k_read8(0x018Cu) == 0xFDu);
    CHECK(ng68k_read8(0x018Du) == 0x81u);
    CHECK(ng68k_read16(0x0192u) == 0x0004u);
    CHECK(ng68k_read8(0x0194u) == 0x54u);
    CHECK(ng68k_read8(0x0195u) == 0x99u);
    CHECK(ng68k_read8(0x0196u) == 0x38u);
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
    CHECK(ng68k_read8(0x101Eu) == 0x00u);
    CHECK((g_ng_m68k.sr & CCR_X) != 0);
    CHECK((g_ng_m68k.sr & CCR_N) != 0);

    ng_generated_call(0x00DEADu);
    CHECK(g_dispatch_miss_count == 1);
    CHECK(g_last_dispatch_miss == 0x00DEADu);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_ng_m68k.sr = SR_S;
    g_dispatch_miss_count = 0;
    g_last_dispatch_miss = 0;

    ng_generated_call(0x000002D0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001F0u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read32(0x000001ECu) == 0x000002DCu);
    CHECK(ng68k_read32(0x00001000u) == 0x000002DCu);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    expected_state.usp = 0x00000100u;
    bus_write32(expected_bus, 0x00000080u, 0x00000300u);
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x000002F0u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_ng_m68k.usp = 0x00000100u;
    ng68k_write32(0x00000080u, 0x00000300u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x000002F0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(g_ng_m68k.usp == expected_state.usp);
    CHECK(g_ng_m68k.ssp == expected_state.ssp);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001EAu);
    CHECK(g_ng_m68k.usp == 0x00000100u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001EAu) == 0x0000u);
    CHECK(ng68k_read32(0x000001ECu) == 0x000002F6u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    expected_state.usp = 0x00000100u;
    bus_write16(expected_bus, 0x000001F0u, 0x0000u);
    bus_write32(expected_bus, 0x000001F2u, 0x00000340u);
    bus_write32(expected_bus, 0x00000080u, 0x00000360u);
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000330u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_ng_m68k.usp = 0x00000100u;
    ng68k_write16(0x000001F0u, 0x0000u);
    ng68k_write32(0x000001F2u, 0x00000340u);
    ng68k_write32(0x00000080u, 0x00000360u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000330u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(g_ng_m68k.usp == expected_state.usp);
    CHECK(g_ng_m68k.ssp == expected_state.ssp);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001F0u);
    CHECK(g_ng_m68k.usp == 0x00000100u);
    CHECK(g_ng_m68k.ssp == 0x000001F6u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001F0u) == 0x0000u);
    CHECK(ng68k_read32(0x000001F2u) == 0x00000342u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.a[7] = 0x00000100u;
    expected_state.ssp = 0x000001F0u;
    bus_write32(expected_bus, 0x00000080u, 0x00000360u);
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000350u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.a[7] = 0x00000100u;
    g_ng_m68k.ssp = 0x000001F0u;
    ng68k_write32(0x00000080u, 0x00000360u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000350u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(g_ng_m68k.usp == expected_state.usp);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001EAu);
    CHECK(g_ng_m68k.usp == 0x00000100u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001EAu) == 0x0000u);
    CHECK(ng68k_read32(0x000001ECu) == 0x00000352u);

    {
        static const struct {
            uint32_t start;
            uint32_t fault_pc;
        } cases[] = {
            {0x00000370u, 0x00000370u}, /* MOVE #imm,SR */
            {0x000003A0u, 0x000003A0u}, /* STOP */
            {0x000003B0u, 0x000003B0u}, /* RTE */
            {0x000003C0u, 0x000003C0u}, /* MOVE An,USP */
            {0x000003D0u, 0x000003D0u}, /* RESET */
            {0x000003E0u, 0x000003E0u}, /* ORI #imm,SR */
        };
        for (unsigned i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
            memset(&expected_state, 0, sizeof(expected_state));
            memset(expected_bus, 0, sizeof(expected_bus));
            expected_state.a[0] = 0x0000CAFEu;
            expected_state.a[7] = 0x00000100u;
            expected_state.ssp = 0x000001F0u;
            bus_write32(expected_bus, 0x00000020u, 0x00000390u);
            CHECK(oracle_exec(program, (uint32_t)sizeof(program), cases[i].start,
                              &expected_state, expected_bus, 0));

            memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
            memset(g_bus, 0, sizeof(g_bus));
            g_ng_m68k.a[0] = 0x0000CAFEu;
            g_ng_m68k.a[7] = 0x00000100u;
            g_ng_m68k.ssp = 0x000001F0u;
            ng68k_write32(0x00000020u, 0x00000390u);
            g_dispatch_miss_count = 0;

            ng_generated_call(cases[i].start);

            CHECK(g_dispatch_miss_count == 0);
            CHECK(g_ng_m68k.a[0] == expected_state.a[0]);
            CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
            CHECK(g_ng_m68k.usp == expected_state.usp);
            CHECK(g_ng_m68k.sr == expected_state.sr);
            CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
            CHECK(g_ng_m68k.a[7] == 0x000001EAu);
            CHECK(g_ng_m68k.usp == 0x00000100u);
            CHECK(g_ng_m68k.sr == 0x2700u);
            CHECK(ng68k_read16(0x000001EAu) == 0x0000u);
            CHECK(ng68k_read32(0x000001ECu) == cases[i].fault_pc);
        }
    }

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    bus_write32(expected_bus, 28u * 4u, 0x00000410u);
    g_oracle_stop_interrupt_level = 4u;
    g_oracle_stop_interrupt_vector = 28u;
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000400u,
                      &expected_state, expected_bus, 0));
    g_oracle_stop_interrupt_level = 0;
    g_oracle_stop_interrupt_vector = 0;

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(28u * 4u, 0x00000410u);
    g_pending_interrupt_level = 0;
    g_pending_interrupt_vector = 0;
    g_arm_stop_interrupt_level = 4u;
    g_arm_stop_interrupt_vector = 28u;
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000400u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_pending_interrupt_level == 0);
    CHECK(g_arm_stop_interrupt_level == 0);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.d[0] == 0x00000007u);
    CHECK(g_ng_m68k.a[7] == 0x000001F0u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001EAu) == 0x2000u);
    CHECK(ng68k_read32(0x000001ECu) == 0x00000404u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    g_oracle_stop_interrupt_level = 4u;
    g_oracle_stop_interrupt_vector = 28u;
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000406u,
                      &expected_state, expected_bus, 0));
    CHECK(g_oracle_stop_interrupt_level == 4u);
    CHECK(g_oracle_stop_interrupt_vector == 28u);
    g_oracle_stop_interrupt_level = 0;
    g_oracle_stop_interrupt_vector = 0;

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_pending_interrupt_level = 0;
    g_pending_interrupt_vector = 0;
    g_arm_stop_interrupt_level = 4u;
    g_arm_stop_interrupt_vector = 28u;
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000406u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_pending_interrupt_level == 4u);
    CHECK(g_pending_interrupt_vector == 28u);
    CHECK(g_arm_stop_interrupt_level == 0);
    CHECK(g_arm_stop_interrupt_vector == 0);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.d[0] == 0x00000000u);
    CHECK(g_ng_m68k.a[7] == 0x000001F0u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001EAu) == 0x0000u);
    g_pending_interrupt_level = 0;
    g_pending_interrupt_vector = 0;

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    bus_write32(expected_bus, 26u * 4u, 0x00000430u);
    g_oracle_pending_interrupt_level = 2u;
    g_oracle_pending_interrupt_vector = 26u;
    g_oracle_pending_interrupt_after_poll = 2u;
    g_oracle_interrupt_poll_count = 0;
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000420u,
                      &expected_state, expected_bus, 0));
    CHECK(g_oracle_pending_interrupt_level == 0);
    g_oracle_pending_interrupt_vector = 0;
    g_oracle_pending_interrupt_after_poll = 0;
    g_oracle_interrupt_poll_count = 0;

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(26u * 4u, 0x00000430u);
    g_pending_interrupt_level = 2u;
    g_pending_interrupt_vector = 26u;
    g_pending_interrupt_after_poll = 2u;
    g_interrupt_poll_count = 0;
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000420u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_pending_interrupt_level == 0);
    CHECK(g_pending_interrupt_vector == 0);
    CHECK(g_pending_interrupt_after_poll == 0);
    CHECK(g_interrupt_poll_count >= 2u);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.d[1] == expected_state.d[1]);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.d[0] == 0x00000001u);
    CHECK(g_ng_m68k.d[1] == 0x00000002u);
    CHECK(g_ng_m68k.a[7] == 0x000001F0u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(g_ng_m68k.pc == 0x00000428u);
    CHECK(ng68k_read16(0x000001EAu) == 0x2000u);
    CHECK(ng68k_read32(0x000001ECu) == 0x00000422u);
    g_interrupt_poll_count = 0;

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(9u * 4u, 0x00000450u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000440u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.d[2] == 0x00000003u);
    CHECK(g_ng_m68k.a[7] == 0x000001EAu);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(g_ng_m68k.pc == 0x00000454u);
    CHECK(ng68k_read16(0x000001EAu) == (uint16_t)(SR_S | SR_T));
    CHECK(ng68k_read32(0x000001ECu) == 0x00000442u);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T | CCR_N);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(9u * 4u, 0x00000470u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000460u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001EAu);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001EAu) == (uint16_t)(SR_S | SR_T | CCR_N));
    CHECK(ng68k_read32(0x000001ECu) == 0x00000466u);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T);
    g_ng_m68k.d[0] = 0x00000001u;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(9u * 4u, 0x00000490u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000480u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK((g_ng_m68k.d[0] & 0xFFFFu) == 0x0000u);
    CHECK(g_ng_m68k.a[7] == 0x000001EAu);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001EAu) == (uint16_t)(SR_S | SR_T));
    CHECK(ng68k_read32(0x000001ECu) == 0x00000486u);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(9u * 4u, 0x000004C0u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x000004A0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001E6u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001E6u) == (uint16_t)(SR_S | SR_T));
    CHECK(ng68k_read32(0x000001E8u) == 0x000004B0u);
    CHECK(ng68k_read32(0x000001ECu) == 0x000004A6u);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(0x000001F0u, 0x000004D0u);
    ng68k_write32(9u * 4u, 0x000004F0u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x000004E0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001EEu);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001EEu) == (uint16_t)(SR_S | SR_T));
    CHECK(ng68k_read32(0x000001F0u) == 0x000004D0u);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(9u * 4u, 0x00000510u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000500u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001EAu);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001EAu) == 0x2700u);
    CHECK(ng68k_read32(0x000001ECu) == 0x00000504u);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write16(0x000001F0u, 0x2700u);
    ng68k_write32(0x000001F2u, 0x00000530u);
    ng68k_write32(9u * 4u, 0x00000510u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000520u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001F0u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001F0u) == 0x2700u);
    CHECK(ng68k_read32(0x000001F2u) == 0x00000530u);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(32u * 4u, 0x00000550u);
    ng68k_write32(9u * 4u, 0x00000560u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000540u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001E4u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001E4u) == SR_S);
    CHECK(ng68k_read32(0x000001E6u) == 0x00000550u);
    CHECK(ng68k_read16(0x000001EAu) == (uint16_t)(SR_S | SR_T));
    CHECK(ng68k_read32(0x000001ECu) == 0x00000542u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = (uint16_t)(SR_S | CCR_V);
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    bus_write32(expected_bus, 7u * 4u, 0x00000590u);
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000580u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | CCR_V);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(7u * 4u, 0x00000590u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000580u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(g_ng_m68k.a[7] == 0x000001EAu);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001EAu) == (uint16_t)(SR_S | CCR_V));
    CHECK(ng68k_read32(0x000001ECu) == 0x00000582u);
    CHECK(g_ng_m68k.pc == 0x00000594u);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T | CCR_V);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(7u * 4u, 0x00000590u);
    ng68k_write32(9u * 4u, 0x000005A0u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000580u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001E4u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001E4u) == (uint16_t)(SR_S | CCR_V));
    CHECK(ng68k_read32(0x000001E6u) == 0x00000590u);
    CHECK(ng68k_read16(0x000001EAu) == (uint16_t)(SR_S | SR_T | CCR_V));
    CHECK(ng68k_read32(0x000001ECu) == 0x00000582u);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T);
    g_ng_m68k.d[0] = 0x0000000Bu;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(6u * 4u, 0x000005D0u);
    ng68k_write32(9u * 4u, 0x000005E0u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x000005C0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001E4u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001E4u) == SR_S);
    CHECK(ng68k_read32(0x000001E6u) == 0x000005D0u);
    CHECK(ng68k_read16(0x000001EAu) == (uint16_t)(SR_S | SR_T));
    CHECK(ng68k_read32(0x000001ECu) == 0x000005C4u);

    {
        const struct {
            uint32_t d0;
            uint16_t initial_sr;
            uint16_t saved_sr;
        } chk_cases[] = {
            {0x0000FFFFu, SR_S, (uint16_t)(SR_S | CCR_N)},
            {0x0000000Bu, (uint16_t)(SR_S | CCR_N), SR_S},
        };

        for (uint32_t i = 0; i < sizeof(chk_cases) / sizeof(chk_cases[0]); ++i) {
            memset(&expected_state, 0, sizeof(expected_state));
            memset(expected_bus, 0, sizeof(expected_bus));
            expected_state.sr = chk_cases[i].initial_sr;
            expected_state.d[0] = chk_cases[i].d0;
            expected_state.a[7] = 0x000001F0u;
            expected_state.ssp = expected_state.a[7];
            bus_write32(expected_bus, 6u * 4u, 0x000005D0u);
            CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x000005C0u,
                              &expected_state, expected_bus, 0));

            memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
            memset(g_bus, 0, sizeof(g_bus));
            g_ng_m68k.sr = chk_cases[i].initial_sr;
            g_ng_m68k.d[0] = chk_cases[i].d0;
            g_ng_m68k.a[7] = 0x000001F0u;
            g_ng_m68k.ssp = g_ng_m68k.a[7];
            ng68k_write32(6u * 4u, 0x000005D0u);
            g_dispatch_miss_count = 0;

            ng_generated_call(0x000005C0u);

            CHECK(g_dispatch_miss_count == 0);
            CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
            CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
            CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
            CHECK(g_ng_m68k.sr == expected_state.sr);
            CHECK(g_ng_m68k.a[7] == 0x000001EAu);
            CHECK(g_ng_m68k.sr == 0x2700u);
            CHECK(ng68k_read16(0x000001EAu) == chk_cases[i].saved_sr);
            CHECK(ng68k_read32(0x000001ECu) == 0x000005C4u);
            CHECK(g_ng_m68k.pc == 0x000005D4u);
        }
    }

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(4u * 4u, 0x00000600u);
    ng68k_write32(9u * 4u, 0x00000610u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x000005F0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001EAu);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(g_ng_m68k.pc == 0x00000604u);
    CHECK(ng68k_read16(0x000001EAu) == (uint16_t)(SR_S | SR_T));
    CHECK(ng68k_read32(0x000001ECu) == 0x000005F2u);
    CHECK(ng68k_read16(0x000001E4u) == 0x0000u);

    {
        const struct {
            uint32_t start;
            uint8_t vector;
            uint32_t handler;
            uint32_t saved_pc;
            uint32_t final_pc;
        } illegal_cases[] = {
            {0x000005F0u, 4u,  0x00000600u, 0x000005F2u, 0x00000604u},
            {0x00000680u, 10u, 0x00000690u, 0x00000682u, 0x00000694u},
            {0x000006A0u, 11u, 0x000006B0u, 0x000006A2u, 0x000006B4u},
        };

        for (uint32_t i = 0; i < sizeof(illegal_cases) / sizeof(illegal_cases[0]); ++i) {
            memset(&expected_state, 0, sizeof(expected_state));
            memset(expected_bus, 0, sizeof(expected_bus));
            expected_state.sr = SR_S;
            expected_state.a[7] = 0x000001F0u;
            expected_state.ssp = expected_state.a[7];
            bus_write32(expected_bus, (uint32_t)illegal_cases[i].vector * 4u,
                        illegal_cases[i].handler);
            CHECK(oracle_exec(program, (uint32_t)sizeof(program), illegal_cases[i].start,
                              &expected_state, expected_bus, 0));

            memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
            memset(g_bus, 0, sizeof(g_bus));
            g_ng_m68k.sr = SR_S;
            g_ng_m68k.a[7] = 0x000001F0u;
            g_ng_m68k.ssp = g_ng_m68k.a[7];
            ng68k_write32((uint32_t)illegal_cases[i].vector * 4u,
                          illegal_cases[i].handler);
            g_dispatch_miss_count = 0;

            ng_generated_call(illegal_cases[i].start);

            CHECK(g_dispatch_miss_count == 0);
            CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
            CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
            CHECK(g_ng_m68k.sr == expected_state.sr);
            CHECK(g_ng_m68k.a[7] == 0x000001EAu);
            CHECK(g_ng_m68k.sr == 0x2700u);
            CHECK(ng68k_read16(0x000001EAu) == SR_S);
            CHECK(ng68k_read32(0x000001ECu) == illegal_cases[i].saved_pc);
            CHECK(g_ng_m68k.pc == illegal_cases[i].final_pc);
        }
    }

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = (uint16_t)(SR_S | SR_T | CCR_X);
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    bus_write32(expected_bus, 4u * 4u, 0x00005F30u);
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00005F20u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T | CCR_X);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(4u * 4u, 0x00005F30u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00005F20u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(ng68k_read16(0x0000128Au) == (uint16_t)(SR_S | CCR_X));
    CHECK(ng68k_read16(0x000001EAu) == (uint16_t)(SR_S | SR_T | CCR_X));
    CHECK(ng68k_read32(0x000001ECu) == 0x00005F22u);
    CHECK(g_ng_m68k.pc == 0x00005F3Au);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_T;
    g_ng_m68k.a[7] = 0x00000100u;
    g_ng_m68k.ssp = 0x000001F0u;
    ng68k_write32(8u * 4u, 0x00000390u);
    ng68k_write32(9u * 4u, 0x00000610u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000370u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001EAu);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001EAu) == SR_T);
    CHECK(ng68k_read32(0x000001ECu) == 0x00000370u);
    CHECK(ng68k_read16(0x000001E4u) == 0x0000u);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = 0;
    g_ng_m68k.a[7] = 0x00000100u;
    g_ng_m68k.ssp = 0x000001F0u;
    ng68k_write32(5u * 4u, 0x00000630u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000620u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001EAu);
    CHECK(g_ng_m68k.usp == 0x00000100u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001EAu) == 0x0000u);
    CHECK(ng68k_read32(0x000001ECu) == 0x00000624u);
    CHECK(ng68k_read16(0x000000FAu) == 0x0000u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C);
    expected_state.d[7] = 0x12345678u;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    bus_write32(expected_bus, 5u * 4u, 0x00000630u);
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000620u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C);
    g_ng_m68k.d[7] = 0x12345678u;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(5u * 4u, 0x00000630u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000620u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.d[7] == expected_state.d[7]);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(g_ng_m68k.d[7] == 0x12345678u);
    CHECK(g_ng_m68k.a[7] == 0x000001EAu);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001EAu) ==
          (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));
    CHECK(ng68k_read32(0x000001ECu) == 0x00000624u);
    CHECK(g_ng_m68k.pc == 0x00000634u);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(5u * 4u, 0x00000630u);
    ng68k_write32(9u * 4u, 0x00000640u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000620u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001E4u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001E4u) == SR_S);
    CHECK(ng68k_read32(0x000001E6u) == 0x00000630u);
    CHECK(ng68k_read16(0x000001EAu) == (uint16_t)(SR_S | SR_T));
    CHECK(ng68k_read32(0x000001ECu) == 0x00000624u);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(9u * 4u, 0x00000450u);
    ng68k_write32(26u * 4u, 0x000005D0u);
    g_pending_interrupt_level = 2u;
    g_pending_interrupt_vector = 26u;
    g_pending_interrupt_after_poll = 2u;
    g_interrupt_poll_count = 0;
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000440u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_pending_interrupt_level == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001E4u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001E4u) == SR_S);
    CHECK(ng68k_read32(0x000001E6u) == 0x00000450u);
    CHECK(ng68k_read16(0x000001EAu) == (uint16_t)(SR_S | SR_T));
    CHECK(ng68k_read32(0x000001ECu) == 0x00000442u);
    g_pending_interrupt_after_poll = 0;
    g_interrupt_poll_count = 0;

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001ECu;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(0x000001ECu, 0x00000660u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000650u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001F0u);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read32(0x000001E8u) == 0x00000656u);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(10u * 4u, 0x00000690u);
    ng68k_write32(9u * 4u, 0x00000610u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000680u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001EAu);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001EAu) == (uint16_t)(SR_S | SR_T));
    CHECK(ng68k_read32(0x000001ECu) == 0x00000682u);
    CHECK(ng68k_read16(0x000001E4u) == 0x0000u);

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | SR_T);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write32(11u * 4u, 0x000006B0u);
    ng68k_write32(9u * 4u, 0x00000610u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x000006A0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001EAu);
    CHECK(g_ng_m68k.sr == 0x2700u);
    CHECK(ng68k_read16(0x000001EAu) == (uint16_t)(SR_S | SR_T));
    CHECK(ng68k_read32(0x000001ECu) == 0x000006A2u);
    CHECK(ng68k_read16(0x000001E4u) == 0x0000u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x000006C0u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x000006C0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK((g_ng_m68k.d[0] & 0xFFFFu) == 0x8001u);
    CHECK(ng68k_read16(0x000001A0u) == 0x2018u); /* LSL count 0: X preserved, C/V/Z clear, N set */
    CHECK(ng68k_read16(0x000001A2u) == 0x2018u); /* ROL count 0: X preserved, C/V/Z clear, N set */
    CHECK(ng68k_read16(0x000001A4u) == 0x2019u); /* ROXL count 0: C mirrors preserved X */

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000700u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000700u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.d[0] == 0x80000000u);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(ng68k_read16(0x000001A6u) == bus_read16(expected_bus, 0x000001A6u));
    CHECK((ng68k_read16(0x000001A6u) & CCR_X) != 0);
    CHECK((ng68k_read16(0x000001A6u) & CCR_V) != 0);
    CHECK((ng68k_read16(0x000001A6u) & CCR_C) == 0);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C);
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000720u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C);
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000720u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.d[0] == 0x12345678u);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(ng68k_read16(0x000001ACu) == bus_read16(expected_bus, 0x000001ACu));
    CHECK(ng68k_read16(0x000001ACu) == (uint16_t)(SR_S | CCR_X));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000740u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000740u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001ECu);
    CHECK(ng68k_read32(0x000001ECu) == 0x12345678u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000760u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000760u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.a[7] == 0x000001ECu);
    CHECK(ng68k_read32(0x000001ECu) == 0x000001F0u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000780u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000780u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.d[1] == expected_state.d[1]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.d[1] == 0xA1B2C3D4u);
    CHECK(ng68k_read8(0x000001C0u) == 0xA1u);
    CHECK(ng68k_read8(0x000001C2u) == 0xB2u);
    CHECK(ng68k_read8(0x000001C4u) == 0xC3u);
    CHECK(ng68k_read8(0x000001C6u) == 0xD4u);
    CHECK(ng68k_read16(0x000001BCu) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x000007C0u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x000007C0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.a[0] == expected_state.a[0]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.d[0] == 0xFFFF8001u);
    CHECK(g_ng_m68k.a[0] == 0x00007FFEu);
    CHECK(ng68k_read16(0x000001D4u) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000820u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000820u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[0] == expected_state.a[0]);
    CHECK(g_ng_m68k.a[1] == expected_state.a[1]);
    CHECK(g_ng_m68k.a[2] == expected_state.a[2]);
    CHECK(g_ng_m68k.a[3] == expected_state.a[3]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.a[0] == 0x000001E1u);
    CHECK(g_ng_m68k.a[1] == 0x000001E3u);
    CHECK(g_ng_m68k.a[2] == 0x000001E5u);
    CHECK(g_ng_m68k.a[3] == 0x000001E7u);
    CHECK(ng68k_read8(0x000001E3u) == 0x00u);
    CHECK(ng68k_read16(0x000001F2u) == (uint16_t)(SR_S | CCR_X | CCR_Z | CCR_C));
    CHECK(ng68k_read8(0x000001E7u) == 0x37u);
    CHECK(ng68k_read16(0x000001F4u) == SR_S);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000890u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000890u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.d[1] == expected_state.d[1]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(ng68k_read16(0x000001F6u) == (uint16_t)(SR_S | CCR_X | CCR_Z | CCR_V | CCR_C));
    CHECK(ng68k_read16(0x000001F8u) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_C));
    CHECK(ng68k_read16(0x000001FAu) == (uint16_t)(SR_S | CCR_X | CCR_V));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x000008D0u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x000008D0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(ng68k_read16(0x000001FCu) == (uint16_t)(SR_S | CCR_Z));
    CHECK((g_ng_m68k.d[0] & 0xFFu) == 0x54u);
    CHECK(ng68k_read16(0x000001FEu) == (uint16_t)(SR_S | CCR_X | CCR_C));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000900u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000900u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.a[4] == expected_state.a[4]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.d[0] == 0x11111111u);
    CHECK(g_ng_m68k.a[4] == 0x00000218u);
    CHECK(ng68k_read32(0x00000210u) == 0x11111111u);
    CHECK(ng68k_read32(0x00000214u) == 0x22222222u);
    CHECK(ng68k_read16(0x00000208u) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000940u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000940u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[0] == expected_state.a[0]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.a[0] == 0x0000FFFFu);
    CHECK(ng68k_read16(0x00000220u) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000970u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000970u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[0] == expected_state.a[0]);
    CHECK(g_ng_m68k.a[1] == expected_state.a[1]);
    CHECK(g_ng_m68k.a[2] == expected_state.a[2]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.a[0] == 0xFFFFFFFFu);
    CHECK(g_ng_m68k.a[1] == 0xFFFF8000u);
    CHECK(g_ng_m68k.a[2] == 0x00007FFFu);
    CHECK(ng68k_read16(0x00000222u) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x000009A0u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x000009A0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[0] == expected_state.a[0]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.a[0] == 0x00017FFFu);
    CHECK(ng68k_read16(0x00000224u) == (uint16_t)(SR_S | CCR_X));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    bus_write8(expected_bus, 0x00000230u, 0x80u);
    bus_write8(expected_bus, 0x00000240u, 0x7Fu);
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x000009D0u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write8(0x00000230u, 0x80u);
    ng68k_write8(0x00000240u, 0x7Fu);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x000009D0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[0] == expected_state.a[0]);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.a[0] == 0x00000241u);
    CHECK(g_ng_m68k.a[7] == 0x00000232u);
    CHECK(ng68k_read16(0x00000226u) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_V | CCR_C));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    bus_write8(expected_bus, 0x00000250u, 0x45u);
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000A00u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    ng68k_write8(0x00000250u, 0x45u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000A00u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.a[7] == 0x00000250u);
    CHECK(ng68k_read8(0x00000250u) == 0x54u);
    CHECK(ng68k_read16(0x00000228u) == (uint16_t)(SR_S | CCR_X | CCR_C));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000A20u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000A20u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.d[0] == 0x00000080u);
    CHECK(ng68k_read16(0x0000022Au) == (uint16_t)(SR_S | CCR_X | CCR_Z));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000A40u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000A40u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.d[2] == expected_state.d[2]);
    CHECK(g_ng_m68k.a[0] == expected_state.a[0]);
    CHECK(g_ng_m68k.a[1] == expected_state.a[1]);
    CHECK(g_ng_m68k.a[3] == expected_state.a[3]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.a[0] == 0x33334444u);
    CHECK(g_ng_m68k.a[1] == 0x11112222u);
    CHECK(g_ng_m68k.d[2] == 0x77778888u);
    CHECK(g_ng_m68k.a[3] == 0x55556666u);
    CHECK(ng68k_read16(0x0000022Cu) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000A80u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000A80u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.a[7] == 0x0000025Cu);
    CHECK(ng68k_read32(0x0000025Cu) == 0x00000A9Cu);
    CHECK(ng68k_read16(0x0000022Eu) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000AC0u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000AC0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.a[2] == expected_state.a[2]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.d[0] == 0x00000002u);
    CHECK(g_ng_m68k.a[2] == 0x00000ADAu);
    CHECK(ng68k_read16(0x00000230u) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000B00u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000B00u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.d[0] == 0x00010000u);
    CHECK(ng68k_read16(0x00000232u) == (uint16_t)(SR_S | CCR_X | CCR_V));

    for (uint32_t chunk = 0; chunk < 4u; ++chunk) {
        uint32_t start = 0x00000B40u + chunk * 0x1B0u;
        uint32_t first_flags = chunk * 4u;

        memset(&expected_state, 0, sizeof(expected_state));
        memset(expected_bus, 0, sizeof(expected_bus));
        expected_state.sr = SR_S;
        expected_state.a[7] = 0x000001F0u;
        expected_state.ssp = expected_state.a[7];
        CHECK(oracle_exec(program, (uint32_t)sizeof(program), start,
                          &expected_state, expected_bus, 0));

        memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
        memset(g_bus, 0, sizeof(g_bus));
        g_ng_m68k.sr = SR_S;
        g_ng_m68k.a[7] = 0x000001F0u;
        g_ng_m68k.ssp = g_ng_m68k.a[7];
        g_dispatch_miss_count = 0;

        ng_generated_call(start);

        CHECK(g_dispatch_miss_count == 0);
        CHECK(g_ng_m68k.sr == expected_state.sr);
        CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
        for (uint32_t flags = first_flags; flags < first_flags + 4u; ++flags) {
            uint16_t sr = (uint16_t)(SR_S | CCR_X | flags);
            CHECK(ng68k_read16(0x00000500u + flags * 2u) == sr);
            for (uint32_t cond = 0; cond < 16u; ++cond) {
                uint8_t expected = oracle_condition_true(sr, (uint8_t)cond) ?
                    0xFFu : 0x00u;
                CHECK(ng68k_read8(0x00000300u + flags * 16u + cond) == expected);
            }
        }
    }

    for (uint32_t chunk = 0; chunk < 8u; ++chunk) {
        uint32_t start = 0x00001220u + chunk * 0x2E0u;
        uint32_t first_flags = chunk * 2u;

        memset(&expected_state, 0, sizeof(expected_state));
        memset(expected_bus, 0, sizeof(expected_bus));
        expected_state.sr = SR_S;
        expected_state.a[7] = 0x000001F0u;
        expected_state.ssp = expected_state.a[7];
        CHECK(oracle_exec(program, (uint32_t)sizeof(program), start,
                          &expected_state, expected_bus, 0));

        memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
        memset(g_bus, 0, sizeof(g_bus));
        g_ng_m68k.sr = SR_S;
        g_ng_m68k.a[7] = 0x000001F0u;
        g_ng_m68k.ssp = g_ng_m68k.a[7];
        g_dispatch_miss_count = 0;

        ng_generated_call(start);

        CHECK(g_dispatch_miss_count == 0);
        CHECK(g_ng_m68k.sr == expected_state.sr);
        CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
        for (uint32_t flags = first_flags; flags < first_flags + 2u; ++flags) {
            uint16_t sr = (uint16_t)(SR_S | CCR_X | flags);
            for (uint32_t cond = 0; cond < 16u; ++cond) {
                uint16_t expected_d0 = oracle_condition_true(sr, (uint8_t)cond) ?
                    0x0000u : 0xFFFFu;
                CHECK(ng68k_read16(0x00000800u + flags * 32u + cond * 2u) == sr);
                CHECK(ng68k_read16(0x00000600u + flags * 32u + cond * 2u) == expected_d0);
            }
        }
    }

    for (uint32_t chunk = 0; chunk < 8u; ++chunk) {
        uint32_t start = 0x00002A00u + chunk * 0x200u;
        uint32_t first_flags = chunk * 2u;

        memset(&expected_state, 0, sizeof(expected_state));
        memset(expected_bus, 0, sizeof(expected_bus));
        expected_state.sr = SR_S;
        expected_state.a[7] = 0x000001F0u;
        expected_state.ssp = expected_state.a[7];
        CHECK(oracle_exec(program, (uint32_t)sizeof(program), start,
                          &expected_state, expected_bus, 0));

        memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
        memset(g_bus, 0, sizeof(g_bus));
        g_ng_m68k.sr = SR_S;
        g_ng_m68k.a[7] = 0x000001F0u;
        g_ng_m68k.ssp = g_ng_m68k.a[7];
        g_dispatch_miss_count = 0;

        ng_generated_call(start);

        CHECK(g_dispatch_miss_count == 0);
        CHECK(g_ng_m68k.sr == expected_state.sr);
        CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
        for (uint32_t flags = first_flags; flags < first_flags + 2u; ++flags) {
            uint16_t sr = (uint16_t)(SR_S | CCR_X | flags);
            CHECK(ng68k_read16(0x00000B00u + flags * 2u) == sr);
            for (uint32_t cond = 2u; cond < 16u; ++cond) {
                uint8_t expected = oracle_condition_true(sr, (uint8_t)cond) ?
                    0x00u : 0xFFu;
                CHECK(ng68k_read8(0x00000A00u + flags * 16u + cond) == expected);
            }
        }
    }

    for (uint32_t flags = 0u; flags < 16u; ++flags) {
        uint32_t start = 0x00003A00u + flags * 0x200u;

        memset(&expected_state, 0, sizeof(expected_state));
        memset(expected_bus, 0, sizeof(expected_bus));
        expected_state.sr = SR_S;
        expected_state.a[7] = 0x000001F0u;
        expected_state.ssp = expected_state.a[7];
        CHECK(oracle_exec(program, (uint32_t)sizeof(program), start,
                          &expected_state, expected_bus, 0));

        memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
        memset(g_bus, 0, sizeof(g_bus));
        g_ng_m68k.sr = SR_S;
        g_ng_m68k.a[7] = 0x000001F0u;
        g_ng_m68k.ssp = g_ng_m68k.a[7];
        g_dispatch_miss_count = 0;

        ng_generated_call(start);

        CHECK(g_dispatch_miss_count == 0);
        CHECK(g_ng_m68k.sr == expected_state.sr);
        CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
        uint16_t sr = (uint16_t)(SR_S | CCR_X | flags);
        for (uint32_t cond = 0u; cond < 16u; ++cond) {
            int condition_true = oracle_condition_true(sr, (uint8_t)cond);
            uint8_t marker = condition_true ? 0xFFu : 0x00u;
            uint16_t d0 = condition_true ? 0x0001u : 0x0000u;
            CHECK(ng68k_read8(0x00000C00u + flags * 16u + cond) == marker);
            CHECK(ng68k_read16(0x00000E00u + flags * 32u + cond * 2u) == d0);
            CHECK(ng68k_read16(0x00001000u + flags * 32u + cond * 2u) == sr);
        }
    }

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00005A00u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00005A00u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(ng68k_read16(0x00001200u) == (uint16_t)(SR_S | CCR_X | CCR_Z));
    CHECK(ng68k_read32(0x00001202u) == 0x00030000u);
    CHECK(ng68k_read16(0x00001206u) == (uint16_t)(SR_S | CCR_X | CCR_N));
    CHECK(ng68k_read32(0x00001208u) == 0x00008000u);
    CHECK(ng68k_read16(0x0000120Cu) == (uint16_t)(SR_S | CCR_X | CCR_N));
    CHECK(ng68k_read32(0x0000120Eu) == 0xFFFFFFFEu);
    CHECK(ng68k_read16(0x00001212u) == (uint16_t)(SR_S | CCR_X | CCR_Z));
    CHECK(ng68k_read32(0x00001214u) == 0x00010000u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00005B00u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00005B00u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(ng68k_read16(0x00001218u) == (uint16_t)(SR_S | CCR_X | CCR_N));
    CHECK(ng68k_read32(0x0000121Au) == 0xFFFE0001u);
    CHECK(ng68k_read16(0x0000121Eu) == (uint16_t)(SR_S | CCR_X | CCR_N));
    CHECK(ng68k_read32(0x00001220u) == 0xFFFFFFFEu);
    CHECK(ng68k_read16(0x00001224u) == (uint16_t)(SR_S | CCR_X | CCR_Z));
    CHECK(ng68k_read32(0x00001226u) == 0x00000000u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00005C00u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00005C00u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(ng68k_read16(0x00001230u) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));
    CHECK(ng68k_read32(0x00001310u) == 0x11112222u);
    CHECK(ng68k_read32(0x00001314u) == 0x33334444u);
    CHECK(ng68k_read16(0x00001232u) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));
    CHECK(ng68k_read32(0x00001234u) == 0x11112222u);
    CHECK(ng68k_read32(0x00001238u) == 0x33334444u);
    CHECK(ng68k_read16(0x0000123Cu) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));
    CHECK(ng68k_read32(0x00001330u) == 0x55556666u);
    CHECK(ng68k_read32(0x00001334u) == 0x77778888u);
    CHECK(ng68k_read16(0x0000123Eu) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));
    CHECK(ng68k_read32(0x00001240u) == 0xFFFF8001u);
    CHECK(ng68k_read32(0x00001244u) == 0x00007FFEu);
    CHECK(ng68k_read16(0x00001248u) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));
    CHECK(ng68k_read32(0x0000124Au) == 0xFFFFFFFEu);
    CHECK(ng68k_read32(0x0000124Eu) == 0x00000002u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00005D00u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00005D00u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(ng68k_read16(0x00001260u) == (uint16_t)(SR_S | CCR_N | CCR_V));
    CHECK(ng68k_read8(0x00001262u) == 0x80u);
    CHECK(ng68k_read16(0x00001264u) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_C));
    CHECK(ng68k_read16(0x00001266u) == 0xC000u);
    CHECK(ng68k_read16(0x00001268u) == (uint16_t)(SR_S | CCR_X | CCR_Z | CCR_C));
    CHECK(ng68k_read32(0x0000126Au) == 0x00000000u);
    CHECK(ng68k_read16(0x0000126Eu) == (uint16_t)(SR_S | CCR_N));
    CHECK(ng68k_read8(0x00001270u) == 0x81u);
    CHECK(ng68k_read16(0x00001272u) == (uint16_t)(SR_S | CCR_X | CCR_N | CCR_C));
    CHECK(ng68k_read16(0x00001274u) == 0x8000u);
    CHECK(ng68k_read16(0x00001276u) == (uint16_t)(SR_S | CCR_X | CCR_C));
    CHECK(ng68k_read16(0x00001360u) == 0x0001u);
    CHECK(ng68k_read16(0x00001278u) == (uint16_t)(SR_S | CCR_N | CCR_V));
    CHECK(ng68k_read16(0x00001362u) == 0x8000u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00005E00u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00005E00u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(ng68k_read32(0x00001418u) == 0x11112222u);
    CHECK(ng68k_read32(0x0000141Cu) == 0x33334444u);
    CHECK(ng68k_read32(0x00001420u) == 0x55556666u);
    CHECK(ng68k_read32(0x00001424u) == 0x77778888u);
    CHECK(g_ng_m68k.d[1] == 0x11112222u);
    CHECK(g_ng_m68k.d[4] == 0x33334444u);
    CHECK(g_ng_m68k.a[2] == 0x55556666u);
    CHECK(g_ng_m68k.a[5] == 0x77778888u);
    CHECK(ng68k_read16(0x0000127Au) ==
          (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00005E80u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00005E80u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.d[0] == expected_state.d[0]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(ng68k_read16(0x0000127Eu) == (uint16_t)(SR_S | CCR_X | CCR_V));
    CHECK(ng68k_read32(0x00001280u) == 0xFFFFFF80u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00005EA0u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00005EA0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.d[7] == expected_state.d[7]);
    CHECK(g_ng_m68k.a[3] == expected_state.a[3]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(g_ng_m68k.pc == 0x00005EBCu);
    CHECK(ng68k_read16(0x00001288u) ==
          (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));
    CHECK(g_ng_m68k.d[7] == 0xA5A55A5Au);
    CHECK(g_ng_m68k.a[3] == 0x00001460u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00001200u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00001200u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.a[1] == expected_state.a[1]);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(ng68k_read16(0x00001284u) ==
          (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));
    CHECK(g_ng_m68k.a[1] == 0xFFFF8001u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00000B18u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00000B18u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(ng68k_read16(0x00000B34u) == 0x8002u);
    CHECK(ng68k_read16(0x00000B36u) == 0x7FFDu);
    CHECK(g_ng_m68k.d[5] == 0xFFFF8002u);
    CHECK(g_ng_m68k.a[6] == 0x00007FFDu);
    CHECK(ng68k_read16(0x0000127Cu) ==
          (uint16_t)(SR_S | CCR_X | CCR_N | CCR_Z | CCR_V | CCR_C));

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = SR_S;
    expected_state.a[7] = 0x000001F0u;
    expected_state.ssp = expected_state.a[7];
    g_oracle_reset_device_count = 0;
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00005EC0u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = SR_S;
    g_ng_m68k.a[7] = 0x000001F0u;
    g_ng_m68k.ssp = g_ng_m68k.a[7];
    g_dispatch_miss_count = 0;
    g_reset_device_count = 0;

    ng_generated_call(0x00005EC0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(g_reset_device_count == g_oracle_reset_device_count);
    CHECK(g_reset_device_count == 1u);

    memset(&expected_state, 0, sizeof(expected_state));
    memset(expected_bus, 0, sizeof(expected_bus));
    expected_state.sr = 0u;            /* RTR is not privileged. */
    expected_state.a[7] = 0x00001300u; /* Active USP in user mode. */
    expected_state.usp = expected_state.a[7];
    expected_state.ssp = 0x000001F0u;
    bus_write16(expected_bus, 0x00001300u, (uint16_t)(CCR_X | CCR_N | CCR_C));
    bus_write32(expected_bus, 0x00001302u, 0x00005EE0u);
    bus_write32(expected_bus, 32u * 4u, 0x00005EF0u);
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0x00005ED0u,
                      &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    g_ng_m68k.sr = 0u;
    g_ng_m68k.a[7] = 0x00001300u;
    g_ng_m68k.usp = g_ng_m68k.a[7];
    g_ng_m68k.ssp = 0x000001F0u;
    ng68k_write16(0x00001300u, (uint16_t)(CCR_X | CCR_N | CCR_C));
    ng68k_write32(0x00001302u, 0x00005EE0u);
    ng68k_write32(32u * 4u, 0x00005EF0u);
    g_dispatch_miss_count = 0;

    ng_generated_call(0x00005ED0u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(g_ng_m68k.usp == expected_state.usp);
    CHECK(g_ng_m68k.a[7] == expected_state.a[7]);
    CHECK(ng68k_read16(0x00001286u) == (uint16_t)(CCR_X | CCR_N | CCR_C));
    CHECK(g_ng_m68k.usp == 0x00001306u);
    CHECK(g_ng_m68k.a[7] == 0x000001EAu);

    return 0;
}
