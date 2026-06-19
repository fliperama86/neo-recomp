#pragma once

#include <stdint.h>
#include <string.h>

#define NG_EXEC_FIXTURE_SIZE 0x5F00u
#define NG_EXEC_FIXTURE_ADDR_COUNT 137u

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

    ng_exec_fixture_write16(data, 0x4A0u, 0x4EB9u); /* JSR $0004B0 */
    ng_exec_fixture_write32(data, 0x4A2u, 0x000004B0u);
    ng_exec_fixture_write16(data, 0x4A6u, 0x4E72u); /* return continuation: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x4A8u, 0x2700u);
    ng_exec_fixture_write16(data, 0x4B0u, 0x4E72u); /* callee target: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x4B2u, 0x2700u);
    ng_exec_fixture_write16(data, 0x4C0u, 0x4E72u); /* JSR trace handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x4C2u, 0x2700u);

    ng_exec_fixture_write16(data, 0x4D0u, 0x4E72u); /* RTS return target: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x4D2u, 0x2700u);
    ng_exec_fixture_write16(data, 0x4E0u, 0x4E75u); /* RTS */
    ng_exec_fixture_write16(data, 0x4F0u, 0x4E72u); /* RTS trace handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x4F2u, 0x2700u);

    ng_exec_fixture_write16(data, 0x500u, 0x4E72u); /* STOP #$2700 with trace enabled at entry */
    ng_exec_fixture_write16(data, 0x502u, 0x2700u);
    ng_exec_fixture_write16(data, 0x510u, 0x4E72u); /* STOP/RTE trace handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x512u, 0x2700u);
    ng_exec_fixture_write16(data, 0x520u, 0x4E73u); /* RTE with trace enabled at entry */
    ng_exec_fixture_write16(data, 0x530u, 0x4E72u); /* RTE return target: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x532u, 0x2700u);

    ng_exec_fixture_write16(data, 0x540u, 0x4E40u); /* TRAP #0 with trace enabled */
    ng_exec_fixture_write16(data, 0x550u, 0x4E72u); /* trap handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x552u, 0x2700u);
    ng_exec_fixture_write16(data, 0x560u, 0x4E72u); /* trace-after-trap handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x562u, 0x2700u);

    ng_exec_fixture_write16(data, 0x580u, 0x4E76u); /* TRAPV with trace enabled */
    ng_exec_fixture_write16(data, 0x582u, 0x4E72u); /* fallthrough when V clear: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x584u, 0x2700u);
    ng_exec_fixture_write16(data, 0x590u, 0x4E72u); /* TRAPV handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x592u, 0x2700u);
    ng_exec_fixture_write16(data, 0x5A0u, 0x4E72u); /* trace-after-TRAPV handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x5A2u, 0x2700u);

    ng_exec_fixture_write16(data, 0x5C0u, 0x41BCu); /* CHK #$000A,D0 with D0 out of range */
    ng_exec_fixture_write16(data, 0x5C2u, 0x000Au);
    ng_exec_fixture_write16(data, 0x5D0u, 0x4E72u); /* CHK handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x5D2u, 0x2700u);
    ng_exec_fixture_write16(data, 0x5E0u, 0x4E72u); /* trace-after-CHK handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x5E2u, 0x2700u);

    ng_exec_fixture_write16(data, 0x5F0u, 0x4AFCu); /* ILLEGAL with trace enabled */
    ng_exec_fixture_write16(data, 0x600u, 0x4E72u); /* illegal handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x602u, 0x2700u);
    ng_exec_fixture_write16(data, 0x610u, 0x4E72u); /* no-trace sentinel: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x612u, 0x2700u);

    ng_exec_fixture_write16(data, 0x620u, 0x8EFCu); /* DIVU.W #$0000,D7 */
    ng_exec_fixture_write16(data, 0x622u, 0x0000u);
    ng_exec_fixture_write16(data, 0x630u, 0x4E72u); /* divide-by-zero handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x632u, 0x2700u);
    ng_exec_fixture_write16(data, 0x640u, 0x4E72u); /* trace-after-divide handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x642u, 0x2700u);

    ng_exec_fixture_write16(data, 0x650u, 0x4EB9u); /* JSR $000670, callee skips local return */
    ng_exec_fixture_write32(data, 0x652u, 0x00000670u);
    ng_exec_fixture_write16(data, 0x660u, 0x4E72u); /* caller continuation: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x662u, 0x2700u);
    ng_exec_fixture_write16(data, 0x670u, 0x588Fu); /* ADDQ.L #4,A7 */
    ng_exec_fixture_write16(data, 0x672u, 0x4E75u); /* RTS */

    ng_exec_fixture_write16(data, 0x680u, 0xA000u); /* A-line emulator exception */
    ng_exec_fixture_write16(data, 0x690u, 0x4E72u); /* A-line handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x692u, 0x2700u);
    ng_exec_fixture_write16(data, 0x6A0u, 0xF000u); /* F-line emulator exception */
    ng_exec_fixture_write16(data, 0x6B0u, 0x4E72u); /* F-line handler: STOP #$2700 */
    ng_exec_fixture_write16(data, 0x6B2u, 0x2700u);

    ng_exec_fixture_write16(data, 0x6C0u, 0x303Cu); /* MOVE.W #$8001,D0 */
    ng_exec_fixture_write16(data, 0x6C2u, 0x8001u);
    ng_exec_fixture_write16(data, 0x6C4u, 0x7200u); /* MOVEQ #0,D1: register-count zero */
    ng_exec_fixture_write16(data, 0x6C6u, 0x44FCu); /* MOVE #$0017,CCR */
    ng_exec_fixture_write16(data, 0x6C8u, 0x0017u);
    ng_exec_fixture_write16(data, 0x6CAu, 0xE368u); /* LSL.W D1,D0 */
    ng_exec_fixture_write16(data, 0x6CCu, 0x40F9u); /* MOVE SR,$000001A0 */
    ng_exec_fixture_write32(data, 0x6CEu, 0x000001A0u);
    ng_exec_fixture_write16(data, 0x6D2u, 0x44FCu); /* MOVE #$0017,CCR */
    ng_exec_fixture_write16(data, 0x6D4u, 0x0017u);
    ng_exec_fixture_write16(data, 0x6D6u, 0xE378u); /* ROL.W D1,D0 */
    ng_exec_fixture_write16(data, 0x6D8u, 0x40F9u); /* MOVE SR,$000001A2 */
    ng_exec_fixture_write32(data, 0x6DAu, 0x000001A2u);
    ng_exec_fixture_write16(data, 0x6DEu, 0x44FCu); /* MOVE #$0016,CCR */
    ng_exec_fixture_write16(data, 0x6E0u, 0x0016u);
    ng_exec_fixture_write16(data, 0x6E2u, 0xE370u); /* ROXL.W D1,D0 */
    ng_exec_fixture_write16(data, 0x6E4u, 0x40F9u); /* MOVE SR,$000001A4 */
    ng_exec_fixture_write32(data, 0x6E6u, 0x000001A4u);
    ng_exec_fixture_write16(data, 0x6EAu, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x6ECu, 0x2700u);

    ng_exec_fixture_write16(data, 0x700u, 0x303Cu); /* MOVE.W #$8000,D0 */
    ng_exec_fixture_write16(data, 0x702u, 0x8000u);
    ng_exec_fixture_write16(data, 0x704u, 0x4840u); /* SWAP D0 -> $80000000 */
    ng_exec_fixture_write16(data, 0x706u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0x708u, 0x001Fu);
    ng_exec_fixture_write16(data, 0x70Au, 0x81FCu); /* DIVS.W #$FFFF,D0 */
    ng_exec_fixture_write16(data, 0x70Cu, 0xFFFFu);
    ng_exec_fixture_write16(data, 0x70Eu, 0x40F9u); /* MOVE SR,$000001A6 */
    ng_exec_fixture_write32(data, 0x710u, 0x000001A6u);
    ng_exec_fixture_write16(data, 0x714u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x716u, 0x2700u);

    ng_exec_fixture_write16(data, 0x720u, 0x203Cu); /* MOVE.L #$12345678,D0 */
    ng_exec_fixture_write32(data, 0x722u, 0x12345678u);
    ng_exec_fixture_write16(data, 0x726u, 0x40F9u); /* MOVE SR,$000001AC */
    ng_exec_fixture_write32(data, 0x728u, 0x000001ACu);
    ng_exec_fixture_write16(data, 0x72Cu, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x72Eu, 0x2700u);

    ng_exec_fixture_write16(data, 0x740u, 0x203Cu); /* MOVE.L #$12345678,D0 */
    ng_exec_fixture_write32(data, 0x742u, 0x12345678u);
    ng_exec_fixture_write16(data, 0x746u, 0x48E7u); /* MOVEM.L #$8000,-(A7): predec bit 15 is D0 */
    ng_exec_fixture_write16(data, 0x748u, 0x8000u);
    ng_exec_fixture_write16(data, 0x74Au, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x74Cu, 0x2700u);

    ng_exec_fixture_write16(data, 0x760u, 0x48E7u); /* MOVEM.L #$0001,-(A7): MC68000 stores initial A7 */
    ng_exec_fixture_write16(data, 0x762u, 0x0001u);
    ng_exec_fixture_write16(data, 0x764u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x766u, 0x2700u);

    ng_exec_fixture_write16(data, 0x780u, 0x207Cu); /* MOVEA.L #$000001B0,A0 */
    ng_exec_fixture_write32(data, 0x782u, 0x000001B0u);
    ng_exec_fixture_write16(data, 0x786u, 0x203Cu); /* MOVE.L #$A1B2C3D4,D0 */
    ng_exec_fixture_write32(data, 0x788u, 0xA1B2C3D4u);
    ng_exec_fixture_write16(data, 0x78Cu, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0x78Eu, 0x001Fu);
    ng_exec_fixture_write16(data, 0x790u, 0x01C8u); /* MOVEP.L D0,($10,A0) */
    ng_exec_fixture_write16(data, 0x792u, 0x0010u);
    ng_exec_fixture_write16(data, 0x794u, 0x0348u); /* MOVEP.L ($10,A0),D1 */
    ng_exec_fixture_write16(data, 0x796u, 0x0010u);
    ng_exec_fixture_write16(data, 0x798u, 0x40F9u); /* MOVE SR,$000001BC */
    ng_exec_fixture_write32(data, 0x79Au, 0x000001BCu);
    ng_exec_fixture_write16(data, 0x79Eu, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x7A0u, 0x2700u);

    ng_exec_fixture_write16(data, 0x7C0u, 0x203Cu); /* MOVE.L #$12345678,D0 */
    ng_exec_fixture_write32(data, 0x7C2u, 0x12345678u);
    ng_exec_fixture_write16(data, 0x7C6u, 0x207Cu); /* MOVEA.L #$12345678,A0 */
    ng_exec_fixture_write32(data, 0x7C8u, 0x12345678u);
    ng_exec_fixture_write16(data, 0x7CCu, 0x287Cu); /* MOVEA.L #$000001D0,A4 */
    ng_exec_fixture_write32(data, 0x7CEu, 0x000001D0u);
    ng_exec_fixture_write16(data, 0x7D2u, 0x33FCu); /* MOVE.W #$8001,$000001D0 */
    ng_exec_fixture_write16(data, 0x7D4u, 0x8001u);
    ng_exec_fixture_write32(data, 0x7D6u, 0x000001D0u);
    ng_exec_fixture_write16(data, 0x7DAu, 0x33FCu); /* MOVE.W #$7FFE,$000001D2 */
    ng_exec_fixture_write16(data, 0x7DCu, 0x7FFEu);
    ng_exec_fixture_write32(data, 0x7DEu, 0x000001D2u);
    ng_exec_fixture_write16(data, 0x7E2u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0x7E4u, 0x001Fu);
    ng_exec_fixture_write16(data, 0x7E6u, 0x4C94u); /* MOVEM.W (A4),D0/A0 */
    ng_exec_fixture_write16(data, 0x7E8u, 0x0101u);
    ng_exec_fixture_write16(data, 0x7EAu, 0x40F9u); /* MOVE SR,$000001D4 */
    ng_exec_fixture_write32(data, 0x7ECu, 0x000001D4u);
    ng_exec_fixture_write16(data, 0x7F0u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x7F2u, 0x2700u);

    ng_exec_fixture_write16(data, 0x820u, 0x207Cu); /* MOVEA.L #$000001E2,A0 */
    ng_exec_fixture_write32(data, 0x822u, 0x000001E2u);
    ng_exec_fixture_write16(data, 0x826u, 0x227Cu); /* MOVEA.L #$000001E4,A1 */
    ng_exec_fixture_write32(data, 0x828u, 0x000001E4u);
    ng_exec_fixture_write16(data, 0x82Cu, 0x247Cu); /* MOVEA.L #$000001E6,A2 */
    ng_exec_fixture_write32(data, 0x82Eu, 0x000001E6u);
    ng_exec_fixture_write16(data, 0x832u, 0x267Cu); /* MOVEA.L #$000001E8,A3 */
    ng_exec_fixture_write32(data, 0x834u, 0x000001E8u);
    ng_exec_fixture_write16(data, 0x838u, 0x13FCu); /* MOVE.B #$45,$000001E1 */
    ng_exec_fixture_write16(data, 0x83Au, 0x0045u);
    ng_exec_fixture_write32(data, 0x83Cu, 0x000001E1u);
    ng_exec_fixture_write16(data, 0x840u, 0x13FCu); /* MOVE.B #$54,$000001E3 */
    ng_exec_fixture_write16(data, 0x842u, 0x0054u);
    ng_exec_fixture_write32(data, 0x844u, 0x000001E3u);
    ng_exec_fixture_write16(data, 0x848u, 0x13FCu); /* MOVE.B #$12,$000001E5 */
    ng_exec_fixture_write16(data, 0x84Au, 0x0012u);
    ng_exec_fixture_write32(data, 0x84Cu, 0x000001E5u);
    ng_exec_fixture_write16(data, 0x850u, 0x13FCu); /* MOVE.B #$50,$000001E7 */
    ng_exec_fixture_write16(data, 0x852u, 0x0050u);
    ng_exec_fixture_write32(data, 0x854u, 0x000001E7u);
    ng_exec_fixture_write16(data, 0x858u, 0x44FCu); /* MOVE #$0014,CCR */
    ng_exec_fixture_write16(data, 0x85Au, 0x0014u);
    ng_exec_fixture_write16(data, 0x85Cu, 0xC308u); /* ABCD -(A0),-(A1) */
    ng_exec_fixture_write16(data, 0x85Eu, 0x40F9u); /* MOVE SR,$000001F2 */
    ng_exec_fixture_write32(data, 0x860u, 0x000001F2u);
    ng_exec_fixture_write16(data, 0x864u, 0x870Au); /* SBCD -(A2),-(A3) */
    ng_exec_fixture_write16(data, 0x866u, 0x40F9u); /* MOVE SR,$000001F4 */
    ng_exec_fixture_write32(data, 0x868u, 0x000001F4u);
    ng_exec_fixture_write16(data, 0x86Cu, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x86Eu, 0x2700u);

    ng_exec_fixture_write16(data, 0x890u, 0x103Cu); /* MOVE.B #$80,D0 */
    ng_exec_fixture_write16(data, 0x892u, 0x0080u);
    ng_exec_fixture_write16(data, 0x894u, 0x123Cu); /* MOVE.B #$80,D1 */
    ng_exec_fixture_write16(data, 0x896u, 0x0080u);
    ng_exec_fixture_write16(data, 0x898u, 0xD001u); /* ADD.B D1,D0 */
    ng_exec_fixture_write16(data, 0x89Au, 0x40F9u); /* MOVE SR,$000001F6 */
    ng_exec_fixture_write32(data, 0x89Cu, 0x000001F6u);
    ng_exec_fixture_write16(data, 0x8A0u, 0x103Cu); /* MOVE.B #$00,D0 */
    ng_exec_fixture_write16(data, 0x8A2u, 0x0000u);
    ng_exec_fixture_write16(data, 0x8A4u, 0x123Cu); /* MOVE.B #$01,D1 */
    ng_exec_fixture_write16(data, 0x8A6u, 0x0001u);
    ng_exec_fixture_write16(data, 0x8A8u, 0x9001u); /* SUB.B D1,D0 */
    ng_exec_fixture_write16(data, 0x8AAu, 0x40F9u); /* MOVE SR,$000001F8 */
    ng_exec_fixture_write32(data, 0x8ACu, 0x000001F8u);
    ng_exec_fixture_write16(data, 0x8B0u, 0x103Cu); /* MOVE.B #$80,D0 */
    ng_exec_fixture_write16(data, 0x8B2u, 0x0080u);
    ng_exec_fixture_write16(data, 0x8B4u, 0x123Cu); /* MOVE.B #$01,D1 */
    ng_exec_fixture_write16(data, 0x8B6u, 0x0001u);
    ng_exec_fixture_write16(data, 0x8B8u, 0x44FCu); /* MOVE #$0010,CCR */
    ng_exec_fixture_write16(data, 0x8BAu, 0x0010u);
    ng_exec_fixture_write16(data, 0x8BCu, 0xB001u); /* CMP.B D1,D0 */
    ng_exec_fixture_write16(data, 0x8BEu, 0x40F9u); /* MOVE SR,$000001FA */
    ng_exec_fixture_write32(data, 0x8C0u, 0x000001FAu);
    ng_exec_fixture_write16(data, 0x8C4u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x8C6u, 0x2700u);

    ng_exec_fixture_write16(data, 0x8D0u, 0x103Cu); /* MOVE.B #$00,D0 */
    ng_exec_fixture_write16(data, 0x8D2u, 0x0000u);
    ng_exec_fixture_write16(data, 0x8D4u, 0x44FCu); /* MOVE #$0004,CCR */
    ng_exec_fixture_write16(data, 0x8D6u, 0x0004u);
    ng_exec_fixture_write16(data, 0x8D8u, 0x4800u); /* NBCD D0 */
    ng_exec_fixture_write16(data, 0x8DAu, 0x40F9u); /* MOVE SR,$000001FC */
    ng_exec_fixture_write32(data, 0x8DCu, 0x000001FCu);
    ng_exec_fixture_write16(data, 0x8E0u, 0x103Cu); /* MOVE.B #$45,D0 */
    ng_exec_fixture_write16(data, 0x8E2u, 0x0045u);
    ng_exec_fixture_write16(data, 0x8E4u, 0x44FCu); /* MOVE #$0014,CCR */
    ng_exec_fixture_write16(data, 0x8E6u, 0x0014u);
    ng_exec_fixture_write16(data, 0x8E8u, 0x4800u); /* NBCD D0 */
    ng_exec_fixture_write16(data, 0x8EAu, 0x40F9u); /* MOVE SR,$000001FE */
    ng_exec_fixture_write32(data, 0x8ECu, 0x000001FEu);
    ng_exec_fixture_write16(data, 0x8F0u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x8F2u, 0x2700u);

    ng_exec_fixture_write16(data, 0x900u, 0x287Cu); /* MOVEA.L #$00000210,A4 */
    ng_exec_fixture_write32(data, 0x902u, 0x00000210u);
    ng_exec_fixture_write16(data, 0x906u, 0x203Cu); /* MOVE.L #$11111111,D0 */
    ng_exec_fixture_write32(data, 0x908u, 0x11111111u);
    ng_exec_fixture_write16(data, 0x90Cu, 0x223Cu); /* MOVE.L #$22222222,D1 */
    ng_exec_fixture_write32(data, 0x90Eu, 0x22222222u);
    ng_exec_fixture_write16(data, 0x912u, 0x48D4u); /* MOVEM.L D0-D1,(A4) */
    ng_exec_fixture_write16(data, 0x914u, 0x0003u);
    ng_exec_fixture_write16(data, 0x916u, 0x203Cu); /* MOVE.L #$AAAAAAAA,D0 */
    ng_exec_fixture_write32(data, 0x918u, 0xAAAAAAAAu);
    ng_exec_fixture_write16(data, 0x91Cu, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0x91Eu, 0x001Fu);
    ng_exec_fixture_write16(data, 0x920u, 0x4CDCu); /* MOVEM.L (A4)+,D0/A4 */
    ng_exec_fixture_write16(data, 0x922u, 0x1001u);
    ng_exec_fixture_write16(data, 0x924u, 0x40F9u); /* MOVE SR,$00000208 */
    ng_exec_fixture_write32(data, 0x926u, 0x00000208u);
    ng_exec_fixture_write16(data, 0x92Au, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x92Cu, 0x2700u);

    ng_exec_fixture_write16(data, 0x940u, 0x207Cu); /* MOVEA.L #$00010000,A0 */
    ng_exec_fixture_write32(data, 0x942u, 0x00010000u);
    ng_exec_fixture_write16(data, 0x946u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0x948u, 0x001Fu);
    ng_exec_fixture_write16(data, 0x94Au, 0x5248u); /* ADDQ.W #1,A0 */
    ng_exec_fixture_write16(data, 0x94Cu, 0x5548u); /* SUBQ.W #2,A0 */
    ng_exec_fixture_write16(data, 0x94Eu, 0x40F9u); /* MOVE SR,$00000220 */
    ng_exec_fixture_write32(data, 0x950u, 0x00000220u);
    ng_exec_fixture_write16(data, 0x954u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x956u, 0x2700u);

    ng_exec_fixture_write16(data, 0x970u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0x972u, 0x001Fu);
    ng_exec_fixture_write16(data, 0x974u, 0x307Cu); /* MOVEA.W #$FFFF,A0 */
    ng_exec_fixture_write16(data, 0x976u, 0xFFFFu);
    ng_exec_fixture_write16(data, 0x978u, 0x327Cu); /* MOVEA.W #$8000,A1 */
    ng_exec_fixture_write16(data, 0x97Au, 0x8000u);
    ng_exec_fixture_write16(data, 0x97Cu, 0x347Cu); /* MOVEA.W #$7FFF,A2 */
    ng_exec_fixture_write16(data, 0x97Eu, 0x7FFFu);
    ng_exec_fixture_write16(data, 0x980u, 0x40F9u); /* MOVE SR,$00000222 */
    ng_exec_fixture_write32(data, 0x982u, 0x00000222u);
    ng_exec_fixture_write16(data, 0x986u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x988u, 0x2700u);

    ng_exec_fixture_write16(data, 0x9A0u, 0x207Cu); /* MOVEA.L #$00010000,A0 */
    ng_exec_fixture_write32(data, 0x9A2u, 0x00010000u);
    ng_exec_fixture_write16(data, 0x9A6u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0x9A8u, 0x001Fu);
    ng_exec_fixture_write16(data, 0x9AAu, 0xD0FCu); /* ADDA.W #$FFFF,A0 */
    ng_exec_fixture_write16(data, 0x9ACu, 0xFFFFu);
    ng_exec_fixture_write16(data, 0x9AEu, 0x90FCu); /* SUBA.W #$8000,A0 */
    ng_exec_fixture_write16(data, 0x9B0u, 0x8000u);
    ng_exec_fixture_write16(data, 0x9B2u, 0xB0FCu); /* CMPA.W #$7FFF,A0 */
    ng_exec_fixture_write16(data, 0x9B4u, 0x7FFFu);
    ng_exec_fixture_write16(data, 0x9B6u, 0x40F9u); /* MOVE SR,$00000224 */
    ng_exec_fixture_write32(data, 0x9B8u, 0x00000224u);
    ng_exec_fixture_write16(data, 0x9BCu, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x9BEu, 0x2700u);

    ng_exec_fixture_write16(data, 0x9D0u, 0x2E7Cu); /* MOVEA.L #$00000230,A7 */
    ng_exec_fixture_write32(data, 0x9D2u, 0x00000230u);
    ng_exec_fixture_write16(data, 0x9D6u, 0x207Cu); /* MOVEA.L #$00000240,A0 */
    ng_exec_fixture_write32(data, 0x9D8u, 0x00000240u);
    ng_exec_fixture_write16(data, 0x9DCu, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0x9DEu, 0x001Fu);
    ng_exec_fixture_write16(data, 0x9E0u, 0xB10Fu); /* CMPM.B (A7)+,(A0)+ */
    ng_exec_fixture_write16(data, 0x9E2u, 0x40F9u); /* MOVE SR,$00000226 */
    ng_exec_fixture_write32(data, 0x9E4u, 0x00000226u);
    ng_exec_fixture_write16(data, 0x9E8u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x9EAu, 0x2700u);

    ng_exec_fixture_write16(data, 0xA00u, 0x2E7Cu); /* MOVEA.L #$00000252,A7 */
    ng_exec_fixture_write32(data, 0xA02u, 0x00000252u);
    ng_exec_fixture_write16(data, 0xA06u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0xA08u, 0x001Fu);
    ng_exec_fixture_write16(data, 0xA0Au, 0x4827u); /* NBCD -(A7) */
    ng_exec_fixture_write16(data, 0xA0Cu, 0x40F9u); /* MOVE SR,$00000228 */
    ng_exec_fixture_write32(data, 0xA0Eu, 0x00000228u);
    ng_exec_fixture_write16(data, 0xA12u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0xA14u, 0x2700u);

    ng_exec_fixture_write16(data, 0xA20u, 0x7000u); /* MOVEQ #0,D0 */
    ng_exec_fixture_write16(data, 0xA22u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0xA24u, 0x001Fu);
    ng_exec_fixture_write16(data, 0xA26u, 0x4AC0u); /* TAS D0 */
    ng_exec_fixture_write16(data, 0xA28u, 0x40F9u); /* MOVE SR,$0000022A */
    ng_exec_fixture_write32(data, 0xA2Au, 0x0000022Au);
    ng_exec_fixture_write16(data, 0xA2Eu, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0xA30u, 0x2700u);

    ng_exec_fixture_write16(data, 0xA40u, 0x207Cu); /* MOVEA.L #$11112222,A0 */
    ng_exec_fixture_write32(data, 0xA42u, 0x11112222u);
    ng_exec_fixture_write16(data, 0xA46u, 0x227Cu); /* MOVEA.L #$33334444,A1 */
    ng_exec_fixture_write32(data, 0xA48u, 0x33334444u);
    ng_exec_fixture_write16(data, 0xA4Cu, 0x243Cu); /* MOVE.L #$55556666,D2 */
    ng_exec_fixture_write32(data, 0xA4Eu, 0x55556666u);
    ng_exec_fixture_write16(data, 0xA52u, 0x267Cu); /* MOVEA.L #$77778888,A3 */
    ng_exec_fixture_write32(data, 0xA54u, 0x77778888u);
    ng_exec_fixture_write16(data, 0xA58u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0xA5Au, 0x001Fu);
    ng_exec_fixture_write16(data, 0xA5Cu, 0xC149u); /* EXG A0,A1 */
    ng_exec_fixture_write16(data, 0xA5Eu, 0xC58Bu); /* EXG D2,A3 */
    ng_exec_fixture_write16(data, 0xA60u, 0x40F9u); /* MOVE SR,$0000022C */
    ng_exec_fixture_write32(data, 0xA62u, 0x0000022Cu);
    ng_exec_fixture_write16(data, 0xA66u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0xA68u, 0x2700u);

    ng_exec_fixture_write16(data, 0xA80u, 0x2E7Cu); /* MOVEA.L #$00000260,A7 */
    ng_exec_fixture_write32(data, 0xA82u, 0x00000260u);
    ng_exec_fixture_write16(data, 0xA86u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0xA88u, 0x001Fu);
    ng_exec_fixture_write16(data, 0xA8Au, 0x487Au); /* PEA ($0010,PC) */
    ng_exec_fixture_write16(data, 0xA8Cu, 0x0010u);
    ng_exec_fixture_write16(data, 0xA8Eu, 0x40F9u); /* MOVE SR,$0000022E */
    ng_exec_fixture_write32(data, 0xA90u, 0x0000022Eu);
    ng_exec_fixture_write16(data, 0xA94u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0xA96u, 0x2700u);

    ng_exec_fixture_write16(data, 0xAC0u, 0x7002u); /* MOVEQ #2,D0 */
    ng_exec_fixture_write16(data, 0xAC2u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0xAC4u, 0x001Fu);
    ng_exec_fixture_write16(data, 0xAC6u, 0x45FBu); /* LEA ($10,PC,D0.W),A2 */
    ng_exec_fixture_write16(data, 0xAC8u, 0x0010u);
    ng_exec_fixture_write16(data, 0xACAu, 0x40F9u); /* MOVE SR,$00000230 */
    ng_exec_fixture_write32(data, 0xACCu, 0x00000230u);
    ng_exec_fixture_write16(data, 0xAD0u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0xAD2u, 0x2700u);

    ng_exec_fixture_write16(data, 0xB00u, 0x203Cu); /* MOVE.L #$00010000,D0 */
    ng_exec_fixture_write32(data, 0xB02u, 0x00010000u);
    ng_exec_fixture_write16(data, 0xB06u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0xB08u, 0x001Fu);
    ng_exec_fixture_write16(data, 0xB0Au, 0x80FCu); /* DIVU.W #$0001,D0 */
    ng_exec_fixture_write16(data, 0xB0Cu, 0x0001u);
    ng_exec_fixture_write16(data, 0xB0Eu, 0x40F9u); /* MOVE SR,$00000232 */
    ng_exec_fixture_write32(data, 0xB10u, 0x00000232u);
    ng_exec_fixture_write16(data, 0xB14u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0xB16u, 0x2700u);

    ng_exec_fixture_write16(data, 0xB18u, 0x33FCu); /* MOVE.W #$8002,$00000B34 */
    ng_exec_fixture_write16(data, 0xB1Au, 0x8002u);
    ng_exec_fixture_write32(data, 0xB1Cu, 0x00000B34u);
    ng_exec_fixture_write16(data, 0xB20u, 0x33FCu); /* MOVE.W #$7FFD,$00000B36 */
    ng_exec_fixture_write16(data, 0xB22u, 0x7FFDu);
    ng_exec_fixture_write32(data, 0xB24u, 0x00000B36u);
    ng_exec_fixture_write16(data, 0xB28u, 0x7004u); /* MOVEQ #4,D0 */
    ng_exec_fixture_write16(data, 0xB2Au, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0xB2Cu, 0x001Fu);
    ng_exec_fixture_write16(data, 0xB2Eu, 0x4CBBu); /* MOVEM.W (-2,PC,D0.W),D5/A6 */
    ng_exec_fixture_write16(data, 0xB30u, 0x4020u);
    ng_exec_fixture_write16(data, 0xB32u, 0x00FEu);
    ng_exec_fixture_write16(data, 0xB34u, 0x40F9u); /* MOVE SR,$0000127C */
    ng_exec_fixture_write32(data, 0xB36u, 0x0000127Cu);
    ng_exec_fixture_write16(data, 0xB3Au, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0xB3Cu, 0x2700u);

    for (uint32_t chunk = 0; chunk < 4u; ++chunk) {
        uint32_t pc = 0xB40u + chunk * 0x1B0u;
        uint32_t first_flags = chunk * 4u;
        for (uint32_t flags = first_flags; flags < first_flags + 4u; ++flags) {
            ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #ccr,CCR */
            pc += 2u;
            ng_exec_fixture_write16(data, pc, (uint16_t)(0x0010u | flags));
            pc += 2u;
            for (uint32_t cond = 0; cond < 16u; ++cond) {
                uint32_t dest = 0x00000300u + flags * 16u + cond;
                ng_exec_fixture_write16(data, pc,
                                        (uint16_t)(0x50F9u | (cond << 8))); /* Scc $abs.l */
                pc += 2u;
                ng_exec_fixture_write32(data, pc, dest);
                pc += 4u;
            }
            ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$abs.l */
            pc += 2u;
            ng_exec_fixture_write32(data, pc, 0x00000500u + flags * 2u);
            pc += 4u;
        }
        ng_exec_fixture_write16(data, pc, 0x4E72u); /* STOP #$2700 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x2700u);
    }

    for (uint32_t chunk = 0; chunk < 8u; ++chunk) {
        uint32_t pc = 0x1220u + chunk * 0x2E0u;
        uint32_t first_flags = chunk * 2u;
        for (uint32_t flags = first_flags; flags < first_flags + 2u; ++flags) {
            for (uint32_t cond = 0; cond < 16u; ++cond) {
                uint32_t sr_dest = 0x00000800u + flags * 32u + cond * 2u;
                uint32_t d0_dest = 0x00000600u + flags * 32u + cond * 2u;
                ng_exec_fixture_write16(data, pc, 0x7000u); /* MOVEQ #0,D0 */
                pc += 2u;
                ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #ccr,CCR */
                pc += 2u;
                ng_exec_fixture_write16(data, pc, (uint16_t)(0x0010u | flags));
                pc += 2u;
                ng_exec_fixture_write16(data, pc,
                                        (uint16_t)(0x50C8u | (cond << 8))); /* DBcc D0,* */
                pc += 2u;
                ng_exec_fixture_write16(data, pc, 0x0002u);
                pc += 2u;
                ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$abs.l */
                pc += 2u;
                ng_exec_fixture_write32(data, pc, sr_dest);
                pc += 4u;
                ng_exec_fixture_write16(data, pc, 0x33C0u); /* MOVE.W D0,$abs.l */
                pc += 2u;
                ng_exec_fixture_write32(data, pc, d0_dest);
                pc += 4u;
            }
        }
        ng_exec_fixture_write16(data, pc, 0x4E72u); /* STOP #$2700 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x2700u);
    }

    for (uint32_t chunk = 0; chunk < 8u; ++chunk) {
        uint32_t pc = 0x2A00u + chunk * 0x200u;
        uint32_t first_flags = chunk * 2u;
        for (uint32_t flags = first_flags; flags < first_flags + 2u; ++flags) {
            for (uint32_t cond = 2u; cond < 16u; ++cond) {
                uint32_t dest = 0x00000A00u + flags * 16u + cond;
                ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #ccr,CCR */
                pc += 2u;
                ng_exec_fixture_write16(data, pc, (uint16_t)(0x0010u | flags));
                pc += 2u;
                ng_exec_fixture_write16(data, pc,
                                        (uint16_t)(0x6006u | (cond << 8))); /* Bcc.S skips marker */
                pc += 2u;
                ng_exec_fixture_write16(data, pc, 0x50F9u); /* ST $abs.l */
                pc += 2u;
                ng_exec_fixture_write32(data, pc, dest);
                pc += 4u;
            }
            ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$abs.l */
            pc += 2u;
            ng_exec_fixture_write32(data, pc, 0x00000B00u + flags * 2u);
            pc += 4u;
        }
        ng_exec_fixture_write16(data, pc, 0x4E72u); /* STOP #$2700 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x2700u);
    }

    for (uint32_t flags = 0u; flags < 16u; ++flags) {
        uint32_t pc = 0x3A00u + flags * 0x200u;
        for (uint32_t cond = 0u; cond < 16u; ++cond) {
            uint32_t marker_dest = 0x00000C00u + flags * 16u + cond;
            uint32_t d0_dest = 0x00000E00u + flags * 32u + cond * 2u;
            uint32_t sr_dest = 0x00001000u + flags * 32u + cond * 2u;
            ng_exec_fixture_write16(data, pc, 0x7001u); /* MOVEQ #1,D0 */
            pc += 2u;
            ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #ccr,CCR */
            pc += 2u;
            ng_exec_fixture_write16(data, pc, (uint16_t)(0x0010u | flags));
            pc += 2u;
            ng_exec_fixture_write16(data, pc,
                                    (uint16_t)(0x50C8u | (cond << 8))); /* DBcc D0,skip marker */
            pc += 2u;
            ng_exec_fixture_write16(data, pc, 0x0008u);
            pc += 2u;
            ng_exec_fixture_write16(data, pc, 0x50F9u); /* ST $abs.l */
            pc += 2u;
            ng_exec_fixture_write32(data, pc, marker_dest);
            pc += 4u;
            ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$abs.l */
            pc += 2u;
            ng_exec_fixture_write32(data, pc, sr_dest);
            pc += 4u;
            ng_exec_fixture_write16(data, pc, 0x33C0u); /* MOVE.W D0,$abs.l */
            pc += 2u;
            ng_exec_fixture_write32(data, pc, d0_dest);
            pc += 4u;
        }
        ng_exec_fixture_write16(data, pc, 0x4E72u); /* STOP #$2700 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x2700u);
    }

    ng_exec_fixture_write16(data, 0x5A00u, 0x203Cu); /* MOVE.L #$00000003,D0 */
    ng_exec_fixture_write32(data, 0x5A02u, 0x00000003u);
    ng_exec_fixture_write16(data, 0x5A06u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0x5A08u, 0x001Fu);
    ng_exec_fixture_write16(data, 0x5A0Au, 0x80FCu); /* DIVU.W #$0004,D0 */
    ng_exec_fixture_write16(data, 0x5A0Cu, 0x0004u);
    ng_exec_fixture_write16(data, 0x5A0Eu, 0x40F9u); /* MOVE SR,$00001200 */
    ng_exec_fixture_write32(data, 0x5A10u, 0x00001200u);
    ng_exec_fixture_write16(data, 0x5A14u, 0x23C0u); /* MOVE.L D0,$00001202 */
    ng_exec_fixture_write32(data, 0x5A16u, 0x00001202u);

    ng_exec_fixture_write16(data, 0x5A1Au, 0x223Cu); /* MOVE.L #$00010000,D1 */
    ng_exec_fixture_write32(data, 0x5A1Cu, 0x00010000u);
    ng_exec_fixture_write16(data, 0x5A20u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0x5A22u, 0x001Fu);
    ng_exec_fixture_write16(data, 0x5A24u, 0x82FCu); /* DIVU.W #$0002,D1 */
    ng_exec_fixture_write16(data, 0x5A26u, 0x0002u);
    ng_exec_fixture_write16(data, 0x5A28u, 0x40F9u); /* MOVE SR,$00001206 */
    ng_exec_fixture_write32(data, 0x5A2Au, 0x00001206u);
    ng_exec_fixture_write16(data, 0x5A2Eu, 0x23C1u); /* MOVE.L D1,$00001208 */
    ng_exec_fixture_write32(data, 0x5A30u, 0x00001208u);

    ng_exec_fixture_write16(data, 0x5A34u, 0x243Cu); /* MOVE.L #$FFFFFFF9,D2 */
    ng_exec_fixture_write32(data, 0x5A36u, 0xFFFFFFF9u);
    ng_exec_fixture_write16(data, 0x5A3Au, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0x5A3Cu, 0x001Fu);
    ng_exec_fixture_write16(data, 0x5A3Eu, 0x85FCu); /* DIVS.W #$0003,D2 */
    ng_exec_fixture_write16(data, 0x5A40u, 0x0003u);
    ng_exec_fixture_write16(data, 0x5A42u, 0x40F9u); /* MOVE SR,$0000120C */
    ng_exec_fixture_write32(data, 0x5A44u, 0x0000120Cu);
    ng_exec_fixture_write16(data, 0x5A48u, 0x23C2u); /* MOVE.L D2,$0000120E */
    ng_exec_fixture_write32(data, 0x5A4Au, 0x0000120Eu);

    ng_exec_fixture_write16(data, 0x5A4Eu, 0x263Cu); /* MOVE.L #$00000001,D3 */
    ng_exec_fixture_write32(data, 0x5A50u, 0x00000001u);
    ng_exec_fixture_write16(data, 0x5A54u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0x5A56u, 0x001Fu);
    ng_exec_fixture_write16(data, 0x5A58u, 0x87FCu); /* DIVS.W #$FFFE,D3 */
    ng_exec_fixture_write16(data, 0x5A5Au, 0xFFFEu);
    ng_exec_fixture_write16(data, 0x5A5Cu, 0x40F9u); /* MOVE SR,$00001212 */
    ng_exec_fixture_write32(data, 0x5A5Eu, 0x00001212u);
    ng_exec_fixture_write16(data, 0x5A62u, 0x23C3u); /* MOVE.L D3,$00001214 */
    ng_exec_fixture_write32(data, 0x5A64u, 0x00001214u);
    ng_exec_fixture_write16(data, 0x5A68u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x5A6Au, 0x2700u);

    ng_exec_fixture_write16(data, 0x5B00u, 0x303Cu); /* MOVE.W #$FFFF,D0 */
    ng_exec_fixture_write16(data, 0x5B02u, 0xFFFFu);
    ng_exec_fixture_write16(data, 0x5B04u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0x5B06u, 0x001Fu);
    ng_exec_fixture_write16(data, 0x5B08u, 0xC0FCu); /* MULU.W #$FFFF,D0 */
    ng_exec_fixture_write16(data, 0x5B0Au, 0xFFFFu);
    ng_exec_fixture_write16(data, 0x5B0Cu, 0x40F9u); /* MOVE SR,$00001218 */
    ng_exec_fixture_write32(data, 0x5B0Eu, 0x00001218u);
    ng_exec_fixture_write16(data, 0x5B12u, 0x23C0u); /* MOVE.L D0,$0000121A */
    ng_exec_fixture_write32(data, 0x5B14u, 0x0000121Au);

    ng_exec_fixture_write16(data, 0x5B18u, 0x323Cu); /* MOVE.W #$0002,D1 */
    ng_exec_fixture_write16(data, 0x5B1Au, 0x0002u);
    ng_exec_fixture_write16(data, 0x5B1Cu, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0x5B1Eu, 0x001Fu);
    ng_exec_fixture_write16(data, 0x5B20u, 0xC3FCu); /* MULS.W #$FFFF,D1 */
    ng_exec_fixture_write16(data, 0x5B22u, 0xFFFFu);
    ng_exec_fixture_write16(data, 0x5B24u, 0x40F9u); /* MOVE SR,$0000121E */
    ng_exec_fixture_write32(data, 0x5B26u, 0x0000121Eu);
    ng_exec_fixture_write16(data, 0x5B2Au, 0x23C1u); /* MOVE.L D1,$00001220 */
    ng_exec_fixture_write32(data, 0x5B2Cu, 0x00001220u);

    ng_exec_fixture_write16(data, 0x5B30u, 0x343Cu); /* MOVE.W #$0000,D2 */
    ng_exec_fixture_write16(data, 0x5B32u, 0x0000u);
    ng_exec_fixture_write16(data, 0x5B34u, 0x44FCu); /* MOVE #$001F,CCR */
    ng_exec_fixture_write16(data, 0x5B36u, 0x001Fu);
    ng_exec_fixture_write16(data, 0x5B38u, 0xC5FCu); /* MULS.W #$1234,D2 */
    ng_exec_fixture_write16(data, 0x5B3Au, 0x1234u);
    ng_exec_fixture_write16(data, 0x5B3Cu, 0x40F9u); /* MOVE SR,$00001224 */
    ng_exec_fixture_write32(data, 0x5B3Eu, 0x00001224u);
    ng_exec_fixture_write16(data, 0x5B42u, 0x23C2u); /* MOVE.L D2,$00001226 */
    ng_exec_fixture_write32(data, 0x5B44u, 0x00001226u);
    ng_exec_fixture_write16(data, 0x5B48u, 0x4E72u); /* STOP #$2700 */
    ng_exec_fixture_write16(data, 0x5B4Au, 0x2700u);

    {
        uint32_t pc = 0x5C00u;
        ng_exec_fixture_write16(data, pc, 0x203Cu); /* MOVE.L #$11112222,D0 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x11112222u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x227Cu); /* MOVEA.L #$33334444,A1 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x33334444u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x287Cu); /* MOVEA.L #$00001300,A4 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001300u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #$001F,CCR */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x001Fu);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x48ECu); /* MOVEM.L D0/A1,($10,A4) */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x0201u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x0010u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$00001230 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001230u);
        pc += 4u;

        ng_exec_fixture_write16(data, pc, 0x4CECu); /* MOVEM.L ($10,A4),D2/A3 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x0804u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x0010u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$00001232 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001232u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x23C2u); /* MOVE.L D2,$00001234 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001234u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x23CBu); /* MOVE.L A3,$00001238 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001238u);
        pc += 4u;

        ng_exec_fixture_write16(data, pc, 0x2A3Cu); /* MOVE.L #$55556666,D5 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x55556666u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x2C7Cu); /* MOVEA.L #$77778888,A6 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x77778888u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #$001F,CCR */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x001Fu);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x48F8u); /* MOVEM.L D5/A6,$1330.W */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x4020u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x1330u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$0000123C */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x0000123Cu);
        pc += 4u;

        ng_exec_fixture_write16(data, pc, 0x33FCu); /* MOVE.W #$8001,$00001340 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x8001u);
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001340u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x33FCu); /* MOVE.W #$7FFE,$00001342 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x7FFEu);
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001342u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #$001F,CCR */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x001Fu);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x4CB9u); /* MOVEM.W $00001340,D3/A4 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x1008u);
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001340u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$0000123E */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x0000123Eu);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x23C3u); /* MOVE.L D3,$00001240 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001240u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x23CCu); /* MOVE.L A4,$00001244 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001244u);
        pc += 4u;

        ng_exec_fixture_write16(data, pc, 0x33FCu); /* MOVE.W #$FFFE,$00001350 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0xFFFEu);
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001350u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x33FCu); /* MOVE.W #$0002,$00001352 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x0002u);
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001352u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #$001F,CCR */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x001Fu);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x4CBAu); /* MOVEM.W ($1350,PC),D4/A5 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x2010u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, (uint16_t)((int32_t)0x00001350 - (int32_t)pc));
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$00001248 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001248u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x23C4u); /* MOVE.L D4,$0000124A */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x0000124Au);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x23CDu); /* MOVE.L A5,$0000124E */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x0000124Eu);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x4E72u); /* STOP #$2700 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x2700u);
    }

    {
        uint32_t pc = 0x5D00u;
        ng_exec_fixture_write16(data, pc, 0x103Cu); /* MOVE.B #$40,D0 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x0040u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #$001F,CCR */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x001Fu);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0xE300u); /* ASL.B #1,D0 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$00001260 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001260u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x13C0u); /* MOVE.B D0,$00001262 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001262u);
        pc += 4u;

        ng_exec_fixture_write16(data, pc, 0x323Cu); /* MOVE.W #$8001,D1 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x8001u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #$001F,CCR */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x001Fu);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0xE241u); /* ASR.W #1,D1 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$00001264 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001264u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x33C1u); /* MOVE.W D1,$00001266 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001266u);
        pc += 4u;

        ng_exec_fixture_write16(data, pc, 0x243Cu); /* MOVE.L #$0000000F,D2 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x0000000Fu);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #$001F,CCR */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x001Fu);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0xE88Au); /* LSR.L #4,D2 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$00001268 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001268u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x23C2u); /* MOVE.L D2,$0000126A */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x0000126Au);
        pc += 4u;

        ng_exec_fixture_write16(data, pc, 0x163Cu); /* MOVE.B #$02,D3 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x0002u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #$001F,CCR */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x001Fu);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0xE213u); /* ROXR.B #1,D3 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$0000126E */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x0000126Eu);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x13C3u); /* MOVE.B D3,$00001270 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001270u);
        pc += 4u;

        ng_exec_fixture_write16(data, pc, 0x383Cu); /* MOVE.W #$0001,D4 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x0001u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #$001F,CCR */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x001Fu);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0xE25Cu); /* ROR.W #1,D4 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$00001272 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001272u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x33C4u); /* MOVE.W D4,$00001274 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001274u);
        pc += 4u;

        ng_exec_fixture_write16(data, pc, 0x33FCu); /* MOVE.W #$8000,$00001360 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x8000u);
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001360u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #$001F,CCR */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x001Fu);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0xE5F9u); /* ROXL.W $00001360 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001360u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$00001276 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001276u);
        pc += 4u;

        ng_exec_fixture_write16(data, pc, 0x33FCu); /* MOVE.W #$4000,$00001362 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x4000u);
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001362u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #$001F,CCR */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x001Fu);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0xE1F9u); /* ASL.W $00001362 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001362u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$00001278 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001278u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x4E72u); /* STOP #$2700 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x2700u);
    }

    {
        uint32_t pc = 0x5E00u;
        ng_exec_fixture_write16(data, pc, 0x203Cu); /* MOVE.L #$11112222,D0 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x11112222u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x263Cu); /* MOVE.L #$33334444,D3 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x33334444u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x227Cu); /* MOVEA.L #$55556666,A1 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x55556666u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x2C7Cu); /* MOVEA.L #$77778888,A6 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x77778888u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x2A7Cu); /* MOVEA.L #$00001420,A5 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001420u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x2C3Cu); /* MOVE.L #$0000FFF0,D6 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x0000FFF0u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #$001F,CCR */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x001Fu);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x48F5u); /* MOVEM.L D0/D3/A1/A6,(8,A5,D6.W) */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x4209u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x6008u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x4CF5u); /* MOVEM.L (8,A5,D6.W),D1/D4/A2/A5 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x2412u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x6008u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$0000127A */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x0000127Au);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x4E72u); /* STOP #$2700 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x2700u);
    }

    {
        uint32_t pc = 0x5E80u;
        ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #$0010,CCR */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x0010u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x7080u); /* MOVEQ #-128,D0 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x0C00u); /* CMPI.B #$01,D0 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x0001u);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$0000127E */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x0000127Eu);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x23C0u); /* MOVE.L D0,$00001280 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001280u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x4E72u); /* STOP #$2700 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x2700u);
    }

    {
        uint32_t pc = 0x5EA0u;
        ng_exec_fixture_write16(data, pc, 0x2E3Cu); /* MOVE.L #$A5A55A5A,D7 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0xA5A55A5Au);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x267Cu); /* MOVEA.L #$00001460,A3 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001460u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #$001F,CCR */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x001Fu);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x4E71u); /* NOP */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$00001288 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001288u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x4E72u); /* STOP #$2700 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x2700u);
    }

    {
        uint32_t pc = 0x5EC0u;
        ng_exec_fixture_write16(data, pc, 0x4E70u); /* RESET */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x4E72u); /* STOP #$2700 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x2700u);
    }

    {
        uint32_t pc = 0x1200u;
        ng_exec_fixture_write16(data, pc, 0x33FCu); /* MOVE.W #$8001,$0000121E */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x8001u);
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x0000121Eu);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x7000u); /* MOVEQ #0,D0 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x44FCu); /* MOVE #$001F,CCR */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x001Fu);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x327Bu); /* MOVEA.W ($0E,PC,D0.W),A1 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x000Eu);
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$00001284 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001284u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x4E72u); /* STOP #$2700 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x2700u);
    }

    {
        uint32_t pc = 0x5ED0u;
        ng_exec_fixture_write16(data, pc, 0x4E77u); /* RTR */
    }

    {
        uint32_t pc = 0x5EE0u;
        ng_exec_fixture_write16(data, pc, 0x40F9u); /* MOVE SR,$00001286 */
        pc += 2u;
        ng_exec_fixture_write32(data, pc, 0x00001286u);
        pc += 4u;
        ng_exec_fixture_write16(data, pc, 0x4E40u); /* TRAP #0 */
    }

    {
        uint32_t pc = 0x5EF0u;
        ng_exec_fixture_write16(data, pc, 0x4E72u); /* STOP #$2700 */
        pc += 2u;
        ng_exec_fixture_write16(data, pc, 0x2700u);
    }
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
        0x000004A0u,
        0x000004B0u,
        0x000004C0u,
        0x000004D0u,
        0x000004E0u,
        0x000004F0u,
        0x00000500u,
        0x00000510u,
        0x00000520u,
        0x00000530u,
        0x00000540u,
        0x00000550u,
        0x00000560u,
        0x00000580u,
        0x00000590u,
        0x000005A0u,
        0x000005C0u,
        0x000005D0u,
        0x000005E0u,
        0x000005F0u,
        0x00000600u,
        0x00000610u,
        0x00000620u,
        0x00000630u,
        0x00000640u,
        0x00000650u,
        0x00000660u,
        0x00000670u,
        0x00000680u,
        0x00000690u,
        0x000006A0u,
        0x000006B0u,
        0x000006C0u,
        0x00000700u,
        0x00000720u,
        0x00000740u,
        0x00000760u,
        0x00000780u,
        0x000007C0u,
        0x00000820u,
        0x00000890u,
        0x000008D0u,
        0x00000900u,
        0x00000940u,
        0x00000970u,
        0x000009A0u,
        0x000009D0u,
        0x00000A00u,
        0x00000A20u,
        0x00000A40u,
        0x00000A80u,
        0x00000AC0u,
        0x00000B00u,
        0x00000B18u,
        0x00000B40u,
        0x00000CF0u,
        0x00000EA0u,
        0x00001050u,
        0x00001200u,
        0x00001220u,
        0x00001500u,
        0x000017E0u,
        0x00001AC0u,
        0x00001DA0u,
        0x00002080u,
        0x00002360u,
        0x00002640u,
        0x00002A00u,
        0x00002C00u,
        0x00002E00u,
        0x00003000u,
        0x00003200u,
        0x00003400u,
        0x00003600u,
        0x00003800u,
        0x00003A00u,
        0x00003C00u,
        0x00003E00u,
        0x00004000u,
        0x00004200u,
        0x00004400u,
        0x00004600u,
        0x00004800u,
        0x00004A00u,
        0x00004C00u,
        0x00004E00u,
        0x00005000u,
        0x00005200u,
        0x00005400u,
        0x00005600u,
        0x00005800u,
        0x00005A00u,
        0x00005B00u,
        0x00005C00u,
        0x00005D00u,
        0x00005E00u,
        0x00005E80u,
        0x00005EA0u,
        0x00005EC0u,
        0x00005ED0u,
        0x00005EE0u,
        0x00005EF0u,
    };
    return index < NG_EXEC_FIXTURE_ADDR_COUNT ? addrs[index] : 0;
}

#undef NG_EXEC_FIXTURE_MAYBE_UNUSED
