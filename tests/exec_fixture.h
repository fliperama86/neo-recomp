#pragma once

#include <stdint.h>
#include <string.h>

#define NG_EXEC_FIXTURE_SIZE 0x80u
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
    ng_exec_fixture_write16(data, 0x76u, 0x13FCu); /* MOVE.B #$80,$00100E */
    ng_exec_fixture_write16(data, 0x78u, 0x0080u);
    ng_exec_fixture_write32(data, 0x7Au, 0x0000100Eu);
    ng_exec_fixture_write16(data, 0x7Eu, 0x4E75u); /* RTS */
}

static uint32_t ng_exec_fixture_addr(uint32_t index) {
    static const uint32_t addrs[NG_EXEC_FIXTURE_ADDR_COUNT] = {
        0x00000000u,
        0x00000020u,
        0x00000060u,
    };
    return index < NG_EXEC_FIXTURE_ADDR_COUNT ? addrs[index] : 0;
}
