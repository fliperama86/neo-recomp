#pragma once

#include <stdint.h>
#include <string.h>

#define NG_EXEC_FIXTURE_SIZE 0x40u
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
    ng_exec_fixture_write16(data, 0x0Au, 0x4EF9u); /* JMP $00000030 */
    ng_exec_fixture_write32(data, 0x0Cu, 0x00000030u);

    ng_exec_fixture_write16(data, 0x20u, 0x33FCu); /* MOVE.W #$1234,$001000 */
    ng_exec_fixture_write16(data, 0x22u, 0x1234u);
    ng_exec_fixture_write32(data, 0x24u, 0x00001000u);
    ng_exec_fixture_write16(data, 0x28u, 0x4E75u); /* RTS */

    ng_exec_fixture_write16(data, 0x30u, 0x41FAu); /* LEA $000038,A0 */
    ng_exec_fixture_write16(data, 0x32u, 0x0004u);
    ng_exec_fixture_write16(data, 0x34u, 0x23C8u); /* MOVE.L A0,$001004 */
    ng_exec_fixture_write32(data, 0x36u, 0x00001004u);
    ng_exec_fixture_write16(data, 0x3Au, 0x4E75u); /* RTS */
}

static uint32_t ng_exec_fixture_addr(uint32_t index) {
    static const uint32_t addrs[NG_EXEC_FIXTURE_ADDR_COUNT] = {
        0x00000000u,
        0x00000020u,
        0x00000030u,
    };
    return index < NG_EXEC_FIXTURE_ADDR_COUNT ? addrs[index] : 0;
}
