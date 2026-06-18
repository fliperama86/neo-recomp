#define _CRT_SECURE_NO_WARNINGS

#include "c_emitter.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static int read_file(FILE *f, char *out, size_t out_size) {
    long size;
    rewind(f);
    if (fseek(f, 0, SEEK_END) != 0) {
        return 0;
    }
    size = ftell(f);
    if (size < 0 || (size_t)size >= out_size) {
        return 0;
    }
    rewind(f);
    if (fread(out, 1, (size_t)size, f) != (size_t)size) {
        return 0;
    }
    out[size] = 0;
    return 1;
}

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
    char symbol[32];
    NgFunctionDiscovery discovery;
    FILE *out;
    char text[4096];

    ng_c_symbol_for_addr(0x000007CCu, symbol, (unsigned)sizeof(symbol));
    CHECK(strcmp(symbol, "ng_func_0007CC") == 0);

    ng_function_discovery_init(&discovery);
    discovery.addrs[discovery.count++] = 0x000007CCu;
    discovery.addrs[discovery.count++] = 0x0000080Cu;
    discovery.addrs[discovery.count++] = 0x00024E38u;

    out = tmpfile();
    CHECK(out != NULL);
    CHECK(ng_emit_c_skeleton(out, &discovery));
    CHECK(read_file(out, text, sizeof(text)));
    fclose(out);

    CHECK(strstr(text, "#include \"ngrecomp/neogeo_runtime.h\"") != NULL);
    CHECK(strstr(text, "static void ng_func_0007CC(void);") != NULL);
    CHECK(strstr(text, "case 0x000007CCu: ng_func_0007CC(); return;") != NULL);
    CHECK(strstr(text, "static void ng_func_024E38(void)") != NULL);
    CHECK(strstr(text, "ng_log_dispatch_miss(0x00024E38u);") != NULL);

    {
        NgProgramRom rom = make_rom(0x40u);
        CHECK(rom.data != NULL);
        write16(&rom, 0x00u, 0x7005u); /* MOVEQ #5,D0 */
        write16(&rom, 0x02u, 0xD040u); /* ADD.W D0,D0 */
        write16(&rom, 0x04u, 0x4EB9u); /* JSR $00000020 */
        write32(&rom, 0x06u, 0x00000020u);
        write16(&rom, 0x0Au, 0x4EF9u); /* JMP $00000030 */
        write32(&rom, 0x0Cu, 0x00000030u);
        write16(&rom, 0x20u, 0x4E75u); /* RTS */
        write16(&rom, 0x30u, 0x4E75u); /* RTS */

        ng_function_discovery_init(&discovery);
        discovery.addrs[discovery.count++] = 0x00000000u;
        discovery.addrs[discovery.count++] = 0x00000020u;
        discovery.addrs[discovery.count++] = 0x00000030u;

        out = tmpfile();
        CHECK(out != NULL);
        CHECK(ng_emit_c(out, &rom, &discovery));
        CHECK(read_file(out, text, sizeof(text)));
        fclose(out);

        CHECK(strstr(text, "/* $000000: MOVEQ #5,D0 */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[0] = 0x00000005u;") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[0] = (g_ng_m68k.d[0] & 0xFFFF0000u) | ((g_ng_m68k.d[0] + g_ng_m68k.d[0]) & 0x0000FFFFu);") != NULL);
        CHECK(strstr(text, "ng_generated_call(0x00000020u);") != NULL);
        CHECK(strstr(text, "ng_generated_call(0x00000030u);") != NULL);
        CHECK(strstr(text, "return;") != NULL);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x30u);
        CHECK(rom.data != NULL);
        write16(&rom, 0x00u, 0x41FAu); /* LEA $000008,A0 */
        write16(&rom, 0x02u, 0x0004u);
        write16(&rom, 0x04u, 0x23C8u); /* MOVE.L A0,$001000 */
        write32(&rom, 0x06u, 0x00001000u);
        write16(&rom, 0x0Au, 0x33FCu); /* MOVE.W #$0007,$003C000C */
        write16(&rom, 0x0Cu, 0x0007u);
        write32(&rom, 0x0Eu, 0x003C000Cu);
        write16(&rom, 0x12u, 0x13FCu); /* MOVE.B #$0080,$0010FD80 */
        write16(&rom, 0x14u, 0x0080u);
        write32(&rom, 0x16u, 0x0010FD80u);
        write16(&rom, 0x1Au, 0x4E75u);

        ng_function_discovery_init(&discovery);
        discovery.addrs[discovery.count++] = 0x00000000u;

        out = tmpfile();
        CHECK(out != NULL);
        CHECK(ng_emit_c(out, &rom, &discovery));
        CHECK(read_file(out, text, sizeof(text)));
        fclose(out);

        CHECK(strstr(text, "g_ng_m68k.a[0] = 0x00000008u;") != NULL);
        CHECK(strstr(text, "ng68k_write32(0x00001000u, g_ng_m68k.a[0]);") != NULL);
        CHECK(strstr(text, "ng68k_write16(0x003C000Cu, 0x0007u);") != NULL);
        CHECK(strstr(text, "ng68k_write8(0x0010FD80u, 0x80u);") != NULL);
        CHECK(strstr(text, "ng_set_nz8(0x80u);") != NULL);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x30u);
        CHECK(rom.data != NULL);
        write16(&rom, 0x00u, 0x08B9u); /* BCLR #7,$0010FD80 */
        write16(&rom, 0x02u, 0x0007u);
        write32(&rom, 0x04u, 0x0010FD80u);
        write16(&rom, 0x08u, 0x027Cu); /* ANDI #$F8FF,SR */
        write16(&rom, 0x0Au, 0xF8FFu);
        write16(&rom, 0x0Cu, 0x1039u); /* MOVE.B $0010FDAE,D0 */
        write32(&rom, 0x0Eu, 0x0010FDAEu);
        write16(&rom, 0x12u, 0x207Bu); /* MOVEA.L (4,PC,D0.W),A0 */
        write16(&rom, 0x14u, 0x0004u);
        write16(&rom, 0x16u, 0x4ED0u); /* JMP (A0) */

        ng_function_discovery_init(&discovery);
        discovery.addrs[discovery.count++] = 0x00000000u;

        out = tmpfile();
        CHECK(out != NULL);
        CHECK(ng_emit_c(out, &rom, &discovery));
        CHECK(read_file(out, text, sizeof(text)));
        fclose(out);

        CHECK(strstr(text, "ng68k_write8(0x0010FD80u, (uint8_t)(ng68k_read8(0x0010FD80u) & (uint8_t)~0x80u));") != NULL);
        CHECK(strstr(text, "g_ng_m68k.sr &= 0xF8FFu;") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[0] = (g_ng_m68k.d[0] & 0xFFFFFF00u) | ng68k_read8(0x0010FDAEu);") != NULL);
        CHECK(strstr(text, "g_ng_m68k.a[0] = ng68k_read32(0x00000018u + (uint32_t)(int16_t)(g_ng_m68k.d[0] & 0xFFFFu));") != NULL);
        CHECK(strstr(text, "ng_generated_call(g_ng_m68k.a[0]);") != NULL);

        ng_program_rom_free(&rom);
    }

    return 0;
}
