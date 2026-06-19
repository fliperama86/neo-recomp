#include "m68k_decode.h"
#include "m68k_validate.h"

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
    NgM68kInstr instr;

    {
        NgProgramRom rom = make_rom(0x08u);
        CHECK(rom.data != NULL);
        write16(&rom, 0x00u, 0x4EF9u); /* JMP $000004 */
        write32(&rom, 0x02u, 0x00000004u);
        CHECK(ng_m68k_decode(&rom, 0x00u, &instr));
        CHECK(ng_m68k_validate(&instr));
        ng_program_rom_free(&rom);
    }

    {
        NgProgramRom rom = make_rom(0x04u);
        CHECK(rom.data != NULL);
        write16(&rom, 0x00u, 0x15C0u); /* unknown/unsupported form */
        CHECK(ng_m68k_decode(&rom, 0x00u, &instr));
        CHECK(!ng_m68k_validate(&instr));
        ng_program_rom_free(&rom);
    }

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_JMP;
    instr.byte_length = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_JMP;
    instr.byte_length = 2u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.src.reg = 0u;
    instr.form = NG_M68K_FORM_AREG_INDIRECT;
    instr.reg = 0u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_JMP;
    instr.byte_length = 2u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_JMP;
    instr.byte_length = 4u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.src.reg = 0u;
    instr.form = NG_M68K_FORM_AREG_INDIRECT;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_JMP;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.src.reg = 0u;
    instr.form = NG_M68K_FORM_AREG_INDIRECT;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_JMP;
    instr.byte_length = 2u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.src.reg = 0u;
    instr.form = NG_M68K_FORM_ABS;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_JSR;
    instr.byte_length = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 4u;
    instr.form = NG_M68K_FORM_PC_RELATIVE;
    instr.target = 4u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_JSR;
    instr.byte_length = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 4u;
    instr.form = NG_M68K_FORM_PC_RELATIVE;
    instr.target = 4u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_JSR;
    instr.byte_length = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 6u;
    instr.form = NG_M68K_FORM_PC_RELATIVE;
    instr.target = 6u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_JSR;
    instr.byte_length = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_JSR;
    instr.byte_length = 6u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.absolute_addr = 4u;
    instr.form = NG_M68K_FORM_PC_RELATIVE;
    instr.target = 4u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_JSR;
    instr.byte_length = 6u;
    instr.src.mode = NG_M68K_EA_ABS_L;
    instr.src.reg = 1u;
    instr.src.absolute_addr = 0x123456u;
    instr.form = NG_M68K_FORM_ABS;
    instr.target = 0x123456u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_JSR;
    instr.byte_length = 6u;
    instr.src.mode = NG_M68K_EA_ABS_L;
    instr.src.reg = 1u;
    instr.src.absolute_addr = 0x123456u;
    instr.form = NG_M68K_FORM_ABS;
    instr.target = 0x654321u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_PEA;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_ADISP;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_PEA;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_AINDEX;
    instr.src.reg = 1u;
    instr.src.index_reg = 2u;
    instr.src.displacement = 256;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_PEA;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_ADISP;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_PEA;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_ADISP;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_PEA;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_ADISP;
    instr.src.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_PEA;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_ADISP;
    instr.src.reg = 0u;
    instr.src_reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_SR;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_SR;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_SR;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_CCR;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.src.immediate = 0x001Bu;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_CCR;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_SR;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 4u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_SR;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.src.immediate = 0x2700u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_IMM;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    instr.src_reg = 1u;
    instr.reg = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE;
    instr.byte_length = 8u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.src.reg = 4u;
    instr.src.immediate = 0x12345678u;
    instr.dst.mode = NG_M68K_EA_AINDEX;
    instr.dst.reg = 0u;
    instr.dst.index_is_addr = 1u;
    instr.dst.index_reg = 2u;
    instr.dst.index_is_long = 1u;
    instr.dst.displacement = 0x0C;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE;
    instr.byte_length = 6u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.src.reg = 4u;
    instr.src.immediate = 0x12345678u;
    instr.dst.mode = NG_M68K_EA_AINDEX;
    instr.dst.reg = 0u;
    instr.dst.index_is_addr = 1u;
    instr.dst.index_reg = 2u;
    instr.dst.index_is_long = 1u;
    instr.dst.displacement = 0x0C;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.mnemonic = NG_M68K_MOVE;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 0x104u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 1u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.mnemonic = NG_M68K_MOVE;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 0x108u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_AINDEX;
    instr.dst.reg = 0u;
    instr.dst.index_reg = 2u;
    instr.dst.displacement = 256;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.opcode = 0x6B08u;
    instr.mnemonic = NG_M68K_BCC;
    instr.byte_length = 2u;
    instr.condition = 16u;
    instr.displacement = 8;
    instr.target = 0x10Au;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.opcode = 0x60FEu;
    instr.mnemonic = NG_M68K_BRA;
    instr.byte_length = 2u;
    instr.displacement = -2;
    instr.target = 0x100u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.opcode = 0x60FEu;
    instr.mnemonic = NG_M68K_BRA;
    instr.byte_length = 2u;
    instr.displacement = -2;
    instr.target = 0x102u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.opcode = 0x60FFu;
    instr.mnemonic = NG_M68K_BRA;
    instr.byte_length = 2u;
    instr.displacement = -1;
    instr.target = 0x101u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.opcode = 0x6000u;
    instr.mnemonic = NG_M68K_BRA;
    instr.byte_length = 6u;
    instr.target = 0x102u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x852u;
    instr.opcode = 0x6100u;
    instr.mnemonic = NG_M68K_BSR;
    instr.byte_length = 4u;
    instr.displacement = 14;
    instr.target = 0x862u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x852u;
    instr.opcode = 0x6100u;
    instr.mnemonic = NG_M68K_BSR;
    instr.byte_length = 4u;
    instr.displacement = 14;
    instr.target = 0x862u;
    instr.src.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.opcode = 0x6B08u;
    instr.mnemonic = NG_M68K_BCC;
    instr.byte_length = 2u;
    instr.condition = 11u;
    instr.displacement = 8;
    instr.target = 0x10Au;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.opcode = 0x6F00u;
    instr.mnemonic = NG_M68K_BCC;
    instr.byte_length = 4u;
    instr.condition = 15u;
    instr.displacement = -4;
    instr.target = 0xFEu;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.opcode = 0x6108u;
    instr.mnemonic = NG_M68K_BCC;
    instr.byte_length = 2u;
    instr.condition = 1u;
    instr.displacement = 8;
    instr.target = 0x10Au;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.opcode = 0x6B00u;
    instr.mnemonic = NG_M68K_BCC;
    instr.byte_length = 2u;
    instr.condition = 11u;
    instr.displacement = 0;
    instr.target = 0x102u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.opcode = 0x6B08u;
    instr.mnemonic = NG_M68K_BCC;
    instr.byte_length = 2u;
    instr.condition = 11u;
    instr.form = NG_M68K_FORM_PC_RELATIVE;
    instr.displacement = 8;
    instr.target = 0x10Au;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEQ;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_IMM_TO_DREG;
    instr.reg = 0u;
    instr.immediate = 0xFFFFFF80u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEQ;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.reg = 0u;
    instr.immediate = 0xFFFFFF80u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEQ;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.form = NG_M68K_FORM_IMM_TO_DREG;
    instr.reg = 0u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEQ;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_IMM_TO_DREG;
    instr.reg = 0u;
    instr.immediate = 0x80u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEQ;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_IMM_TO_DREG;
    instr.reg = 0u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEQ;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_IMM_TO_DREG;
    instr.reg = 0u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 0u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUBQ;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.immediate = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUBQ;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.immediate = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.immediate = 2u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 8u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TST;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_IMM;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TST;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TST;
    instr.byte_length = 0u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TST;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TST;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TST;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPI;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_PC_DISP;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPI;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x7Fu;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPI;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.immediate = 0x7Fu;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPI;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x100u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPI;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x7Fu;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BTST;
    instr.byte_length = 6u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_PC_DISP;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BTST;
    instr.byte_length = 6u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_IMM;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BTST;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_IMM;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CHK;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CHK;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CHK;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CHK;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CHK;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 8u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CHK;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MULU;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MULU;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_DIVS;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 8u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_DIVS;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXT;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 3u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXT;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 3u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXT;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.reg = 3u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXT;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 3u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXT;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_DREG;
    instr.immediate = 1u;
    instr.reg = 3u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXT;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 3u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXT;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SWAP;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SWAP;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SWAP;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.reg = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SWAP;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SWAP;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDI;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CLR;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    instr.reg = 0u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CLR;
    instr.byte_length = 2u;
    instr.size = 0u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NEG;
    instr.byte_length = 2u;
    instr.size = 8u;
    instr.dst.mode = NG_M68K_EA_AIND;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TAS;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.form = NG_M68K_FORM_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TAS;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NBCD;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_APRE;
    instr.dst.reg = 7u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NBCD;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NEG;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NEG;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NEG;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NOT;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_APOST;
    instr.dst.reg = 0u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NOT;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_APOST;
    instr.dst.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NEGX;
    instr.byte_length = 6u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_L;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NEGX;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_L;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NBCD;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.form = NG_M68K_FORM_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TAS;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.immediate = 1u;
    instr.form = NG_M68K_FORM_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ORI;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_PC_DISP;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EORI;
    instr.byte_length = 6u;
    instr.size = 4u;
    instr.immediate = 0x12345678u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDI;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.immediate = 0x7Fu;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUBI;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x100u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ANDI;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x7Fu;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EORI;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x7Fu;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADD;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.dst.mode = NG_M68K_EA_AIND;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADD;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADD;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADD;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADD;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMP;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 3u;
    instr.src_reg = 3u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 4u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADD;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADD;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 0u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUB;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_AIND;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUB;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_AIND;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUB;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_AIND;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMP;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_AIND;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EOR;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EOR;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_OR;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 7u;
    instr.src_reg = 7u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_AND;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 7u;
    instr.src_reg = 7u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_AND;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_PC_DISP;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BCLR;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_PC_DISP;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BSET;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BSET;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_APOST;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BSET;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_APOST;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BCLR;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 7u;
    instr.dst.mode = NG_M68K_EA_AIND;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BCLR;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 7u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BCLR;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.immediate = 0x100u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BCHG;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BSET;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 8u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BCLR;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 7u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_AIND;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDA;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUBA;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPA;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPA;
    instr.byte_length = 0u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPA;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 8u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDA;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPA;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEA;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEA;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEA;
    instr.byte_length = 0u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEA;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 8u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEA;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LEA;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 4u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LEA;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LEA;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LEA;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LEA;
    instr.byte_length = 6u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LEA;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 2u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LEA;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 8u;
    instr.reg = 8u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXG;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 0u;
    instr.src_reg = 0u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 1u;
    instr.reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXG;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 0u;
    instr.src_reg = 0u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 1u;
    instr.reg = 1u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXG;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 0u;
    instr.src_reg = 0u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 1u;
    instr.reg = 1u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXG;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 0u;
    instr.src_reg = 0u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 1u;
    instr.reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXG;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 2u;
    instr.src_reg = 2u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXG;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 2u;
    instr.src_reg = 2u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXG;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 2u;
    instr.src_reg = 2u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXG;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 2u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXG;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 2u;
    instr.src_reg = 2u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 3u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXG;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 8u;
    instr.src_reg = 8u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXG;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 2u;
    instr.src_reg = 2u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 3u;
    instr.dst.displacement = 2;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LINK;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.reg = 8u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LINK;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.reg = 7u;
    instr.displacement = -4;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LINK;
    instr.byte_length = 4u;
    instr.reg = 7u;
    instr.displacement = -4;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LINK;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LINK;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.reg = 7u;
    instr.immediate = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LINK;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.reg = 7u;
    instr.src.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LINK;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.reg = 7u;
    instr.src.reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LINK;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.reg = 7u;
    instr.form = NG_M68K_FORM_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LINK;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.reg = 7u;
    instr.target = 0x100u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_UNLK;
    instr.byte_length = 2u;
    instr.reg = 7u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_UNLK;
    instr.byte_length = 4u;
    instr.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_UNLK;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_UNLK;
    instr.byte_length = 2u;
    instr.reg = 7u;
    instr.displacement = -4;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_UNLK;
    instr.byte_length = 2u;
    instr.reg = 7u;
    instr.immediate = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_UNLK;
    instr.byte_length = 2u;
    instr.reg = 7u;
    instr.src_reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEP;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 2u;
    instr.dst.displacement = 0x10;
    instr.reg = 1u;
    instr.displacement = 0x10;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEP;
    instr.byte_length = 6u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 2u;
    instr.dst.displacement = 0x10;
    instr.reg = 1u;
    instr.displacement = 0x10;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEP;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 0u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 2u;
    instr.dst.displacement = 0x10;
    instr.reg = 1u;
    instr.displacement = 0x10;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEP;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 2u;
    instr.dst.displacement = 0x10;
    instr.reg = 0u;
    instr.displacement = 0x10;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEP;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.immediate = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 2u;
    instr.dst.displacement = 0x10;
    instr.reg = 1u;
    instr.displacement = 0x10;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEP;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_ADISP;
    instr.src.reg = 2u;
    instr.src.displacement = -4;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 1u;
    instr.reg = 1u;
    instr.displacement = -4;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEP;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_ADISP;
    instr.src.reg = 2u;
    instr.src.displacement = -4;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 1u;
    instr.reg = 0u;
    instr.displacement = -4;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEP;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_ADISP;
    instr.src.reg = 2u;
    instr.src.displacement = -4;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 1u;
    instr.reg = 1u;
    instr.displacement = 0;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEP;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPM;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_APOST;
    instr.src.reg = 5u;
    instr.src_reg = 5u;
    instr.dst.mode = NG_M68K_EA_APOST;
    instr.dst.reg = 6u;
    instr.reg = 6u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPM;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.dst.mode = NG_M68K_EA_APOST;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPM;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_APOST;
    instr.src.reg = 5u;
    instr.src_reg = 5u;
    instr.dst.mode = NG_M68K_EA_APOST;
    instr.dst.reg = 6u;
    instr.reg = 6u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPM;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_APOST;
    instr.src.reg = 5u;
    instr.src_reg = 4u;
    instr.dst.mode = NG_M68K_EA_APOST;
    instr.dst.reg = 6u;
    instr.reg = 6u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPM;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_APOST;
    instr.src.reg = 5u;
    instr.src_reg = 5u;
    instr.dst.mode = NG_M68K_EA_APOST;
    instr.dst.reg = 6u;
    instr.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPM;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_APOST;
    instr.src.reg = 8u;
    instr.src_reg = 8u;
    instr.dst.mode = NG_M68K_EA_APOST;
    instr.dst.reg = 6u;
    instr.reg = 6u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPM;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 1u;
    instr.src.mode = NG_M68K_EA_APOST;
    instr.src.reg = 5u;
    instr.src_reg = 5u;
    instr.dst.mode = NG_M68K_EA_APOST;
    instr.dst.reg = 6u;
    instr.reg = 6u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDX;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_APRE;
    instr.src.reg = 5u;
    instr.src_reg = 5u;
    instr.dst.mode = NG_M68K_EA_APRE;
    instr.dst.reg = 6u;
    instr.reg = 6u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUBX;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_APRE;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDX;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_APRE;
    instr.src.reg = 5u;
    instr.src_reg = 5u;
    instr.dst.mode = NG_M68K_EA_APRE;
    instr.dst.reg = 6u;
    instr.reg = 6u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUBX;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 0u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUBX;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUBX;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.immediate = 1u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ABCD;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SBCD;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_APRE;
    instr.dst.mode = NG_M68K_EA_APRE;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SBCD;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_APRE;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_APRE;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ABCD;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_APRE;
    instr.src.reg = 8u;
    instr.src_reg = 8u;
    instr.dst.mode = NG_M68K_EA_APRE;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_APOST;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.immediate = 0x8000u;
    instr.dst.mode = NG_M68K_EA_APRE;
    instr.dst.reg = 7u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.byte_length = 6u;
    instr.size = 2u;
    instr.immediate = 0x0003u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 4u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x0003u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 4u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.immediate = 0x10000u;
    instr.dst.mode = NG_M68K_EA_APRE;
    instr.dst.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.immediate = 0x8000u;
    instr.dst.mode = NG_M68K_EA_APRE;
    instr.dst.reg = 7u;
    instr.reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.byte_length = 6u;
    instr.size = 4u;
    instr.immediate = 0x0003u;
    instr.dst.mode = NG_M68K_EA_AINDEX;
    instr.dst.reg = 2u;
    instr.dst.index_reg = 8u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x0003u;
    instr.src.mode = NG_M68K_EA_APOST;
    instr.src.reg = 4u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.byte_length = 6u;
    instr.size = 2u;
    instr.immediate = 0x0003u;
    instr.src.mode = NG_M68K_EA_APOST;
    instr.src.reg = 4u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.immediate = 0x0003u;
    instr.src.mode = NG_M68K_EA_APRE;
    instr.src.reg = 4u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LSL;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LSL;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LSL;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 4u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LSR;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LSR;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    instr.reg = 7u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LSR;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 0u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    instr.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ROR;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_W;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ROR;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_W;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ROXR;
    instr.byte_length = 6u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_L;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ROXR;
    instr.byte_length = 6u;
    instr.size = 2u;
    instr.immediate = 2u;
    instr.dst.mode = NG_M68K_EA_ABS_L;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ROXL;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_L;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.condition = 5u;
    instr.dst.mode = NG_M68K_EA_PC_DISP;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.condition = 5u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.condition = 5u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 2u;
    instr.dst.displacement = -4;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.byte_length = 6u;
    instr.size = 1u;
    instr.condition = 5u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.condition = 5u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 2u;
    instr.dst.displacement = -4;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.condition = 5u;
    instr.target = 0x100u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.condition = 5u;
    instr.form = NG_M68K_FORM_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.byte_length = 0u;
    instr.size = 1u;
    instr.condition = 5u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.condition = 5u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.condition = 5u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.condition = 5u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x110u;
    instr.mnemonic = NG_M68K_DBCC;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.condition = 15u;
    instr.reg = 7u;
    instr.displacement = -18;
    instr.target = 0x100u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x110u;
    instr.mnemonic = NG_M68K_DBCC;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.condition = 15u;
    instr.reg = 8u;
    instr.displacement = -18;
    instr.target = 0x100u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x110u;
    instr.mnemonic = NG_M68K_DBCC;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.condition = 15u;
    instr.reg = 7u;
    instr.displacement = -18;
    instr.target = 0x100u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x110u;
    instr.mnemonic = NG_M68K_DBCC;
    instr.byte_length = 4u;
    instr.condition = 15u;
    instr.reg = 7u;
    instr.displacement = -18;
    instr.target = 0x100u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x110u;
    instr.mnemonic = NG_M68K_DBCC;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.condition = 15u;
    instr.reg = 7u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.displacement = -18;
    instr.target = 0x100u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x110u;
    instr.mnemonic = NG_M68K_DBCC;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.condition = 15u;
    instr.reg = 7u;
    instr.immediate = 1u;
    instr.displacement = -18;
    instr.target = 0x100u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x110u;
    instr.mnemonic = NG_M68K_DBCC;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.condition = 15u;
    instr.reg = 7u;
    instr.displacement = -18;
    instr.target = 0x102u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x110u;
    instr.mnemonic = NG_M68K_DBCC;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.condition = 15u;
    instr.reg = 7u;
    instr.form = NG_M68K_FORM_DREG;
    instr.displacement = -18;
    instr.target = 0x100u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x110u;
    instr.mnemonic = NG_M68K_DBCC;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.condition = 15u;
    instr.reg = 7u;
    instr.src.reg = 1u;
    instr.displacement = -18;
    instr.target = 0x100u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ORI_TO_CCR;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x1Fu;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ANDI_TO_CCR;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x1Fu;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EORI_TO_CCR;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x100u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ORI_TO_SR;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x2700u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ANDI_TO_SR;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0xFFu;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EORI_TO_SR;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x10000u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_USP;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.reg = 3u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_USP;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.reg = 4u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 4u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_USP;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.reg = 4u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 4u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_USP;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.reg = 4u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 4u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 4u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TRAP;
    instr.byte_length = 2u;
    instr.immediate = 15u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TRAP;
    instr.byte_length = 2u;
    instr.immediate = 16u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_STOP;
    instr.byte_length = 4u;
    instr.immediate = 0x2700u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_STOP;
    instr.byte_length = 2u;
    instr.immediate = 0x2700u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NOP;
    instr.byte_length = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_RESET;
    instr.byte_length = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_RTE;
    instr.byte_length = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_RTR;
    instr.byte_length = 4u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_RTS;
    instr.byte_length = 2u;
    instr.immediate = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TRAPV;
    instr.byte_length = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ILLEGAL;
    instr.byte_length = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ILLEGAL;
    instr.byte_length = 4u;
    CHECK(!ng_m68k_validate(&instr));

    return 0;
}
