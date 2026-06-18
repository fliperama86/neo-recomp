#include "c_emitter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

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

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "usage: %s <out.c>\n", argv[0]);
        return 2;
    }

    NgProgramRom rom = make_rom(0x40u);
    if (!rom.data) {
        return 1;
    }

    write16(&rom, 0x00u, 0x7005u); /* MOVEQ #5,D0 */
    write16(&rom, 0x02u, 0xD040u); /* ADD.W D0,D0 */
    write16(&rom, 0x04u, 0x4EB9u); /* JSR $00000020 */
    write32(&rom, 0x06u, 0x00000020u);
    write16(&rom, 0x0Au, 0x4EF9u); /* JMP $00000030 */
    write32(&rom, 0x0Cu, 0x00000030u);

    write16(&rom, 0x20u, 0x33FCu); /* MOVE.W #$1234,$001000 */
    write16(&rom, 0x22u, 0x1234u);
    write32(&rom, 0x24u, 0x00001000u);
    write16(&rom, 0x28u, 0x4E75u); /* RTS */

    write16(&rom, 0x30u, 0x41FAu); /* LEA $000038,A0 */
    write16(&rom, 0x32u, 0x0004u);
    write16(&rom, 0x34u, 0x23C8u); /* MOVE.L A0,$001004 */
    write32(&rom, 0x36u, 0x00001004u);
    write16(&rom, 0x3Au, 0x4E75u); /* RTS */

    NgFunctionDiscovery discovery;
    ng_function_discovery_init(&discovery);
    discovery.addrs[discovery.count++] = 0x00000000u;
    discovery.addrs[discovery.count++] = 0x00000020u;
    discovery.addrs[discovery.count++] = 0x00000030u;

    FILE *out = fopen(argv[1], "w");
    if (!out) {
        ng_program_rom_free(&rom);
        return 1;
    }

    int ok = ng_emit_c(out, &rom, &discovery);
    if (fclose(out) != 0) {
        ok = 0;
    }
    ng_program_rom_free(&rom);
    return ok ? 0 : 1;
}
