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
    char text[32768];

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
        NgProgramRom rom = make_rom(0x60u);
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
        CHECK(strstr(text, "/* $000002: ADD.W D0,D0 */") != NULL);
        CHECK(strstr(text, "uint64_t ng_full = (uint64_t)ng_dst + (uint64_t)ng_src;") != NULL);
        CHECK(strstr(text, "if (ng_full > 0x0000FFFFu) g_ng_m68k.sr |= NG_CCR_C | NG_CCR_X;") != NULL);
        CHECK(strstr(text, "ng_generated_call(0x00000020u);") != NULL);
        CHECK(strstr(text, "ng_generated_call(0x00000030u);") != NULL);
        CHECK(strstr(text, "return;") != NULL);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x90u);
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
        write16(&rom, 0x1Au, 0x4239u); /* CLR.B $0010FD81 */
        write32(&rom, 0x1Cu, 0x0010FD81u);
        write16(&rom, 0x20u, 0x42B9u); /* CLR.L $0010FD84 */
        write32(&rom, 0x22u, 0x0010FD84u);
        write16(&rom, 0x26u, 0x4A39u); /* TST.B $0010FD81 */
        write32(&rom, 0x28u, 0x0010FD81u);
        write16(&rom, 0x2Cu, 0x23CEu); /* MOVE.L A6,$001014 */
        write32(&rom, 0x2Eu, 0x00001014u);
        write16(&rom, 0x32u, 0x103Cu); /* MOVE.B #$007F,D0 */
        write16(&rom, 0x34u, 0x007Fu);
        write16(&rom, 0x36u, 0x13C0u); /* MOVE.B D0,$001018 */
        write32(&rom, 0x38u, 0x00001018u);
        write16(&rom, 0x3Cu, 0x0C00u); /* CMPI.B #$007F,D0 */
        write16(&rom, 0x3Eu, 0x007Fu);
        write16(&rom, 0x40u, 0x022Eu); /* ANDI.B #$0F,($0F7A,A6) */
        write16(&rom, 0x42u, 0x000Fu);
        write16(&rom, 0x44u, 0x0F7Au);
        write16(&rom, 0x46u, 0x4A2Eu); /* TST.B ($0F7A,A6) */
        write16(&rom, 0x48u, 0x0F7Au);
        write16(&rom, 0x4Au, 0x182Eu); /* MOVE.B ($0F7A,A6),D4 */
        write16(&rom, 0x4Cu, 0x0F7Au);
        write16(&rom, 0x4Eu, 0x5904u); /* SUBQ.B #4,D4 */
        write16(&rom, 0x50u, 0xD101u); /* ADDX.B D1,D0 */
        write16(&rom, 0x52u, 0x3A00u); /* MOVE.W D0,D5 */
        write16(&rom, 0x54u, 0x3085u); /* MOVE.W D5,(A0) */
        write16(&rom, 0x56u, 0xD001u); /* ADD.B D1,D0 */
        write16(&rom, 0x58u, 0x9401u); /* SUB.B D1,D2 */
        write16(&rom, 0x5Au, 0xB001u); /* CMP.B D1,D0 */
        write16(&rom, 0x5Cu, 0xB039u); /* CMP.B $0010FE80,D0 */
        write32(&rom, 0x5Eu, 0x0010FE80u);
        write16(&rom, 0x62u, 0x4268u); /* CLR.W ($44,A0) */
        write16(&rom, 0x64u, 0x0044u);
        write16(&rom, 0x66u, 0x4298u); /* CLR.L (A0)+ */
        write16(&rom, 0x68u, 0x207Cu); /* MOVEA.L #$00000120,A0 */
        write32(&rom, 0x6Au, 0x00000120u);
        write16(&rom, 0x6Eu, 0x2248u); /* MOVEA.L A0,A1 */
        write16(&rom, 0x70u, 0x10C1u); /* MOVE.B D1,(A0)+ */
        write16(&rom, 0x72u, 0x20C1u); /* MOVE.L D1,(A0)+ */
        write16(&rom, 0x74u, 0x32C0u); /* MOVE.W D0,(A1)+ */
        write16(&rom, 0x76u, 0x12D8u); /* MOVE.B (A0)+,(A1)+ */
        write16(&rom, 0x78u, 0x21BCu); /* MOVE.L #$12345678,($0C,A0,A2.L) */
        write32(&rom, 0x7Au, 0x12345678u);
        write16(&rom, 0x7Eu, 0xA80Cu);
        write16(&rom, 0x80u, 0x4A81u); /* TST.L D1 */
        write16(&rom, 0x82u, 0x4A58u); /* TST.W (A0)+ */
        write16(&rom, 0x84u, 0x4E75u);

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
        CHECK(strstr(text, "ng68k_write8(0x0010FD81u, 0x00u);") != NULL);
        CHECK(strstr(text, "ng68k_write32(0x0010FD84u, 0x00000000u);") != NULL);
        CHECK(strstr(text, "ng_set_nz8(ng68k_read8(0x0010FD81u));") != NULL);
        CHECK(strstr(text, "ng68k_write32(0x00001014u, g_ng_m68k.a[6]);") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[0] = (g_ng_m68k.d[0] & 0xFFFFFF00u) | 0x7Fu;") != NULL);
        CHECK(strstr(text, "ng68k_write8(0x00001018u, (uint8_t)(g_ng_m68k.d[0] & 0x00FFu));") != NULL);
        CHECK(strstr(text, "ng_set_nz8((uint8_t)((g_ng_m68k.d[0] & 0x00FFu) - 0x7Fu));") != NULL);
        CHECK(strstr(text, "ng68k_write8((uint32_t)(g_ng_m68k.a[6] + (int32_t)3962), (uint8_t)(ng68k_read8((uint32_t)(g_ng_m68k.a[6] + (int32_t)3962)) & 0x0Fu));") != NULL);
        CHECK(strstr(text, "ng_set_nz8(ng68k_read8((uint32_t)(g_ng_m68k.a[6] + (int32_t)3962)));") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[4] = (g_ng_m68k.d[4] & 0xFFFFFF00u) | ng68k_read8((uint32_t)(g_ng_m68k.a[6] + (int32_t)3962));") != NULL);
        CHECK(strstr(text, "uint8_t ng_result = (uint8_t)(ng_old - 4u);") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[0] = (g_ng_m68k.d[0] & 0xFFFFFF00u) | ng_addx8((uint8_t)(g_ng_m68k.d[1] & 0x00FFu), (uint8_t)(g_ng_m68k.d[0] & 0x00FFu));") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[5] = (g_ng_m68k.d[5] & 0xFFFF0000u)") != NULL);
        CHECK(strstr(text, "ng68k_write16(g_ng_m68k.a[0], (uint16_t)((uint16_t)(g_ng_m68k.d[5] & 0xFFFFu)));") != NULL);
        CHECK(strstr(text, "/* $000056: ADD.B D1,D0 */") != NULL);
        CHECK(strstr(text, "uint64_t ng_full = (uint64_t)ng_dst + (uint64_t)ng_src;") != NULL);
        CHECK(strstr(text, "/* $000058: SUB.B D1,D2 */") != NULL);
        CHECK(strstr(text, "if (ng_src > ng_dst) g_ng_m68k.sr |= NG_CCR_C | NG_CCR_X;") != NULL);
        CHECK(strstr(text, "/* $00005A: CMP.B D1,D0 */") != NULL);
        CHECK(strstr(text, "if (ng_src > ng_dst) g_ng_m68k.sr |= NG_CCR_C;") != NULL);
        CHECK(strstr(text, "/* $00005C: CMP.B $10FE80,D0 */") != NULL);
        CHECK(strstr(text, "uint8_t ng_ea_00005C = ng68k_read8(0x0010FE80u);") != NULL);
        CHECK(strstr(text, "/* $000062: CLR.W ($44,A0) */") != NULL);
        CHECK(strstr(text, "ng68k_write16((uint32_t)(g_ng_m68k.a[0] + (int32_t)68), (uint16_t)(0));") != NULL);
        CHECK(strstr(text, "/* $000066: CLR.L (A0)+ */") != NULL);
        CHECK(strstr(text, "ng68k_write32(g_ng_m68k.a[0], (uint32_t)(0));") != NULL);
        CHECK(strstr(text, "g_ng_m68k.a[0] += 4u;") != NULL);
        CHECK(strstr(text, "/* $000068: MOVEA.L #$120,A0 */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.a[0] = (uint32_t)(0x00000120u);") != NULL);
        CHECK(strstr(text, "/* $00006E: MOVEA.L A0,A1 */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.a[1] = (uint32_t)(g_ng_m68k.a[0]);") != NULL);
        CHECK(strstr(text, "/* $000070: MOVE.B D1,(A0)+ */") != NULL);
        CHECK(strstr(text, "ng68k_write8(g_ng_m68k.a[0], (uint8_t)((uint8_t)(g_ng_m68k.d[1] & 0xFFu)));") != NULL);
        CHECK(strstr(text, "g_ng_m68k.a[0] += 1u;") != NULL);
        CHECK(strstr(text, "/* $000072: MOVE.L D1,(A0)+ */") != NULL);
        CHECK(strstr(text, "ng68k_write32(g_ng_m68k.a[0], (uint32_t)(g_ng_m68k.d[1]));") != NULL);
        CHECK(strstr(text, "/* $000074: MOVE.W D0,(A1)+ */") != NULL);
        CHECK(strstr(text, "ng68k_write16(g_ng_m68k.a[1], (uint16_t)((uint16_t)(g_ng_m68k.d[0] & 0xFFFFu)));") != NULL);
        CHECK(strstr(text, "/* $000076: MOVE.B (A0)+,(A1)+ */") != NULL);
        CHECK(strstr(text, "uint8_t ng_ea_000076 = ng68k_read8(g_ng_m68k.a[0]);") != NULL);
        CHECK(strstr(text, "ng68k_write8(g_ng_m68k.a[1], (uint8_t)(ng_ea_000076));") != NULL);
        CHECK(strstr(text, "/* $000078: MOVE.L #$12345678,($C,A0,A2.L) */") != NULL);
        CHECK(strstr(text, "ng68k_write32((uint32_t)(g_ng_m68k.a[0] + (int32_t)g_ng_m68k.a[2] + (int32_t)12), (uint32_t)(0x12345678u));") != NULL);
        CHECK(strstr(text, "/* $000080: TST.L D1 */") != NULL);
        CHECK(strstr(text, "ng_set_nz32((uint32_t)(g_ng_m68k.d[1]));") != NULL);
        CHECK(strstr(text, "/* $000082: TST.W (A0)+ */") != NULL);
        CHECK(strstr(text, "uint16_t ng_ea_000082 = ng68k_read16(g_ng_m68k.a[0]);") != NULL);
        CHECK(strstr(text, "g_ng_m68k.a[0] += 2u;") != NULL);
        CHECK(strstr(text, "ng_set_nz16((uint16_t)(ng_ea_000082));") != NULL);

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
