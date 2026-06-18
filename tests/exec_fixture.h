#pragma once

#include <stdint.h>
#include <string.h>

#define NG_EXEC_FIXTURE_SIZE 0x1A6u
#define NG_EXEC_FIXTURE_ADDR_COUNT 3u

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
    ng_exec_fixture_write16(data, 0x62u, 0x0004u);
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
    ng_exec_fixture_write16(data, 0x88u, 0x0016u);
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
    ng_exec_fixture_write16(data, 0x18Eu, 0x7000u); /* MOVEQ #0,D0 */
    ng_exec_fixture_write16(data, 0x190u, 0x7200u); /* MOVEQ #0,D1 */
    ng_exec_fixture_write16(data, 0x192u, 0x5300u); /* SUBQ.B #1,D0, sets X */
    ng_exec_fixture_write16(data, 0x194u, 0xD101u); /* ADDX.B D1,D0 */
    ng_exec_fixture_write16(data, 0x196u, 0x13C0u); /* MOVE.B D0,$00101C */
    ng_exec_fixture_write32(data, 0x198u, 0x0000101Cu);
    ng_exec_fixture_write16(data, 0x19Cu, 0x13FCu); /* MOVE.B #$80,$00100E */
    ng_exec_fixture_write16(data, 0x19Eu, 0x0080u);
    ng_exec_fixture_write32(data, 0x1A0u, 0x0000100Eu);
    ng_exec_fixture_write16(data, 0x1A4u, 0x4E75u); /* RTS */
}

static uint32_t ng_exec_fixture_addr(uint32_t index) {
    static const uint32_t addrs[NG_EXEC_FIXTURE_ADDR_COUNT] = {
        0x00000000u,
        0x00000020u,
        0x00000060u,
    };
    return index < NG_EXEC_FIXTURE_ADDR_COUNT ? addrs[index] : 0;
}
