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
        NgProgramRom rom = make_rom(0x1C4u);
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
        write16(&rom, 0x84u, 0x0C58u); /* CMPI.W #$005A,(A0)+ */
        write16(&rom, 0x86u, 0x005Au);
        write16(&rom, 0x88u, 0x0CB0u); /* CMPI.L #$12345678,($0C,A0,A2.L) */
        write32(&rom, 0x8Au, 0x12345678u);
        write16(&rom, 0x8Eu, 0xA80Cu);
        write16(&rom, 0x90u, 0x0242u); /* ANDI.W #$0F0F,D2 */
        write16(&rom, 0x92u, 0x0F0Fu);
        write16(&rom, 0x94u, 0x0218u); /* ANDI.B #$0F,(A0)+ */
        write16(&rom, 0x96u, 0x000Fu);
        write16(&rom, 0x98u, 0x5442u); /* ADDQ.W #2,D2 */
        write16(&rom, 0x9Au, 0x5318u); /* SUBQ.B #1,(A0)+ */
        write16(&rom, 0x9Cu, 0x0042u); /* ORI.W #$00F0,D2 */
        write16(&rom, 0x9Eu, 0x00F0u);
        write16(&rom, 0xA0u, 0x0A18u); /* EORI.B #$0F,(A0)+ */
        write16(&rom, 0xA2u, 0x000Fu);
        write16(&rom, 0xA4u, 0x0642u); /* ADDI.W #$0010,D2 */
        write16(&rom, 0xA6u, 0x0010u);
        write16(&rom, 0xA8u, 0x0418u); /* SUBI.B #$01,(A0)+ */
        write16(&rom, 0xAAu, 0x0001u);
        write16(&rom, 0xACu, 0x08D8u); /* BSET #0,(A0)+ */
        write16(&rom, 0xAEu, 0x0000u);
        write16(&rom, 0xB0u, 0x0842u); /* BCHG #1,D2 */
        write16(&rom, 0xB2u, 0x0001u);
        write16(&rom, 0xB4u, 0x01C2u); /* BSET D0,D2 */
        write16(&rom, 0xB6u, 0x03D8u); /* BSET D1,(A0)+ */
        write16(&rom, 0xB8u, 0x4442u); /* NEG.W D2 */
        write16(&rom, 0xBAu, 0x4618u); /* NOT.B (A0)+ */
        write16(&rom, 0xBCu, 0x163Cu); /* MOVE.B #$80,D3 */
        write16(&rom, 0xBEu, 0x0080u);
        write16(&rom, 0xC0u, 0x4883u); /* EXT.W D3 */
        write16(&rom, 0xC2u, 0x48C3u); /* EXT.L D3 */
        write16(&rom, 0xC4u, 0x4842u); /* SWAP D2 */
        write16(&rom, 0xC6u, 0x247Cu); /* MOVEA.L #$00000100,A2 */
        write32(&rom, 0xC8u, 0x00000100u);
        write16(&rom, 0xCCu, 0xD4FCu); /* ADDA.W #$0010,A2 */
        write16(&rom, 0xCEu, 0x0010u);
        write16(&rom, 0xD0u, 0x95FCu); /* SUBA.L #$00000008,A2 */
        write32(&rom, 0xD2u, 0x00000008u);
        write16(&rom, 0xD6u, 0xB5FCu); /* CMPA.L #$00000108,A2 */
        write32(&rom, 0xD8u, 0x00000108u);
        write16(&rom, 0xDCu, 0x1E3Cu); /* MOVE.B #$0F,D7 */
        write16(&rom, 0xDEu, 0x000Fu);
        write16(&rom, 0xE0u, 0x8607u); /* OR.B D7,D3 */
        write16(&rom, 0xE2u, 0xC607u); /* AND.B D7,D3 */
        write16(&rom, 0xE4u, 0xBF03u); /* EOR.B D7,D3 */
        write16(&rom, 0xE6u, 0x8F18u); /* OR.B D7,(A0)+ */
        write16(&rom, 0xE8u, 0xCF18u); /* AND.B D7,(A0)+ */
        write16(&rom, 0xEAu, 0xBF18u); /* EOR.B D7,(A0)+ */
        write16(&rom, 0xECu, 0xDF18u); /* ADD.B D7,(A0)+ */
        write16(&rom, 0xEEu, 0x9F18u); /* SUB.B D7,(A0)+ */
        write16(&rom, 0xF0u, 0x2E7Cu); /* MOVEA.L #$00000140,A7 */
        write32(&rom, 0xF2u, 0x00000140u);
        write16(&rom, 0xF6u, 0x4868u); /* PEA ($10,A0) */
        write16(&rom, 0xF8u, 0x0010u);
        write16(&rom, 0xFAu, 0x2A7Cu); /* MOVEA.L #$00000160,A5 */
        write32(&rom, 0xFCu, 0x00000160u);
        write16(&rom, 0x100u, 0x4E55u); /* LINK A5,#-4 */
        write16(&rom, 0x102u, 0xFFFCu);
        write16(&rom, 0x104u, 0x4E5Du); /* UNLK A5 */
        write16(&rom, 0x106u, 0x6B08u); /* BMI $000110 */
        write16(&rom, 0x108u, 0x13FCu); /* skipped */
        write16(&rom, 0x10Au, 0x0077u);
        write32(&rom, 0x10Cu, 0x0000101Eu);
        write16(&rom, 0x110u, 0x5BD8u); /* SMI (A0)+ */
        write16(&rom, 0x112u, 0x7E00u); /* MOVEQ #0,D7 */
        write16(&rom, 0x114u, 0x51CFu); /* DBF D7,$000110 */
        write16(&rom, 0x116u, 0xFFFAu);
        write16(&rom, 0x118u, 0x3E3Cu); /* MOVE.W #3,D7 */
        write16(&rom, 0x11Au, 0x0003u);
        write16(&rom, 0x11Cu, 0xCEFCu); /* MULU.W #4,D7 */
        write16(&rom, 0x11Eu, 0x0004u);
        write16(&rom, 0x120u, 0xCFFCu); /* MULS.W #-2,D7 */
        write16(&rom, 0x122u, 0xFFFEu);
        write16(&rom, 0x124u, 0x7012u); /* MOVEQ #$12,D0 */
        write16(&rom, 0x126u, 0x7234u); /* MOVEQ #$34,D1 */
        write16(&rom, 0x128u, 0xC141u); /* EXG D0,D1 */
        write16(&rom, 0x12Au, 0x47E8u); /* LEA ($10,A0),A3 */
        write16(&rom, 0x12Cu, 0x0010u);
        write16(&rom, 0x12Eu, 0x2E3Cu); /* MOVE.L #$14,D7 */
        write32(&rom, 0x130u, 0x00000014u);
        write16(&rom, 0x134u, 0x8EFCu); /* DIVU.W #4,D7 */
        write16(&rom, 0x136u, 0x0004u);
        write16(&rom, 0x138u, 0x7EF6u); /* MOVEQ #-10,D7 */
        write16(&rom, 0x13Au, 0x8FFCu); /* DIVS.W #-2,D7 */
        write16(&rom, 0x13Cu, 0xFFFEu);
        write16(&rom, 0x13Eu, 0xE34Fu); /* LSL.W #1,D7 */
        write16(&rom, 0x140u, 0xE24Fu); /* LSR.W #1,D7 */
        write16(&rom, 0x142u, 0x287Cu); /* MOVEA.L #$00000180,A4 */
        write32(&rom, 0x144u, 0x00000180u);
        write16(&rom, 0x148u, 0x48D4u); /* MOVEM.L D0-D1,(A4) */
        write16(&rom, 0x14Au, 0x0003u);
        write16(&rom, 0x14Cu, 0x4CD4u); /* MOVEM.L (A4),D0-D1 */
        write16(&rom, 0x14Eu, 0x0003u);
        write16(&rom, 0x150u, 0x44FCu); /* MOVE #$001B,CCR */
        write16(&rom, 0x152u, 0x001Bu);
        write16(&rom, 0x154u, 0x003Cu); /* ORI #$04,CCR */
        write16(&rom, 0x156u, 0x0004u);
        write16(&rom, 0x158u, 0x0A3Cu); /* EORI #$04,CCR */
        write16(&rom, 0x15Au, 0x0004u);
        write16(&rom, 0x15Cu, 0x023Cu); /* ANDI #$1F,CCR */
        write16(&rom, 0x15Eu, 0x001Fu);
        write16(&rom, 0x160u, 0x007Cu); /* ORI #$0100,SR */
        write16(&rom, 0x162u, 0x0100u);
        write16(&rom, 0x164u, 0x0A7Cu); /* EORI #$0100,SR */
        write16(&rom, 0x166u, 0x0100u);
        write16(&rom, 0x168u, 0x027Cu); /* ANDI #$FFFF,SR */
        write16(&rom, 0x16Au, 0xFFFFu);
        write16(&rom, 0x16Cu, 0x40F9u); /* MOVE SR,$00000188 */
        write32(&rom, 0x16Eu, 0x00000188u);
        write16(&rom, 0x172u, 0x33FCu); /* MOVE.W #$0003,$0000018A */
        write16(&rom, 0x174u, 0x0003u);
        write32(&rom, 0x176u, 0x0000018Au);
        write16(&rom, 0x17Au, 0xE3F9u); /* LSL.W $0000018A */
        write32(&rom, 0x17Cu, 0x0000018Au);
        write16(&rom, 0x180u, 0x003Cu); /* ORI #$10,CCR */
        write16(&rom, 0x182u, 0x0010u);
        write16(&rom, 0x184u, 0x13FCu); /* MOVE.B #$02,$0000018C */
        write16(&rom, 0x186u, 0x0002u);
        write32(&rom, 0x188u, 0x0000018Cu);
        write16(&rom, 0x18Cu, 0x4039u); /* NEGX.B $0000018C */
        write32(&rom, 0x18Eu, 0x0000018Cu);
        write16(&rom, 0x192u, 0x13FCu); /* MOVE.B #$01,$0000018D */
        write16(&rom, 0x194u, 0x0001u);
        write32(&rom, 0x196u, 0x0000018Du);
        write16(&rom, 0x19Au, 0x4AF9u); /* TAS $0000018D */
        write32(&rom, 0x19Cu, 0x0000018Du);
        write16(&rom, 0x1A0u, 0x4BF9u); /* LEA $0000018E,A5 */
        write32(&rom, 0x1A2u, 0x0000018Eu);
        write16(&rom, 0x1A6u, 0x4DF9u); /* LEA $00000190,A6 */
        write32(&rom, 0x1A8u, 0x00000190u);
        write16(&rom, 0x1ACu, 0x33FCu); /* MOVE.W #$0003,$0000018E */
        write16(&rom, 0x1AEu, 0x0003u);
        write32(&rom, 0x1B0u, 0x0000018Eu);
        write16(&rom, 0x1B4u, 0x33FCu); /* MOVE.W #$0005,$00000190 */
        write16(&rom, 0x1B6u, 0x0005u);
        write32(&rom, 0x1B8u, 0x00000190u);
        write16(&rom, 0x1BCu, 0xBD4Du); /* CMPM.W (A5)+,(A6)+ */
        write16(&rom, 0x1BEu, 0x4E65u); /* MOVE A5,USP */
        write16(&rom, 0x1C0u, 0x4E6Cu); /* MOVE USP,A4 */
        write16(&rom, 0x1C2u, 0x4E75u);

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
        CHECK(strstr(text, "uint8_t ng_src = (uint8_t)(4u); uint8_t ng_dst = (uint8_t)((uint8_t)(g_ng_m68k.d[4] & 0xFFu)); uint8_t ng_result = (uint8_t)(ng_dst - ng_src);") != NULL);
        CHECK(strstr(text, "uint8_t ng_src = (uint8_t)(g_ng_m68k.d[1] & 0x000000FFu); uint8_t ng_dst = (uint8_t)(g_ng_m68k.d[0] & 0x000000FFu);") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[0] = (g_ng_m68k.d[0] & 0xFFFFFF00u) | (uint32_t)(ng_result & 0x000000FFu);") != NULL);
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
        CHECK(strstr(text, "/* $000084: CMPI.W #$5A,(A0)+ */") != NULL);
        CHECK(strstr(text, "uint16_t ng_ea_000084 = ng68k_read16(g_ng_m68k.a[0]);") != NULL);
        CHECK(strstr(text, "{ uint16_t ng_src = (uint16_t)(0x005Au); uint16_t ng_dst = (uint16_t)(ng_ea_000084); uint16_t ng_result = (uint16_t)(ng_dst - ng_src);") != NULL);
        CHECK(strstr(text, "if (ng_src > ng_dst) g_ng_m68k.sr |= NG_CCR_C;") != NULL);
        CHECK(strstr(text, "/* $000088: CMPI.L #$12345678,($C,A0,A2.L) */") != NULL);
        CHECK(strstr(text, "uint32_t ng_ea_000088 = ng68k_read32((uint32_t)(g_ng_m68k.a[0] + (int32_t)g_ng_m68k.a[2] + (int32_t)12));") != NULL);
        CHECK(strstr(text, "{ uint32_t ng_src = (uint32_t)(0x12345678u); uint32_t ng_dst = (uint32_t)(ng_ea_000088); uint32_t ng_result = (uint32_t)(ng_dst - ng_src);") != NULL);
        CHECK(strstr(text, "/* $000090: ANDI.W #$F0F,D2 */") != NULL);
        CHECK(strstr(text, "{ uint16_t ng_result = (uint16_t)(((uint16_t)(g_ng_m68k.d[2] & 0xFFFFu)) & 0x0F0Fu);") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[2] = (g_ng_m68k.d[2] & 0xFFFF0000u) | (uint32_t)((uint16_t)(ng_result));") != NULL);
        CHECK(strstr(text, "/* $000094: ANDI.B #$F,(A0)+ */") != NULL);
        CHECK(strstr(text, "uint32_t ng_addr_000094 = g_ng_m68k.a[0];") != NULL);
        CHECK(strstr(text, "uint8_t ng_value = ng68k_read8(ng_addr_000094); uint8_t ng_result = (uint8_t)(ng_value & 0x0Fu);") != NULL);
        CHECK(strstr(text, "ng68k_write8(ng_addr_000094, ng_result);") != NULL);
        CHECK(strstr(text, "/* $000098: ADDQ.W #2,D2 */") != NULL);
        CHECK(strstr(text, "{ uint16_t ng_src = (uint16_t)(2u); uint16_t ng_dst = (uint16_t)((uint16_t)(g_ng_m68k.d[2] & 0xFFFFu)); uint64_t ng_full = (uint64_t)ng_dst + (uint64_t)ng_src; uint16_t ng_result = (uint16_t)ng_full;") != NULL);
        CHECK(strstr(text, "if (ng_full > 0x0000FFFFu) g_ng_m68k.sr |= NG_CCR_C | NG_CCR_X;") != NULL);
        CHECK(strstr(text, "/* $00009A: SUBQ.B #1,(A0)+ */") != NULL);
        CHECK(strstr(text, "uint32_t ng_addr_00009A = g_ng_m68k.a[0];") != NULL);
        CHECK(strstr(text, "{ uint8_t ng_src = (uint8_t)(1u); uint8_t ng_dst = ng68k_read8(ng_addr_00009A); uint8_t ng_result = (uint8_t)(ng_dst - ng_src);") != NULL);
        CHECK(strstr(text, "ng68k_write8(ng_addr_00009A, ng_result);") != NULL);
        CHECK(strstr(text, "/* $00009C: ORI.W #$F0,D2 */") != NULL);
        CHECK(strstr(text, "{ uint16_t ng_result = (uint16_t)(((uint16_t)(g_ng_m68k.d[2] & 0xFFFFu)) | 0x00F0u);") != NULL);
        CHECK(strstr(text, "/* $0000A0: EORI.B #$F,(A0)+ */") != NULL);
        CHECK(strstr(text, "uint32_t ng_addr_0000A0 = g_ng_m68k.a[0];") != NULL);
        CHECK(strstr(text, "uint8_t ng_value = ng68k_read8(ng_addr_0000A0); uint8_t ng_result = (uint8_t)(ng_value ^ 0x0Fu);") != NULL);
        CHECK(strstr(text, "ng68k_write8(ng_addr_0000A0, ng_result);") != NULL);
        CHECK(strstr(text, "/* $0000A4: ADDI.W #$10,D2 */") != NULL);
        CHECK(strstr(text, "{ uint16_t ng_src = (uint16_t)(0x0010u); uint16_t ng_dst = (uint16_t)((uint16_t)(g_ng_m68k.d[2] & 0xFFFFu)); uint64_t ng_full = (uint64_t)ng_dst + (uint64_t)ng_src; uint16_t ng_result = (uint16_t)ng_full;") != NULL);
        CHECK(strstr(text, "/* $0000A8: SUBI.B #$1,(A0)+ */") != NULL);
        CHECK(strstr(text, "uint32_t ng_addr_0000A8 = g_ng_m68k.a[0];") != NULL);
        CHECK(strstr(text, "{ uint8_t ng_src = (uint8_t)(0x01u); uint8_t ng_dst = ng68k_read8(ng_addr_0000A8); uint8_t ng_result = (uint8_t)(ng_dst - ng_src);") != NULL);
        CHECK(strstr(text, "ng68k_write8(ng_addr_0000A8, ng_result);") != NULL);
        CHECK(strstr(text, "/* $0000AC: BSET #0,(A0)+ */") != NULL);
        CHECK(strstr(text, "uint32_t ng_addr_0000AC = g_ng_m68k.a[0];") != NULL);
        CHECK(strstr(text, "{ uint8_t ng_mask = (uint8_t)(1u << (0u)); uint8_t ng_value = ng68k_read8(ng_addr_0000AC);") != NULL);
        CHECK(strstr(text, "ng68k_write8(ng_addr_0000AC, (uint8_t)(ng_value | ng_mask));") != NULL);
        CHECK(strstr(text, "/* $0000B0: BCHG #1,D2 */") != NULL);
        CHECK(strstr(text, "{ uint32_t ng_mask = (uint32_t)(1u << (1u)); uint32_t ng_value = g_ng_m68k.d[2];") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[2] = ng_value ^ ng_mask;") != NULL);
        CHECK(strstr(text, "/* $0000B4: BSET D0,D2 */") != NULL);
        CHECK(strstr(text, "{ uint32_t ng_mask = (uint32_t)(1u << (g_ng_m68k.d[0] & 31u)); uint32_t ng_value = g_ng_m68k.d[2];") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[2] = ng_value | ng_mask;") != NULL);
        CHECK(strstr(text, "/* $0000B6: BSET D1,(A0)+ */") != NULL);
        CHECK(strstr(text, "uint32_t ng_addr_0000B6 = g_ng_m68k.a[0];") != NULL);
        CHECK(strstr(text, "{ uint8_t ng_mask = (uint8_t)(1u << (g_ng_m68k.d[1] & 7u)); uint8_t ng_value = ng68k_read8(ng_addr_0000B6);") != NULL);
        CHECK(strstr(text, "/* $0000B8: NEG.W D2 */") != NULL);
        CHECK(strstr(text, "{ uint16_t ng_value = (uint16_t)((uint16_t)(g_ng_m68k.d[2] & 0xFFFFu)); uint16_t ng_result = (uint16_t)(0u - ng_value);") != NULL);
        CHECK(strstr(text, "if (ng_value != 0) g_ng_m68k.sr |= NG_CCR_C | NG_CCR_X;") != NULL);
        CHECK(strstr(text, "/* $0000BA: NOT.B (A0)+ */") != NULL);
        CHECK(strstr(text, "uint32_t ng_addr_0000BA = g_ng_m68k.a[0];") != NULL);
        CHECK(strstr(text, "{ uint8_t ng_value = ng68k_read8(ng_addr_0000BA); uint8_t ng_result = (uint8_t)~ng_value;") != NULL);
        CHECK(strstr(text, "/* $0000C0: EXT.W D3 */") != NULL);
        CHECK(strstr(text, "{ uint16_t ng_result = (uint16_t)(int16_t)(int8_t)(g_ng_m68k.d[3] & 0xFFu);") != NULL);
        CHECK(strstr(text, "/* $0000C2: EXT.L D3 */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[3] = (uint32_t)(int32_t)(int16_t)(g_ng_m68k.d[3] & 0xFFFFu);") != NULL);
        CHECK(strstr(text, "/* $0000C4: SWAP D2 */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[2] = (g_ng_m68k.d[2] << 16) | (g_ng_m68k.d[2] >> 16);") != NULL);
        CHECK(strstr(text, "/* $0000CC: ADDA.W #$10,A2 */") != NULL);
        CHECK(strstr(text, "{ uint32_t ng_src = (uint32_t)(int32_t)(int16_t)(0x0010u); g_ng_m68k.a[2] += ng_src; }") != NULL);
        CHECK(strstr(text, "/* $0000D0: SUBA.L #$8,A2 */") != NULL);
        CHECK(strstr(text, "{ uint32_t ng_src = (uint32_t)(0x00000008u); g_ng_m68k.a[2] -= ng_src; }") != NULL);
        CHECK(strstr(text, "/* $0000D6: CMPA.L #$108,A2 */") != NULL);
        CHECK(strstr(text, "{ uint32_t ng_src = (uint32_t)(0x00000108u); uint32_t ng_dst = g_ng_m68k.a[2]; uint32_t ng_result = ng_dst - ng_src;") != NULL);
        CHECK(strstr(text, "/* $0000E0: OR.B D7,D3 */") != NULL);
        CHECK(strstr(text, "ng_result = (uint8_t)(ng_dst | ng_src);") != NULL);
        CHECK(strstr(text, "/* $0000E2: AND.B D7,D3 */") != NULL);
        CHECK(strstr(text, "ng_result = (uint8_t)(ng_dst & ng_src);") != NULL);
        CHECK(strstr(text, "/* $0000E4: EOR.B D7,D3 */") != NULL);
        CHECK(strstr(text, "ng_result = (uint8_t)(ng_dst ^ ng_src);") != NULL);
        CHECK(strstr(text, "/* $0000E6: OR.B D7,(A0)+ */") != NULL);
        CHECK(strstr(text, "uint32_t ng_addr_0000E6 = g_ng_m68k.a[0];") != NULL);
        CHECK(strstr(text, "ng68k_write8(ng_addr_0000E6, ng_result);") != NULL);
        CHECK(strstr(text, "/* $0000EC: ADD.B D7,(A0)+ */") != NULL);
        CHECK(strstr(text, "uint32_t ng_addr_0000EC = g_ng_m68k.a[0];") != NULL);
        CHECK(strstr(text, "uint64_t ng_full = (uint64_t)ng_dst + (uint64_t)ng_src;") != NULL);
        CHECK(strstr(text, "/* $0000EE: SUB.B D7,(A0)+ */") != NULL);
        CHECK(strstr(text, "uint32_t ng_addr_0000EE = g_ng_m68k.a[0];") != NULL);
        CHECK(strstr(text, "/* $0000F6: PEA ($10,A0) */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.a[7] -= 4u;") != NULL);
        CHECK(strstr(text, "ng68k_write32(g_ng_m68k.a[7], (uint32_t)((uint32_t)(g_ng_m68k.a[0] + (int32_t)16)));") != NULL);
        CHECK(strstr(text, "/* $000100: LINK A5,#-4 */") != NULL);
        CHECK(strstr(text, "ng68k_write32(g_ng_m68k.a[7], g_ng_m68k.a[5]);") != NULL);
        CHECK(strstr(text, "g_ng_m68k.a[7] = (uint32_t)((int32_t)g_ng_m68k.a[7] + (int32_t)-4);") != NULL);
        CHECK(strstr(text, "/* $000104: UNLK A5 */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.a[5] = ng68k_read32(g_ng_m68k.a[7]);") != NULL);
        CHECK(strstr(text, "/* $000106: Bcc.B $000110 */") != NULL);
        CHECK(strstr(text, "if (((g_ng_m68k.sr & NG_CCR_N) != 0)) goto ng_label_000110;") != NULL);
        CHECK(strstr(text, "/* $000110: Scc.B (A0)+ */") != NULL);
        CHECK(strstr(text, "ng68k_write8(g_ng_m68k.a[0], (uint8_t)((((g_ng_m68k.sr & NG_CCR_N) != 0) ? 0xFFu : 0x00u)));") != NULL);
        CHECK(strstr(text, "/* $000114: DBcc.1 D7,$000110 */") != NULL);
        CHECK(strstr(text, "uint16_t ng_counter = (uint16_t)((g_ng_m68k.d[7] & 0xFFFFu) - 1u);") != NULL);
        CHECK(strstr(text, "/* $00011C: MULU.W #$4,D7 */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[7] = (uint32_t)((uint32_t)(uint16_t)(g_ng_m68k.d[7] & 0xFFFFu) * (uint32_t)(uint16_t)(0x0004u));") != NULL);
        CHECK(strstr(text, "/* $000120: MULS.W #$FFFE,D7 */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[7] = (uint32_t)((int32_t)(int16_t)(g_ng_m68k.d[7] & 0xFFFFu) * (int32_t)(int16_t)(0xFFFEu));") != NULL);
        CHECK(strstr(text, "/* $000128: EXG D0,D1 */") != NULL);
        CHECK(strstr(text, "{ uint32_t ng_tmp = g_ng_m68k.d[0]; g_ng_m68k.d[0] = g_ng_m68k.d[1]; g_ng_m68k.d[1] = ng_tmp; }") != NULL);
        CHECK(strstr(text, "/* $00012A: LEA ($10,A0),A3 */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.a[3] = (uint32_t)((uint32_t)(g_ng_m68k.a[0] + (int32_t)16));") != NULL);
        CHECK(strstr(text, "/* $000134: DIVU.W #$4,D7 */") != NULL);
        CHECK(strstr(text, "uint32_t ng_dividend = g_ng_m68k.d[7]; uint32_t ng_quotient = ng_dividend / ng_divisor;") != NULL);
        CHECK(strstr(text, "/* $00013A: DIVS.W #$FFFE,D7 */") != NULL);
        CHECK(strstr(text, "int32_t ng_dividend = (int32_t)g_ng_m68k.d[7]; int32_t ng_quotient = ng_dividend / ng_divisor;") != NULL);
        CHECK(strstr(text, "/* $00013E: LSL.W #1,D7 */") != NULL);
        CHECK(strstr(text, "uint8_t ng_count = 1u; uint32_t ng_result = g_ng_m68k.d[7] & 0x0000FFFFu;") != NULL);
        CHECK(strstr(text, "/* $000148: MOVEM.L #$0003,(A4) */") != NULL);
        CHECK(strstr(text, "ng68k_write32(ng_addr, (uint32_t)(g_ng_m68k.d[0])); ng_addr += 4u;") != NULL);
        CHECK(strstr(text, "/* $00014C: MOVEM.L (A4),#$0003 */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[0] = ng68k_read32(ng_addr); ng_addr += 4u;") != NULL);
        CHECK(strstr(text, "/* $000150: MOVE #$1B,CCR */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.sr = (uint16_t)((g_ng_m68k.sr & 0xFFE0u) | ((0x001Bu) & 0x001Fu));") != NULL);
        CHECK(strstr(text, "/* $000154: ORI #$04,CCR */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.sr = (uint16_t)((g_ng_m68k.sr & 0xFFE0u) | ((g_ng_m68k.sr | 0x04u) & 0x001Fu));") != NULL);
        CHECK(strstr(text, "/* $000158: EORI #$04,CCR */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.sr = (uint16_t)((g_ng_m68k.sr & 0xFFE0u) | ((g_ng_m68k.sr ^ 0x04u) & 0x001Fu));") != NULL);
        CHECK(strstr(text, "/* $00015C: ANDI #$1F,CCR */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.sr = (uint16_t)((g_ng_m68k.sr & 0xFFE0u) | ((g_ng_m68k.sr & 0x1Fu) & 0x001Fu));") != NULL);
        CHECK(strstr(text, "/* $000160: ORI #$0100,SR */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.sr = (uint16_t)(g_ng_m68k.sr | 0x0100u);") != NULL);
        CHECK(strstr(text, "/* $000164: EORI #$0100,SR */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.sr = (uint16_t)(g_ng_m68k.sr ^ 0x0100u);") != NULL);
        CHECK(strstr(text, "/* $000168: ANDI #$FFFF,SR */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.sr = (uint16_t)(g_ng_m68k.sr & 0xFFFFu);") != NULL);
        CHECK(strstr(text, "/* $00016C: MOVE SR,$000188 */") != NULL);
        CHECK(strstr(text, "ng68k_write16(0x00000188u, (uint16_t)(g_ng_m68k.sr));") != NULL);
        CHECK(strstr(text, "/* $00017A: LSL.W $00018A */") != NULL);
        CHECK(strstr(text, "uint32_t ng_addr = 0x0000018Au; uint32_t ng_result = ng68k_read16(ng_addr);") != NULL);
        CHECK(strstr(text, "ng_result = (ng_result << 1) & 0x0000FFFFu;") != NULL);
        CHECK(strstr(text, "ng68k_write16(ng_addr, (uint16_t)(ng_result & 0x0000FFFFu));") != NULL);
        CHECK(strstr(text, "/* $00018C: NEGX.B $00018C */") != NULL);
        CHECK(strstr(text, "uint8_t ng_value = ng68k_read8(0x0000018Cu); uint8_t ng_x = (g_ng_m68k.sr & NG_CCR_X) ? 1u : 0u;") != NULL);
        CHECK(strstr(text, "ng68k_write8(0x0000018Cu, ng_result);") != NULL);
        CHECK(strstr(text, "/* $00019A: TAS $00018D */") != NULL);
        CHECK(strstr(text, "uint32_t ng_addr = 0x0000018Du; uint8_t ng_value = ng68k_read8(ng_addr);") != NULL);
        CHECK(strstr(text, "ng68k_write8(ng_addr, (uint8_t)(ng_value | 0x80u));") != NULL);
        CHECK(strstr(text, "/* $0001BC: CMPM.W (A5)+,(A6)+ */") != NULL);
        CHECK(strstr(text, "uint32_t ng_src_addr = g_ng_m68k.a[5]; g_ng_m68k.a[5] += 2u;") != NULL);
        CHECK(strstr(text, "uint32_t ng_dst_addr = g_ng_m68k.a[6]; g_ng_m68k.a[6] += 2u;") != NULL);
        CHECK(strstr(text, "uint16_t ng_src = ng68k_read16(ng_src_addr); uint16_t ng_dst = ng68k_read16(ng_dst_addr);") != NULL);
        CHECK(strstr(text, "/* $0001BE: MOVE A5,USP */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.usp = g_ng_m68k.a[5];") != NULL);
        CHECK(strstr(text, "/* $0001C0: MOVE USP,A4 */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.a[4] = g_ng_m68k.usp;") != NULL);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x08u);
        CHECK(rom.data != NULL);
        write16(&rom, 0x00u, 0x4E90u); /* JSR (A0) */
        write16(&rom, 0x02u, 0x4EE8u); /* JMP ($4,A0) */
        write16(&rom, 0x04u, 0x0004u);

        ng_function_discovery_init(&discovery);
        discovery.addrs[discovery.count++] = 0x00000000u;

        out = tmpfile();
        CHECK(out != NULL);
        CHECK(ng_emit_c(out, &rom, &discovery));
        CHECK(read_file(out, text, sizeof(text)));
        fclose(out);

        CHECK(strstr(text, "/* $000000: JSR (A0) */") != NULL);
        CHECK(strstr(text, "ng_generated_call(g_ng_m68k.a[0]);") != NULL);
        CHECK(strstr(text, "/* $000002: JMP ($4,A0) */") != NULL);
        CHECK(strstr(text, "ng_generated_call((uint32_t)(g_ng_m68k.a[0] + (int32_t)4));") != NULL);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x0Au);
        CHECK(rom.data != NULL);
        write16(&rom, 0x00u, 0x0188u); /* MOVEP.W D0,($10,A0) */
        write16(&rom, 0x02u, 0x0010u);
        write16(&rom, 0x04u, 0x0349u); /* MOVEP.L ($FFFC,A1),D1 */
        write16(&rom, 0x06u, 0xFFFCu);
        write16(&rom, 0x08u, 0x4E75u);

        ng_function_discovery_init(&discovery);
        discovery.addrs[discovery.count++] = 0x00000000u;

        out = tmpfile();
        CHECK(out != NULL);
        CHECK(ng_emit_c(out, &rom, &discovery));
        CHECK(read_file(out, text, sizeof(text)));
        fclose(out);

        CHECK(strstr(text, "/* $000000: MOVEP.W D0,($10,A0) */") != NULL);
        CHECK(strstr(text, "uint32_t ng_addr = (uint32_t)(g_ng_m68k.a[0] + (int32_t)16); uint32_t ng_value = g_ng_m68k.d[0];") != NULL);
        CHECK(strstr(text, "ng68k_write8(ng_addr, (uint8_t)(ng_value >> 8)); ng68k_write8(ng_addr + 2u, (uint8_t)ng_value);") != NULL);
        CHECK(strstr(text, "/* $000004: MOVEP.L ($FFFC,A1),D1 */") != NULL);
        CHECK(strstr(text, "uint32_t ng_addr = (uint32_t)(g_ng_m68k.a[1] + (int32_t)-4);") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[1] = ((uint32_t)ng68k_read8(ng_addr) << 24)") != NULL);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x06u);
        CHECK(rom.data != NULL);
        write16(&rom, 0x00u, 0x9340u); /* SUBX.W D0,D1 */
        write16(&rom, 0x02u, 0xDD4Du); /* ADDX.W -(A5),-(A6) */
        write16(&rom, 0x04u, 0x4E75u);

        ng_function_discovery_init(&discovery);
        discovery.addrs[discovery.count++] = 0x00000000u;

        out = tmpfile();
        CHECK(out != NULL);
        CHECK(ng_emit_c(out, &rom, &discovery));
        CHECK(read_file(out, text, sizeof(text)));
        fclose(out);

        CHECK(strstr(text, "/* $000000: SUBX.W D0,D1 */") != NULL);
        CHECK(strstr(text, "uint16_t ng_src = (uint16_t)(g_ng_m68k.d[0] & 0x0000FFFFu); uint16_t ng_dst = (uint16_t)(g_ng_m68k.d[1] & 0x0000FFFFu);") != NULL);
        CHECK(strstr(text, "uint64_t ng_src_full = (uint64_t)ng_src + (uint64_t)ng_x;") != NULL);
        CHECK(strstr(text, "/* $000002: ADDX.W -(A5),-(A6) */") != NULL);
        CHECK(strstr(text, "g_ng_m68k.a[5] -= 2u; uint32_t ng_src_addr = g_ng_m68k.a[5]; g_ng_m68k.a[6] -= 2u; uint32_t ng_dst_addr = g_ng_m68k.a[6];") != NULL);
        CHECK(strstr(text, "ng68k_write16(ng_dst_addr, ng_result);") != NULL);

        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x08u);
        CHECK(rom.data != NULL);
        write16(&rom, 0x00u, 0x4839u); /* NBCD.B $00000194 */
        write32(&rom, 0x02u, 0x00000194u);
        write16(&rom, 0x06u, 0x4E75u);

        ng_function_discovery_init(&discovery);
        discovery.addrs[discovery.count++] = 0x00000000u;

        out = tmpfile();
        CHECK(out != NULL);
        CHECK(ng_emit_c(out, &rom, &discovery));
        CHECK(read_file(out, text, sizeof(text)));
        fclose(out);

        CHECK(strstr(text, "/* $000000: NBCD.B $000194 */") != NULL);
        CHECK(strstr(text, "uint8_t ng_value = (uint8_t)(ng68k_read8(0x00000194u));") != NULL);
        CHECK(strstr(text, "uint8_t ng_result_decimal = (uint8_t)((100u - ng_decimal - ng_x) % 100u);") != NULL);
        CHECK(strstr(text, "ng68k_write8(0x00000194u, ng_result);") != NULL);

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

        CHECK(strstr(text, "/* $000000: BCLR #7,$10FD80 */") != NULL);
        CHECK(strstr(text, "{ uint8_t ng_mask = (uint8_t)(1u << (7u)); uint8_t ng_value = ng68k_read8(0x0010FD80u);") != NULL);
        CHECK(strstr(text, "ng68k_write8(0x0010FD80u, (uint8_t)(ng_value & (uint8_t)~ng_mask));") != NULL);
        CHECK(strstr(text, "g_ng_m68k.sr = (uint16_t)(g_ng_m68k.sr & 0xF8FFu);") != NULL);
        CHECK(strstr(text, "g_ng_m68k.d[0] = (g_ng_m68k.d[0] & 0xFFFFFF00u) | ng68k_read8(0x0010FDAEu);") != NULL);
        CHECK(strstr(text, "g_ng_m68k.a[0] = ng68k_read32(0x00000018u + (uint32_t)(int16_t)(g_ng_m68k.d[0] & 0xFFFFu));") != NULL);
        CHECK(strstr(text, "ng_generated_call(g_ng_m68k.a[0]);") != NULL);

        ng_program_rom_free(&rom);
    }

    return 0;
}
