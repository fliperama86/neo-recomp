#pragma once

#include <stdint.h>
#include <string.h>

#define NG_EXEC_FIXTURE_SIZE 0x494u
#define NG_EXEC_FIXTURE_ADDR_COUNT 35u

#if defined(__GNUC__) || defined(__clang__)
#define NG_EXEC_FIXTURE_MAYBE_UNUSED __attribute__((unused))
#else
#define NG_EXEC_FIXTURE_MAYBE_UNUSED
#endif

static void ng_exec_fixture_write16(uint8_t *data, uint32_t addr, uint16_t value) {
    data[addr] = (uint8_t)(value >> 8);
    data[addr + 1u] = (uint8_t)value;
}

static void ng_exec_fixture_write32(uint8_t *data, uint32_t addr, uint32_t value) {
    ng_exec_fixture_write16(data, addr, (uint16_t)(value >> 16));
    ng_exec_fixture_write16(data, addr + 2u, (uint16_t)value);
}

static void ng_exec_fixture_fill(uint8_t *data, uint32_t size) {
    memset(data, 0, size);
    if (size < NG_EXEC_FIXTURE_SIZE) {
        return;
    }

    ng_exec_fixture_write16(data, 0x00u, 0x7005u); /* MOVEQ #5,D0 */
    ng_exec_fixture_write16(data, 0x02u, 0xD040u); /* ADD.W D0,D0 */
    ng_exec_fixture_write16(data, 0x04u, 0x4EB9u); /* JSR $00000020 */
    ng_exec_fixture_write32(data, 0x06u, 0x00000020u);
    ng_exec_fixture_write16(data, 0x0Au, 0x4EF9u); /* JMP $00000060 */
    ng_exec_fixture_write32(data, 0x0Cu, 0x00000060u);

    ng_exec_fixture_write16(data, 0x20u, 0x33FCu); /* MOVE.W #$1234,$001000 */
    ng_exec_fixture_write16(data, 0x22u, 0x1234u);
    ng_exec_fixture_write32(data, 0x24u, 0x00001000u);
    ng_exec_fixture_write16(data, 0x28u, 0x7200u); /* MOVEQ #0,D1 */
    ng_exec_fixture_write16(data, 0x2Au, 0x6608u); /* BNE $000034, not taken */
    ng_exec_fixture_write16(data, 0x2Cu, 0x33FCu); /* MOVE.W #$2222,$001008 */
    ng_exec_fixture_write16(data, 0x2Eu, 0x2222u);
    ng_exec_fixture_write32(data, 0x30u, 0x00001008u);
    ng_exec_fixture_write16(data, 0x34u, 0x7201u); /* MOVEQ #1,D1 */
    ng_exec_fixture_write16(data, 0x36u, 0x6608u); /* BNE $000040, taken */
    ng_exec_fixture_write16(data, 0x38u, 0x33FCu); /* skipped */
    ng_exec_fixture_write16(data, 0x3Au, 0x3333u);
    ng_exec_fixture_write32(data, 0x3Cu, 0x0000100Au);
    ng_exec_fixture_write16(data, 0x40u, 0x4E75u); /* RTS */

    ng_exec_fixture_write16(data, 0x60u, 0x41FAu); /* LEA $000068,A0 */
    ng_exec_fixture_write16(data, 0x62u, 0x0006u);
    ng_exec_fixture_write16(data, 0x64u, 0x23C8u); /* MOVE.L A0,$001004 */
    ng_exec_fixture_write32(data, 0x66u, 0x00001004u);
    ng_exec_fixture_write16(data, 0x6Au, 0x4239u); /* CLR.B $00100C */
    ng_exec_fixture_write32(data, 0x6Cu, 0x0000100Cu);
    ng_exec_fixture_write16(data, 0x70u, 0x42B9u); /* CLR.L $001010 */
    ng_exec_fixture_write32(data, 0x72u, 0x00001010u);
    ng_exec_fixture_write16(data, 0x76u, 0x4A39u); /* TST.B $00100C */
    ng_exec_fixture_write32(data, 0x78u, 0x0000100Cu);
    ng_exec_fixture_write16(data, 0x7Cu, 0x6708u); /* BEQ $000086, taken */
    ng_exec_fixture_write16(data, 0x7Eu, 0x13FCu); /* skipped */
    ng_exec_fixture_write16(data, 0x80u, 0x0055u);
    ng_exec_fixture_write32(data, 0x82u, 0x0000100Fu);
    ng_exec_fixture_write16(data, 0x86u, 0x4DFAu); /* LEA $0000A0,A6 */
    ng_exec_fixture_write16(data, 0x88u, 0x0018u);
    ng_exec_fixture_write16(data, 0x8Au, 0x23CEu); /* MOVE.L A6,$001014 */
    ng_exec_fixture_write32(data, 0x8Cu, 0x00001014u);
    ng_exec_fixture_write16(data, 0x90u, 0x143Cu); /* MOVE.B #$7F,D2 */
    ng_exec_fixture_write16(data, 0x92u, 0x007Fu);
    ng_exec_fixture_write16(data, 0x94u, 0x163Cu); /* MOVE.B #$55,D3 */
    ng_exec_fixture_write16(data, 0x96u, 0x0055u);
    ng_exec_fixture_write16(data, 0x98u, 0x4203u); /* CLR.B D3 */
    ng_exec_fixture_write16(data, 0x9Au, 0x13C2u); /* MOVE.B D2,$001018 */
    ng_exec_fixture_write32(data, 0x9Cu, 0x00001018u);
    ng_exec_fixture_write16(data, 0xA0u, 0x0C02u); /* CMPI.B #$7F,D2 */
    ng_exec_fixture_write16(data, 0xA2u, 0x007Fu);
    ng_exec_fixture_write16(data, 0xA4u, 0x6408u); /* BCC $0000AE, taken */
    ng_exec_fixture_write16(data, 0xA6u, 0x13FCu); /* skipped */
    ng_exec_fixture_write16(data, 0xA8u, 0x0066u);
    ng_exec_fixture_write32(data, 0xAAu, 0x00001019u);
    ng_exec_fixture_write16(data, 0xAEu, 0x022Eu); /* ANDI.B #$0F,($0F7A,A6) */
    ng_exec_fixture_write16(data, 0xB0u, 0x000Fu);
    ng_exec_fixture_write16(data, 0xB2u, 0x0F7Au);
    ng_exec_fixture_write16(data, 0xB4u, 0x4A2Eu); /* TST.B ($0F7A,A6) */
    ng_exec_fixture_write16(data, 0xB6u, 0x0F7Au);
    ng_exec_fixture_write16(data, 0xB8u, 0x6608u); /* BNE $0000C2, taken */
    ng_exec_fixture_write16(data, 0xBAu, 0x13FCu); /* skipped */
    ng_exec_fixture_write16(data, 0xBCu, 0x0066u);
    ng_exec_fixture_write32(data, 0xBEu, 0x0000101Bu);
    ng_exec_fixture_write16(data, 0xC2u, 0x182Eu); /* MOVE.B ($0F7A,A6),D4 */
    ng_exec_fixture_write16(data, 0xC4u, 0x0F7Au);
    ng_exec_fixture_write16(data, 0xC6u, 0x5304u); /* SUBQ.B #1,D4 */
    ng_exec_fixture_write16(data, 0xC8u, 0x7000u); /* MOVEQ #0,D0 */
    ng_exec_fixture_write16(data, 0xCAu, 0x7200u); /* MOVEQ #0,D1 */
    ng_exec_fixture_write16(data, 0xCCu, 0x5300u); /* SUBQ.B #1,D0, sets X */
    ng_exec_fixture_write16(data, 0xCEu, 0xD101u); /* ADDX.B D1,D0 */
    ng_exec_fixture_write16(data, 0xD0u, 0x13C0u); /* MOVE.B D0,$00101C */
    ng_exec_fixture_write32(data, 0xD2u, 0x0000101Cu);
    ng_exec_fixture_write16(data, 0xD6u, 0x3C3Cu); /* MOVE.W #$1357,D6 */
    ng_exec_fixture_write16(data, 0xD8u, 0x1357u);
    ng_exec_fixture_write16(data, 0xDAu, 0x3A06u); /* MOVE.W D6,D5 */
    ng_exec_fixture_write16(data, 0xDCu, 0x3085u); /* MOVE.W D5,(A0) */
    ng_exec_fixture_write16(data, 0xDEu, 0x4268u); /* CLR.W ($44,A0) */
    ng_exec_fixture_write16(data, 0xE0u, 0x0044u);
    ng_exec_fixture_write16(data, 0xE2u, 0x4298u); /* CLR.L (A0)+ */
    ng_exec_fixture_write16(data, 0xE4u, 0x103Cu); /* MOVE.B #5,D0 */
    ng_exec_fixture_write16(data, 0xE6u, 0x0005u);
    ng_exec_fixture_write16(data, 0xE8u, 0x123Cu); /* MOVE.B #3,D1 */
    ng_exec_fixture_write16(data, 0xEAu, 0x0003u);
    ng_exec_fixture_write16(data, 0xECu, 0xD001u); /* ADD.B D1,D0 */
    ng_exec_fixture_write16(data, 0xEEu, 0x9001u); /* SUB.B D1,D0 */
    ng_exec_fixture_write16(data, 0xF0u, 0xB001u); /* CMP.B D1,D0 */
    ng_exec_fixture_write16(data, 0xF2u, 0x13C0u); /* MOVE.B D0,$00101D */
    ng_exec_fixture_write32(data, 0xF4u, 0x0000101Du);
    ng_exec_fixture_write16(data, 0xF8u, 0x7000u); /* MOVEQ #0,D0 */
    ng_exec_fixture_write16(data, 0xFAu, 0x7200u); /* MOVEQ #0,D1 */
    ng_exec_fixture_write16(data, 0xFCu, 0x5300u); /* SUBQ.B #1,D0, sets X */
    ng_exec_fixture_write16(data, 0xFEu, 0xD101u); /* ADDX.B D1,D0 */
    ng_exec_fixture_write16(data, 0x100u, 0x13C0u); /* MOVE.B D0,$00101C */
    ng_exec_fixture_write32(data, 0x102u, 0x0000101Cu);
    ng_exec_fixture_write16(data, 0x106u, 0x13FCu); /* MOVE.B #$80,$00100E */
    ng_exec_fixture_write16(data, 0x108u, 0x0080u);
    ng_exec_fixture_write32(data, 0x10Au, 0x0000100Eu);
    ng_exec_fixture_write16(data, 0x10Eu, 0x207Cu); /* MOVEA.L #$00000120,A0 */
    ng_exec_fixture_write32(data, 0x110u, 0x00000120u);
    ng_exec_fixture_write16(data, 0x114u, 0x2248u); /* MOVEA.L A0,A1 */
    ng_exec_fixture_write16(data, 0x116u, 0x123Cu); /* MOVE.B #$5A,D1 */
    ng_exec_fixture_write16(data, 0x118u, 0x005Au);
    ng_exec_fixture_write16(data, 0x11Au, 0x10C1u); /* MOVE.B D1,(A0)+ */
    ng_exec_fixture_write16(data, 0x11Cu, 0x207Cu); /* MOVEA.L #$00000120,A0 */
    ng_exec_fixture_write32(data, 0x11Eu, 0x00000120u);
    ng_exec_fixture_write16(data, 0x122u, 0x227Cu); /* MOVEA.L #$00000124,A1 */
    ng_exec_fixture_write32(data, 0x124u, 0x00000124u);
    ng_exec_fixture_write16(data, 0x128u, 0x12D8u); /* MOVE.B (A0)+,(A1)+ */
    ng_exec_fixture_write16(data, 0x12Au, 0x4A81u); /* TST.L D1 */
    ng_exec_fixture_write16(data, 0x12Cu, 0x4A58u); /* TST.W (A0)+ */
    ng_exec_fixture_write16(data, 0x12Eu, 0x0C58u); /* CMPI.W #$005A,(A0)+ */
    ng_exec_fixture_write16(data, 0x130u, 0x005Au);
    ng_exec_fixture_write16(data, 0x132u, 0x0218u); /* ANDI.B #$0F,(A0)+ */
    ng_exec_fixture_write16(data, 0x134u, 0x000Fu);
    ng_exec_fixture_write16(data, 0x136u, 0x5442u); /* ADDQ.W #2,D2 */
    ng_exec_fixture_write16(data, 0x138u, 0x5318u); /* SUBQ.B #1,(A0)+ */
    ng_exec_fixture_write16(data, 0x13Au, 0x0042u); /* ORI.W #$00F0,D2 */
    ng_exec_fixture_write16(data, 0x13Cu, 0x00F0u);
    ng_exec_fixture_write16(data, 0x13Eu, 0x0A18u); /* EORI.B #$0F,(A0)+ */
    ng_exec_fixture_write16(data, 0x140u, 0x000Fu);
    ng_exec_fixture_write16(data, 0x142u, 0x0642u); /* ADDI.W #$0010,D2 */
    ng_exec_fixture_write16(data, 0x144u, 0x0010u);
    ng_exec_fixture_write16(data, 0x146u, 0x0418u); /* SUBI.B #$01,(A0)+ */
    ng_exec_fixture_write16(data, 0x148u, 0x0001u);
    ng_exec_fixture_write16(data, 0x14Au, 0x08D8u); /* BSET #0,(A0)+ */
    ng_exec_fixture_write16(data, 0x14Cu, 0x0000u);
    ng_exec_fixture_write16(data, 0x14Eu, 0x0842u); /* BCHG #0,D2 */
    ng_exec_fixture_write16(data, 0x150u, 0x0000u);
    ng_exec_fixture_write16(data, 0x152u, 0x01C2u); /* BSET D0,D2 */
    ng_exec_fixture_write16(data, 0x154u, 0x03D8u); /* BSET D1,(A0)+ */
    ng_exec_fixture_write16(data, 0x156u, 0x4442u); /* NEG.W D2 */
    ng_exec_fixture_write16(data, 0x158u, 0x4618u); /* NOT.B (A0)+ */
    ng_exec_fixture_write16(data, 0x15Au, 0x163Cu); /* MOVE.B #$80,D3 */
    ng_exec_fixture_write16(data, 0x15Cu, 0x0080u);
    ng_exec_fixture_write16(data, 0x15Eu, 0x4883u); /* EXT.W D3 */
    ng_exec_fixture_write16(data, 0x160u, 0x48C3u); /* EXT.L D3 */
    ng_exec_fixture_write16(data, 0x162u, 0x4842u); /* SWAP D2 */
    ng_exec_fixture_write16(data, 0x164u, 0x247Cu); /* MOVEA.L #$00000100,A2 */
    ng_exec_fixture_write32(data, 0x166u, 0x00000100u);
    ng_exec_fixture_write16(data, 0x16Au, 0xD4FCu); /* ADDA.W #$0010,A2 */
    ng_exec_fixture_write16(data, 0x16Cu, 0x0010u);
    ng_exec_fixture_write16(data, 0x16Eu, 0x95FCu); /* SUBA.L #$00000008,A2 */
    ng_exec_fixture_write32(data, 0x170u, 0x00000008u);
    ng_exec_fixture_write16(data, 0x174u, 0xB5FCu); /* CMPA.L #$00000108,A2 */
    ng_exec_fixture_write32(data, 0x176u, 0x00000108u);
    ng_exec_fixture_write16(data, 0x17Au, 0x1E3Cu); /* MOVE.B #$0F,D7 */
    ng_exec_fixture_write16(data, 0x17Cu, 0x000Fu);
    ng_exec_fixture_write16(data, 0x17Eu, 0x8607u); /* OR.B D7,D3 */
    ng_exec_fixture_write16(data, 0x180u, 0xC607u); /* AND.B D7,D3 */
    ng_exec_fixture_write16(data, 0x182u, 0xBF03u); /* EOR.B D7,D3 */
    ng_exec_fixture_write16(data, 0x184u, 0x8F18u); /* OR.B D7,(A0)+ */
    ng_exec_fixture_write16(data, 0x186u, 0xCF18u); /* AND.B D7,(A0)+ */
    ng_exec_fixture_write16(data, 0x188u, 0xBF18u); /* EOR.B D7,(A0)+ */
    ng_exec_fixture_write16(data, 0x18Au, 0xDF18u); /* ADD.B D7,(A0)+ */
    ng_exec_fixture_write16(data, 0x18Cu, 0x9F18u); /* SUB.B D7,(A0)+ */
    ng_exec_fixture_write16(data, 0x18Eu, 0x2E7Cu); /* MOVEA.L #$00000140,A7 */
    ng_exec_fixture_write32(data, 0x190u, 0x00000140u);
    ng_exec_fixture_write16(data, 0x194u, 0x4868u); /* PEA ($10,A0) */
    ng_exec_fixture_write16(data, 0x196u, 0x0010u);
    ng_exec_fixture_write16(data, 0x198u, 0x2A7Cu); /* MOVEA.L #$00000160,A5 */
    ng_exec_fixture_write32(data, 0x19Au, 0x00000160u);
    ng_exec_fixture_write16(data, 0x19Eu, 0x4E55u); /* LINK A5,#-4 */
    ng_exec_fixture_write16(data, 0x1A0u, 0xFFFCu);
    ng_exec_fixture_write16(data, 0x1A2u, 0x4E5Du); /* UNLK A5 */
    ng_exec_fixture_write16(data, 0x1A4u, 0x6B08u); /* BMI $0001AE, taken */
    ng_exec_fixture_write16(data, 0x1A6u, 0x13FCu); /* skipped */
    ng_exec_fixture_write16(data, 0x1A8u, 0x0077u);
    ng_exec_fixture_write32(data, 0x1AAu, 0x0000101Eu);
    ng_exec_fixture_write16(data, 0x1AEu, 0x5BD8u); /* SMI (A0)+ */
    ng_exec_fixture_write16(data, 0x1B0u, 0x7E00u); /* MOVEQ #0,D7 */
    ng_exec_fixture_write16(data, 0x1B2u, 0x51CFu); /* DBF D7,$0001AE, not taken */
    ng_exec_fixture_write16(data, 0x1B4u, 0xFFFAu);
    ng_exec_fixture_write16(data, 0x1B6u, 0x3E3Cu); /* MOVE.W #$0003,D7 */
    ng_exec_fixture_write16(data, 0x1B8u, 0x0003u);
    ng_exec_fixture_write16(data, 0x1BAu, 0xCEFCu); /* MULU.W #$0004,D7 */
    ng_exec_fixture_write16(data, 0x1BCu, 0x0004u);
    ng_exec_fixture_write16(data, 0x1BEu, 0xCFFCu); /* MULS.W #$FFFE,D7 */
    ng_exec_fixture_write16(data, 0x1C0u, 0xFFFEu);
    ng_exec_fixture_write16(data, 0x1C2u, 0x7012u); /* MOVEQ #$12,D0 */
    ng_exec_fixture_write16(data, 0x1C4u, 0x7234u); /* MOVEQ #$34,D1 */
    ng_exec_fixture_write16(data, 0x1C6u, 0xC141u); /* EXG D0,D1 */
    ng_exec_fixture_write16(data, 0x1C8u, 0x47E8u); /* LEA ($10,A0),A3 */
    ng_exec_fixture_write16(data, 0x1CAu, 0x0010u);
    ng_exec_fixture_write16(data, 0x1CCu, 0x2E3Cu); /* MOVE.L #$00000014,D7 */
    ng_exec_fixture_write32(data, 0x1CEu, 0x00000014u);
    ng_exec_fixture_write16(data, 0x1D2u, 0x8EFCu); /* DIVU.W #$0004,D7 */
    ng_exec_fixture_write16(data, 0x1D4u, 0x0004u);
    ng_exec_fixture_write16(data, 0x1D6u, 0x7EF6u); /* MOVEQ #-10,D7 */
    ng_exec_fixture_write16(data, 0x1D8u, 0x8FFCu); /* DIVS.W #$FFFE,D7 */
    ng_exec_fixture_write16(data, 0x1DAu, 0xFFFEu);
    ng_exec_fixture_write16(data, 0x1DCu, 0xE34Fu); /* LSL.W #1,D7 */
    ng_exec_fixture_write16(data, 0x1DEu, 0xE24Fu); /* LSR.W #1,D7 */
    ng_exec_fixture_write16(data, 0x1E0u, 0x287Cu); /* MOVEA.L #$00000180,A4 */
    ng_exec_fixture_write32(data, 0x1E2u, 0x00000180u);
    ng_exec_fixture_write16(data, 0x1E6u, 0x48D4u); /* MOVEM.L D0-D1,(A4) */
    ng_exec_fixture_write16(data, 0x1E8u, 0x0003u);
    ng_exec_fixture_write16(data, 0x1EAu, 0x4CD4u); /* MOVEM.L (A4),D0-D1 */
    ng_exec_fixture_write16(data, 0x1ECu, 0x0003u);
    ng_exec_fixture_write16(data, 0x1EEu, 0x44FCu); /* MOVE #$001B,CCR */
    ng_exec_fixture_write16(data, 0x1F0u, 0x001Bu);
    ng_exec_fixture_write16(data, 0x1F2u, 0x003Cu); /* ORI #$04,CCR */
    ng_exec_fixture_write16(data, 0x1F4u, 0x0004u);
    ng_exec_fixture_write16(data, 0x1F6u, 0x0A3Cu); /* EORI #$04,CCR */
    ng_exec_fixture_write16(data, 0x1F8u, 0x0004u);
    ng_exec_fixture_write16(data, 0x1FAu, 0x023Cu); /* ANDI #$1F,CCR */
    ng_exec_fixture_write16(data, 0x1FCu, 0x001Fu);
    ng_exec_fixture_write16(data, 0x1FEu, 0x007Cu); /* ORI #$0100,SR */
    ng_exec_fixture_write16(data, 0x200u, 0x0100u);
    ng_exec_fixture_write16(data, 0x202u, 0x0A7Cu); /* EORI #$0100,SR */
    ng_exec_fixture_write16(data, 0x204u, 0x0100u);
    ng_exec_fixture_write16(data, 0x206u, 0x027Cu); /* ANDI #$FFFF,SR */
    ng_exec_fixture_write16(data, 0x208u, 0xFFFFu);
    ng_exec_fixture_write16(data, 0x20Au, 0x40F9u); /* MOVE SR,$00000188 */
    ng_exec_fixture_write32(data, 0x20Cu, 0x00000188u);
    ng_exec_fixture_write16(data, 0x210u, 0x33FCu); /* MOVE.W #$0003,$0000018A */
    ng_exec_fixture_write16(data, 0x212u, 0x0003u);
    ng_exec_fixture_write32(data, 0x214u, 0x0000018Au);
    ng_exec_fixture_write16(data, 0x218u, 0xE3F9u); /* LSL.W $0000018A */
    ng_exec_fixture_write32(data, 0x21Au, 0x0000018Au);
    ng_exec_fixture_write16(data, 0x21Eu, 0x003Cu); /* ORI #$10,CCR */
    ng_exec_fixture_write16(data, 0x220u, 0x0010u);
    ng_exec_fixture_write16(data, 0x222u, 0x13FCu); /* MOVE.B #$02,$0000018C */
    ng_exec_fixture_write16(data, 0x224u, 0x0002u);
    ng_exec_fixture_write32(data, 0x226u, 0x0000018Cu);
    ng_exec_fixture_write16(data, 0x22Au, 0x4039u); /* NEGX.B $0000018C */
    ng_exec_fixture_write32(data, 0x22Cu, 0x0000018Cu);
    ng_exec_fixture_write16(data, 0x230u, 0x13FCu); /* MOVE.B #$01,$0000018D */
    ng_exec_fixture_write16(data, 0x232u, 0x0001u);
    ng_exec_fixture_write32(data, 0x234u, 0x0000018Du);
    ng_exec_fixture_write16(data, 0x238u, 0x4AF9u); /* TAS $0000018D */
    ng_exec_fixture_write32(data, 0x23Au, 0x0000018Du);
    ng_exec_fixture_write16(data, 0x23Eu, 0x4BF9u); /* LEA $0000018E,A5 */
    ng_exec_fixture_write32(data, 0x240u, 0x0000018Eu);
    ng_exec_fixture_write16(data, 0x244u, 0x4DF9u); /* LEA $00000190,A6 */
    ng_exec_fixture_write32(data, 0x246u, 0x00000190u);
    ng_exec_fixture_write16(data, 0x24Au, 0x33FCu); /* MOVE.W #$0003,$0000018E */
    ng_exec_fixture_write16(data, 0x24Cu, 0x0003u);
    ng_exec_fixture_write32(data, 0x24Eu, 0x0000018Eu);
    ng_exec_fixture_write16(data, 0x252u, 0x33FCu); /* MOVE.W #$0005,$00000190 */
    ng_exec_fixture_write16(data, 0x254u, 0x0005u);
    ng_exec_fixture_write32(data, 0x256u, 0x00000190u);
    ng_exec_fixture_write16(data, 0x25Au, 0xBD4Du); /* CMPM.W (A5)+,(A6)+ */
    ng_exec_fixture_write16(data, 0x25Cu, 0x4E65u); /* MOVE A5,USP */
    ng_exec_fixture_write16(data, 0x25Eu, 0x4E6Cu); /* MOVE USP,A4 */
    ng_exec_fixture_write16(data, 0x260u, 0x303Cu); /* MOVE.W #$0001,D0 */
    ng_exec_fixture_write16(data, 0x262u, 0x0001u);
    ng_exec_fixture_write16(data, 0x264u, 0x323Cu); /* MOVE.W #$0005,D1 */
    ng_exec_fixture_write16(data, 0x266u, 0x0005u);
    ng_exec_fixture_write16(data, 0x268u, 0x023Cu); /* ANDI #$EF,CCR, clears X */
    ng_exec_fixture_write16(data, 0x26Au, 0x00EFu);
    ng_exec_fixture_write16(data, 0x26Cu, 0x9340u); /* SUBX.W D0,D1 */
    ng_exec_fixture_write16(data, 0x26Eu, 0x33C1u); /* MOVE.W D1,$00000192 */
    ng_exec_fixture_write32(data, 0x270u, 0x00000192u);
    ng_exec_fixture_write16(data, 0x274u, 0x003Cu); /* ORI #$10,CCR, sets X */
    ng_exec_fixture_write16(data, 0x276u, 0x0010u);
    ng_exec_fixture_write16(data, 0x278u, 0x13FCu); /* MOVE.B #$45,$00000194 */
    ng_exec_fixture_write16(data, 0x27Au, 0x0045u);
    ng_exec_fixture_write32(data, 0x27Cu, 0x00000194u);
    ng_exec_fixture_write16(data, 0x280u, 0x4839u); /* NBCD $00000194 */
    ng_exec_fixture_write32(data, 0x282u, 0x00000194u);
    ng_exec_fixture_write16(data, 0x286u, 0x023Cu); /* ANDI #$EF,CCR, clears X */
    ng_exec_fixture_write16(data, 0x288u, 0x00EFu);
    ng_exec_fixture_write16(data, 0x28Au, 0x103Cu); /* MOVE.B #$45,D0 */
    ng_exec_fixture_write16(data, 0x28Cu, 0x0045u);
    ng_exec_fixture_write16(data, 0x28Eu, 0x123Cu); /* MOVE.B #$54,D1 */
    ng_exec_fixture_write16(data, 0x290u, 0x0054u);
    ng_exec_fixture_write16(data, 0x292u, 0xC300u); /* ABCD D0,D1 */
    ng_exec_fixture_write16(data, 0x294u, 0x13C1u); /* MOVE.B D1,$00000195 */
    ng_exec_fixture_write32(data, 0x296u, 0x00000195u);
    ng_exec_fixture_write16(data, 0x29Au, 0x103Cu); /* MOVE.B #$12,D0 */
    ng_exec_fixture_write16(data, 0x29Cu, 0x0012u);
    ng_exec_fixture_write16(data, 0x29Eu, 0x123Cu); /* MOVE.B #$50,D1 */
    ng_exec_fixture_write16(data, 0x2A0u, 0x0050u);
    ng_exec_fixture_write16(data, 0x2A2u, 0x8300u); /* SBCD D0,D1 */
    ng_exec_fixture_write16(data, 0x2A4u, 0x13C1u); /* MOVE.B D1,$00000196 */
    ng_exec_fixture_write32(data, 0x2A6u, 0x00000196u);
    ng_exec_fixture_write16(data, 0x2AAu, 0x303Cu); /* MOVE.W #$0005,D0 */
    ng_exec_fixture_write16(data, 0x2ACu, 0x0005u);
    ng_exec_fixture_write16(data, 0x2AEu, 0x41BCu); /* CHK #$000A,D0 */
    ng_exec_fixture_write16(data, 0x2B0u, 0x000Au);
    ng_exec_fixture_write16(data, 0x2B2u, 0x7000u); /* MOVEQ #0,D0 */
    ng_exec_fixture_write16(data, 0x2B4u, 0x7200u); /* MOVEQ #0,D1 */
    ng_exec_fixture_write16(data, 0x2B6u, 0x5300u); /* SUBQ.B #1,D0, sets X */
    ng_exec_fixture_write16(data, 0x2B8u, 0xD101u); /* ADDX.B D1,D0 */
    ng_exec_fixture_write16(data, 0x2BAu, 0x13C0u); /* MOVE.B D0,$00101C */
    ng_exec_fixture_write32(data, 0x2BCu, 0x0000101Cu);
    ng_exec_fixture_write16(data, 0x2C0u, 0x13FCu); /* MOVE.B #$80,$00100E */
    ng_exec_fixture_write16(data, 0x2C2u, 0x0080u);
    ng_exec_fixture_write32(data, 0x2C4u, 0x0000100Eu);
    ng_exec_fixture_write16(data, 0x2C8u, 0x4E72u); /* STOP #$2018 */
    ng_exec_fixture_write16(data, 0x2CAu, 0x2018u);

    ng_exec_fixture_write16(data, 0x2D0u, 0x4EB9u); /* JSR $000002E0 */
    ng_exec_fixture_write32(data, 0x2D2u, 0x000002E0u);
    ng_exec_fixture_write16(data, 0x2D6u, 0x4E72u); /* STOP #$1111, skipped when RTS honors modified stack PC */
    ng_exec_fixture_write16(data, 0x2D8u, 0x1111u);
    ng_exec_fixture_write16(data, 0x2DCu, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x2DEu, 0x2700u);
    ng_exec_fixture_write16(data, 0x2E0u, 0x2EBCu); /* MOVE.L #$000002DC,(A7) */
    ng_exec_fixture_write32(data, 0x2E2u, 0x000002DCu);
    ng_exec_fixture_write16(data, 0x2E6u, 0x23D7u); /* MOVE.L (A7),$001000 */
    ng_exec_fixture_write32(data, 0x2E8u, 0x00001000u);
    ng_exec_fixture_write16(data, 0x2ECu, 0x4E75u); /* RTS */

    ng_exec_fixture_write16(data, 0x2F0u, 0x46FCu); /* MOVE #$0000,SR */
    ng_exec_fixture_write16(data, 0x2F2u, 0x0000u);
    ng_exec_fixture_write16(data, 0x2F4u, 0x4E40u); /* TRAP #0 */
    ng_exec_fixture_write16(data, 0x2F6u, 0x4E72u); /* STOP #$1111, not reached */
    ng_exec_fixture_write16(data, 0x2F8u, 0x1111u);
    ng_exec_fixture_write16(data, 0x300u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x302u, 0x2700u);

    ng_exec_fixture_write16(data, 0x330u, 0x4E73u); /* RTE */
    ng_exec_fixture_write16(data, 0x340u, 0x4E40u); /* TRAP #0 */

    ng_exec_fixture_write16(data, 0x350u, 0x4E40u); /* TRAP #0 */
    ng_exec_fixture_write16(data, 0x360u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x362u, 0x2700u);

    ng_exec_fixture_write16(data, 0x370u, 0x46FCu); /* MOVE #$2700,SR */
    ng_exec_fixture_write16(data, 0x372u, 0x2700u);
    ng_exec_fixture_write16(data, 0x374u, 0x4E72u); /* STOP #$1111, not reached on privilege trap */
    ng_exec_fixture_write16(data, 0x376u, 0x1111u);
    ng_exec_fixture_write16(data, 0x390u, 0x4E72u); /* privilege handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x392u, 0x2700u);
    ng_exec_fixture_write16(data, 0x3A0u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x3A2u, 0x2700u);
    ng_exec_fixture_write16(data, 0x3B0u, 0x4E73u); /* RTE */
    ng_exec_fixture_write16(data, 0x3C0u, 0x4E60u); /* MOVE A0,USP */
    ng_exec_fixture_write16(data, 0x3C2u, 0x4E72u); /* STOP #$1111, not reached on privilege trap */
    ng_exec_fixture_write16(data, 0x3C4u, 0x1111u);
    ng_exec_fixture_write16(data, 0x3D0u, 0x4E70u); /* RESET */
    ng_exec_fixture_write16(data, 0x3D2u, 0x4E72u); /* STOP #$1111, not reached on privilege trap */
    ng_exec_fixture_write16(data, 0x3D4u, 0x1111u);
    ng_exec_fixture_write16(data, 0x3E0u, 0x007Cu); /* ORI #$0100,SR */
    ng_exec_fixture_write16(data, 0x3E2u, 0x0100u);
    ng_exec_fixture_write16(data, 0x3E4u, 0x4E72u); /* STOP #$1111, not reached on privilege trap */
    ng_exec_fixture_write16(data, 0x3E6u, 0x1111u);

    ng_exec_fixture_write16(data, 0x400u, 0x4E72u); /* STOP #$2000 */
    ng_exec_fixture_write16(data, 0x402u, 0x2000u);
    ng_exec_fixture_write16(data, 0x404u, 0x7007u); /* MOVEQ #7,D0, resumed after interrupt RTE */
    ng_exec_fixture_write16(data, 0x406u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x408u, 0x2700u);
    ng_exec_fixture_write16(data, 0x410u, 0x4E73u); /* RTE */

    ng_exec_fixture_write16(data, 0x420u, 0x7001u); /* MOVEQ #1,D0 */
    ng_exec_fixture_write16(data, 0x422u, 0x7202u); /* MOVEQ #2,D1 */
    ng_exec_fixture_write16(data, 0x424u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x426u, 0x2700u);
    ng_exec_fixture_write16(data, 0x430u, 0x4E73u); /* RTE */

    ng_exec_fixture_write16(data, 0x440u, 0x7403u); /* MOVEQ #3,D2 */
    ng_exec_fixture_write16(data, 0x442u, 0x4E72u); /* STOP #$2700, not reached before trace handler */
    ng_exec_fixture_write16(data, 0x444u, 0x2700u);
    ng_exec_fixture_write16(data, 0x450u, 0x4E72u); /* trace handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x452u, 0x2700u);

    ng_exec_fixture_write16(data, 0x460u, 0x6B04u); /* BMI $000466 */
    ng_exec_fixture_write16(data, 0x462u, 0x4E71u); /* not-taken fallthrough NOP */
    ng_exec_fixture_write16(data, 0x464u, 0x4E71u); /* not-taken fallthrough NOP */
    ng_exec_fixture_write16(data, 0x466u, 0x4E72u); /* taken target: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x468u, 0x2700u);
    ng_exec_fixture_write16(data, 0x470u, 0x4E72u); /* branch trace handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x472u, 0x2700u);

    ng_exec_fixture_write16(data, 0x480u, 0x51C8u); /* DBF D0,$000486 */
    ng_exec_fixture_write16(data, 0x482u, 0x0004u);
    ng_exec_fixture_write16(data, 0x484u, 0x4E71u); /* fallthrough NOP */
    ng_exec_fixture_write16(data, 0x486u, 0x4E72u); /* taken target: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x488u, 0x2700u);
    ng_exec_fixture_write16(data, 0x490u, 0x4E72u); /* DBcc trace handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x492u, 0x2700u);
}

static NG_EXEC_FIXTURE_MAYBE_UNUSED uint32_t ng_exec_fixture_addr(uint32_t index) {
    static const uint32_t addrs[NG_EXEC_FIXTURE_ADDR_COUNT] = {
        0x00000000u,
        0x0000000Au,
        0x00000020u,
        0x00000060u,
        0x000002D0u,
        0x000002D6u,
        0x000002DCu,
        0x000002E0u,
        0x000002F0u,
        0x00000300u,
        0x00000330u,
        0x00000340u,
        0x00000350u,
        0x00000360u,
        0x00000370u,
        0x00000390u,
        0x000003A0u,
        0x000003B0u,
        0x000003C0u,
        0x000003D0u,
        0x000003E0u,
        0x00000400u,
        0x00000404u,
        0x00000406u,
        0x00000410u,
        0x00000420u,
        0x00000422u,
        0x00000424u,
        0x00000430u,
        0x00000440u,
        0x00000450u,
        0x00000460u,
        0x00000470u,
        0x00000480u,
        0x00000490u,
    };
    return index < NG_EXEC_FIXTURE_ADDR_COUNT ? addrs[index] : 0;
}

#undef NG_EXEC_FIXTURE_MAYBE_UNUSED
