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
#define CCR_Z 0x0004u
#define CCR_N 0x0008u

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
        if (op == 0xD040u) {
            uint16_t result = (uint16_t)(state->d[0] + state->d[0]);
            state->d[0] = (state->d[0] & 0xFFFF0000u) | result;
            oracle_set_nz16(state, result);
            pc += 2u;
            continue;
        }
        if ((op & 0xF000u) == 0x6000u && (op & 0xFF00u) != 0x6000u &&
            (op & 0xFF00u) != 0x6100u) {
            uint8_t condition = (uint8_t)((op >> 8) & 0xFu);
            int8_t displacement = (int8_t)(op & 0xFFu);
            uint32_t next_pc = pc + 2u;
            int take;
            if (condition == 6u) {
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
        if (op == 0x4239u) {
            uint32_t addr = program_read32(program, size, pc + 2u);
            bus_write8(bus, addr, 0);
            oracle_set_nz8(state, 0);
            pc += 6u;
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
    CHECK(oracle_exec(program, (uint32_t)sizeof(program), 0, &expected_state, expected_bus, 0));

    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    memset(g_bus, 0, sizeof(g_bus));
    ng68k_write8(0x100Cu, 0xAAu);
    ng68k_write32(0x1010u, 0x12345678u);
    g_dispatch_miss_count = 0;
    g_last_dispatch_miss = 0;

    ng_generated_call(0x00000000u);

    CHECK(g_dispatch_miss_count == 0);
    CHECK(memcmp(g_ng_m68k.d, expected_state.d, sizeof(g_ng_m68k.d)) == 0);
    CHECK(memcmp(g_ng_m68k.a, expected_state.a, sizeof(g_ng_m68k.a)) == 0);
    CHECK(g_ng_m68k.sr == expected_state.sr);
    CHECK(memcmp(g_bus, expected_bus, sizeof(g_bus)) == 0);
    CHECK(g_ng_m68k.d[0] == 10u);
    CHECK((g_ng_m68k.d[2] & 0xFFu) == 0x7Fu);
    CHECK(ng68k_read16(0x1000u) == 0x1234u);
    CHECK(ng68k_read32(0x1004u) == 0x00000068u);
    CHECK(ng68k_read16(0x1008u) == 0x2222u);
    CHECK(ng68k_read16(0x100Au) == 0x0000u);
    CHECK(ng68k_read8(0x100Cu) == 0x00u);
    CHECK(ng68k_read8(0x100Eu) == 0x80u);
    CHECK(ng68k_read8(0x100Fu) == 0x00u);
    CHECK(ng68k_read32(0x1010u) == 0x00000000u);
    CHECK(ng68k_read32(0x1014u) == 0x000000A0u);
    CHECK((g_ng_m68k.sr & CCR_N) != 0);

    ng_generated_call(0x00DEADu);
    CHECK(g_dispatch_miss_count == 1);
    CHECK(g_last_dispatch_miss == 0x00DEADu);

    return 0;
}
