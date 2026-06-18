#include "function_discovery.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static NgProgramRom make_rom(uint32_t size) {
    NgProgramRom rom;
    rom.size = size;
    rom.data = (uint8_t *)calloc(size ? size : 1u, 1);
    return rom;
}

static void write16(NgProgramRom *rom, uint32_t addr, uint16_t value) {
    rom->data[addr] = (uint8_t)(value >> 8);
    rom->data[addr + 1] = (uint8_t)value;
}

static void write32(NgProgramRom *rom, uint32_t addr, uint32_t value) {
    write16(rom, addr, (uint16_t)(value >> 16));
    write16(rom, addr + 2u, (uint16_t)value);
}

int main(void) {
    NgFunctionDiscovery discovery;

    {
        NgProgramRom rom = make_rom(0xC0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x40u, 0x207Bu); /* MOVEA.L (4,PC,D0.W),A0 */
        write16(&rom, 0x42u, 0x0004u);
        write16(&rom, 0x44u, 0x4ED0u); /* JMP (A0) */
        write32(&rom, 0x46u, 0x00000080u);
        write32(&rom, 0x4Au, 0x00000090u);
        write32(&rom, 0x4Eu, 0x000000A0u);
        write32(&rom, 0x52u, 0x000000B0u);
        write16(&rom, 0x80u, 0x4E75u);
        write16(&rom, 0x90u, 0x4E75u);
        write16(&rom, 0xA0u, 0x4E75u);
        write16(&rom, 0xB0u, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x40u, &discovery));
        CHECK(discovery.count == 5u);
        CHECK(discovery.addrs[0] == 0x40u);
        CHECK(discovery.addrs[1] == 0x80u);
        CHECK(discovery.addrs[2] == 0x90u);
        CHECK(discovery.addrs[3] == 0xA0u);
        CHECK(discovery.addrs[4] == 0xB0u);
        CHECK(!discovery.truncated);
        CHECK(ng_function_discovery_contains(&discovery, 0xA0u));
        CHECK(!ng_function_discovery_contains(&discovery, 0xA2u));

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xA0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x20u, 0x207Bu);
        write16(&rom, 0x22u, 0x0004u);
        write16(&rom, 0x24u, 0x4ED0u);
        write32(&rom, 0x26u, 0x00000080u);
        write32(&rom, 0x2Au, 0x00000080u);
        write32(&rom, 0x2Eu, 0x00000200u);
        write32(&rom, 0x32u, 0x00000090u);
        write16(&rom, 0x80u, 0x4E75u);
        write16(&rom, 0x90u, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x20u, &discovery));
        CHECK(discovery.count == 3u);
        CHECK(discovery.addrs[0] == 0x20u);
        CHECK(discovery.addrs[1] == 0x80u);
        CHECK(discovery.addrs[2] == 0x90u);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0xC0u);
        CHECK(rom.data != NULL);

        write16(&rom, 0x10u, 0x4EB9u); /* JSR $00000080 */
        write32(&rom, 0x12u, 0x00000080u);
        write16(&rom, 0x16u, 0x6100u); /* BSR $00000030 */
        write16(&rom, 0x18u, 0x0018u);
        write16(&rom, 0x1Au, 0x4EFAu); /* JMP $00000050 */
        write16(&rom, 0x1Cu, 0x0034u);
        write16(&rom, 0x30u, 0x4E75u);
        write16(&rom, 0x50u, 0x4E75u);
        write16(&rom, 0x80u, 0x4E75u);

        CHECK(ng_function_discover_from_entry(&rom, 0x10u, &discovery));
        CHECK(discovery.count == 4u);
        CHECK(discovery.addrs[0] == 0x10u);
        CHECK(discovery.addrs[1] == 0x80u);
        CHECK(discovery.addrs[2] == 0x30u);
        CHECK(discovery.addrs[3] == 0x50u);

        ng_program_rom_free(&rom);
    }

    return 0;
}
