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
    instr.opcode = 0x4ED0u;
    instr.byte_length = 2u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.src.reg = 0u;
    instr.form = NG_M68K_FORM_AREG_INDIRECT;
    instr.reg = 0u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_JMP;
    instr.opcode = 0x4E90u;
    instr.byte_length = 2u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.src.reg = 0u;
    instr.form = NG_M68K_FORM_AREG_INDIRECT;
    instr.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x4EBAu;
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
    instr.opcode = 0x4EFAu;
    instr.byte_length = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 4u;
    instr.form = NG_M68K_FORM_PC_RELATIVE;
    instr.target = 4u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x4EB9u;
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
    instr.opcode = 0x4868u;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_ADISP;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_PEA;
    instr.opcode = 0x4868u;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_ADISP;
    instr.target = 0x100u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_PEA;
    instr.opcode = 0x4868u;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_ADISP;
    instr.absolute_addr = 0x100u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_PEA;
    instr.opcode = 0x4868u;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_ADISP;
    instr.displacement = 2;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_PEA;
    instr.opcode = 0x487Au;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 4;
    instr.src.absolute_addr = 6u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_PEA;
    instr.opcode = 0x487Au;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 4;
    instr.src.absolute_addr = 6u;
    instr.target = 6u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_PEA;
    instr.opcode = 0x4869u;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_ADISP;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x40C3u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_SR;
    instr.opcode = 0x40C2u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_SR;
    instr.opcode = 0x46C3u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_SR;
    instr.opcode = 0x40C3u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.dst.index_reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_SR;
    instr.byte_length = 6u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_ABS_L;
    instr.dst.absolute_addr = 0x123456u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.addr = 0x100u;
    instr.mnemonic = NG_M68K_MOVE_CCR;
    instr.opcode = 0x44FAu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 4;
    instr.src.absolute_addr = 0x106u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.mnemonic = NG_M68K_MOVE_CCR;
    instr.opcode = 0x44F9u;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 4;
    instr.src.absolute_addr = 0x106u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.mnemonic = NG_M68K_MOVE_CCR;
    instr.opcode = 0x46FAu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 4;
    instr.src.absolute_addr = 0x106u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.mnemonic = NG_M68K_MOVE_CCR;
    instr.opcode = 0x44FAu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 4;
    instr.src.absolute_addr = 0x108u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_CCR;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.src.reg = 4u;
    instr.src.immediate = 0x10000u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_CCR;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_SR;
    instr.opcode = 0x46FAu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 4u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_SR;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.src.reg = 4u;
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
    instr.opcode = 0x3008u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE;
    instr.opcode = 0x3401u;
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
    instr.opcode = 0x3201u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    instr.src_reg = 1u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE;
    instr.opcode = 0x2401u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    instr.src_reg = 1u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x21BCu;
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
    instr.opcode = 0x123Au;
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
    instr.opcode = 0x7080u;
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
    instr.opcode = 0x7280u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_IMM_TO_DREG;
    instr.reg = 0u;
    instr.immediate = 0xFFFFFF80u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEQ;
    instr.opcode = 0x7001u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_IMM_TO_DREG;
    instr.reg = 0u;
    instr.immediate = 0xFFFFFF80u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x5188u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUBQ;
    instr.opcode = 0x5189u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUBQ;
    instr.opcode = 0x5088u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUBQ;
    instr.opcode = 0x5148u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.opcode = 0x5248u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUBQ;
    instr.opcode = 0x5188u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.index_reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.opcode = 0x5442u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.immediate = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.opcode = 0x5242u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.immediate = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.opcode = 0x5482u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.immediate = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.opcode = 0x5442u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.immediate = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.dst.index_reg = 1u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.immediate = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 2u;
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
    instr.opcode = 0x522Bu;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 3u;
    instr.dst.displacement = 4;
    instr.form = NG_M68K_FORM_AREG_DISP;
    instr.reg = 3u;
    instr.displacement = 4;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 3u;
    instr.dst.index_reg = 1u;
    instr.form = NG_M68K_FORM_AREG_DISP;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDQ;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 3u;
    instr.dst.displacement = 4;
    instr.form = NG_M68K_FORM_AREG_DISP;
    instr.reg = 3u;
    instr.displacement = 6;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x4A41u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 1u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TST;
    instr.opcode = 0x4A81u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TST;
    instr.opcode = 0x4A41u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src.index_reg = 1u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TST;
    instr.opcode = 0x4A2Au;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_ADISP;
    instr.src.reg = 2u;
    instr.src.displacement = 4;
    instr.form = NG_M68K_FORM_AREG_DISP;
    instr.reg = 2u;
    instr.displacement = 6;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TST;
    instr.opcode = 0x4A41u;
    instr.byte_length = 0u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TST;
    instr.opcode = 0x4A41u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TST;
    instr.opcode = 0x4A41u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TST;
    instr.opcode = 0x4A7Au;
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
    instr.opcode = 0x0C00u;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x7Fu;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.form = NG_M68K_FORM_IMM_TO_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPI;
    instr.opcode = 0x0A00u;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x7Fu;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.form = NG_M68K_FORM_IMM_TO_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPI;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x7Fu;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.index_reg = 1u;
    instr.form = NG_M68K_FORM_IMM_TO_DREG;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.immediate = 7u;
    instr.dst.mode = NG_M68K_EA_PC_DISP;
    instr.dst.reg = 2u;
    instr.dst.displacement = 4;
    instr.dst.absolute_addr = 8u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BTST;
    instr.byte_length = 6u;
    instr.size = 1u;
    instr.immediate = 7u;
    instr.dst.mode = NG_M68K_EA_PC_DISP;
    instr.dst.reg = 2u;
    instr.dst.displacement = 4;
    instr.dst.absolute_addr = 10u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_IMM;
    instr.dst.reg = 4u;
    instr.dst.immediate = 0x12u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BTST;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_IMM;
    instr.dst.immediate = 0x12u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x47BCu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.src.reg = 4u;
    instr.src.immediate = 0x1234u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CHK;
    instr.opcode = 0x45BCu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.src.reg = 4u;
    instr.src.immediate = 0x1234u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CHK;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.src.reg = 4u;
    instr.src.immediate = 0x10000u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.mnemonic = NG_M68K_CHK;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 4;
    instr.src.absolute_addr = 0x108u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0xCEFCu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.src.reg = 4u;
    instr.src.immediate = 0xFFFFu;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    instr.reg = 7u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MULU;
    instr.opcode = 0xCFFCu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.src.reg = 4u;
    instr.src.immediate = 0xFFFFu;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    instr.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_DIVU;
    instr.opcode = 0x8EC1u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    instr.reg = 7u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_DIVU;
    instr.opcode = 0x8FC1u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    instr.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_DIVS;
    instr.opcode = 0x8FC1u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    instr.reg = 7u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_DIVS;
    instr.opcode = 0x8DC1u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    instr.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MULS;
    instr.opcode = 0xCFC1u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    instr.reg = 7u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MULS;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AINDEX;
    instr.src.reg = 1u;
    instr.src.index_reg = 9u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    instr.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_DIVU;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    instr.reg = 6u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_DIVU;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    instr.dst.index_reg = 1u;
    instr.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_DIVS;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    instr.reg = 7u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x4883u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 3u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXT;
    instr.opcode = 0x4882u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 3u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXT;
    instr.opcode = 0x48C3u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 3u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x4842u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SWAP;
    instr.opcode = 0x4843u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SWAP;
    instr.opcode = 0x4882u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.form = NG_M68K_FORM_DREG;
    instr.reg = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x4280u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    instr.reg = 0u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CLR;
    instr.opcode = 0x4281u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    instr.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CLR;
    instr.opcode = 0x4480u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    instr.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CLR;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.form = NG_M68K_FORM_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    instr.dst.index_reg = 1u;
    instr.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x4AC3u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.form = NG_M68K_FORM_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TAS;
    instr.opcode = 0x4803u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.form = NG_M68K_FORM_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TAS;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NBCD;
    instr.opcode = 0x4827u;
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
    instr.opcode = 0x4442u;
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
    instr.opcode = 0x4618u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_APOST;
    instr.dst.reg = 0u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NOT;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_APOST;
    instr.dst.reg = 0u;
    instr.dst.index_reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NOT;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_APOST;
    instr.dst.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NEGX;
    instr.opcode = 0x4039u;
    instr.byte_length = 6u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_L;
    instr.dst.reg = 1u;
    instr.dst.absolute_addr = 0x123456u;
    instr.form = NG_M68K_FORM_ABS;
    instr.absolute_addr = 0x123456u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NEGX;
    instr.byte_length = 6u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_L;
    instr.dst.reg = 1u;
    instr.dst.absolute_addr = 0x123456u;
    instr.form = NG_M68K_FORM_ABS;
    instr.absolute_addr = 0x123458u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NEGX;
    instr.byte_length = 6u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_L;
    instr.dst.absolute_addr = 0x123456u;
    instr.form = NG_M68K_FORM_ABS;
    instr.absolute_addr = 0x123456u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NEGX;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_L;
    instr.dst.reg = 1u;
    instr.dst.absolute_addr = 0x123456u;
    instr.form = NG_M68K_FORM_ABS;
    instr.absolute_addr = 0x123456u;
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
    instr.mnemonic = NG_M68K_NBCD;
    instr.opcode = 0x482Au;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 2u;
    instr.dst.displacement = 4;
    instr.form = NG_M68K_FORM_AREG_DISP;
    instr.reg = 2u;
    instr.displacement = 4;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NBCD;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 2u;
    instr.dst.displacement = 4;
    instr.form = NG_M68K_FORM_AREG_DISP;
    instr.reg = 2u;
    instr.displacement = 6;
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
    instr.opcode = 0x0A80u;
    instr.byte_length = 6u;
    instr.size = 4u;
    instr.immediate = 0x12345678u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.form = NG_M68K_FORM_IMM_TO_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EORI;
    instr.opcode = 0x0A40u;
    instr.byte_length = 6u;
    instr.size = 4u;
    instr.immediate = 0x12345678u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.form = NG_M68K_FORM_IMM_TO_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDI;
    instr.opcode = 0x0640u;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x1234u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.form = NG_M68K_FORM_IMM_TO_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDI;
    instr.opcode = 0x0440u;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x1234u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.form = NG_M68K_FORM_IMM_TO_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDI;
    instr.byte_length = 6u;
    instr.size = 1u;
    instr.immediate = 0x7Fu;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 2u;
    instr.dst.displacement = 4;
    instr.form = NG_M68K_FORM_AREG_DISP;
    instr.reg = 2u;
    instr.displacement = 6;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ORI;
    instr.byte_length = 6u;
    instr.size = 1u;
    instr.immediate = 0x7Fu;
    instr.dst.mode = NG_M68K_EA_ABS_W;
    instr.dst.reg = 0u;
    instr.dst.absolute_addr = 0x10000u;
    instr.form = NG_M68K_FORM_ABS;
    instr.absolute_addr = 0x10000u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x0A00u;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x7Fu;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.form = NG_M68K_FORM_IMM_TO_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADD;
    instr.opcode = 0xD050u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.dst.mode = NG_M68K_EA_AIND;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADD;
    instr.opcode = 0xD050u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADD;
    instr.opcode = 0xD049u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
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
    instr.opcode = 0xB84Bu;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 3u;
    instr.src_reg = 3u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 4u;
    instr.reg = 4u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADD;
    instr.opcode = 0xD001u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADD;
    instr.opcode = 0x9001u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 0u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x9190u;
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
    instr.opcode = 0x8607u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 7u;
    instr.src_reg = 7u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_OR;
    instr.opcode = 0xC607u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 7u;
    instr.src_reg = 7u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_AND;
    instr.opcode = 0xC607u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 7u;
    instr.src_reg = 7u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_OR;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AINDEX;
    instr.src.reg = 1u;
    instr.src.index_reg = 9u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.mnemonic = NG_M68K_CMP;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 4;
    instr.src.absolute_addr = 0x108u;
    instr.src_reg = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUB;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.dst.index_reg = 1u;
    instr.reg = 2u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_OR;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUB;
    instr.opcode = 0x9794u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 3u;
    instr.src_reg = 3u;
    instr.dst.mode = NG_M68K_EA_AIND;
    instr.dst.reg = 4u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUB;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 3u;
    instr.src.index_reg = 1u;
    instr.src_reg = 3u;
    instr.dst.mode = NG_M68K_EA_AIND;
    instr.dst.reg = 4u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_AND;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_AINDEX;
    instr.dst.reg = 2u;
    instr.dst.index_reg = 9u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_OR;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_W;
    instr.dst.reg = 0u;
    instr.dst.absolute_addr = 0x1234u;
    instr.form = NG_M68K_FORM_ABS;
    instr.absolute_addr = 0x4321u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EOR;
    instr.opcode = 0xBB06u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 5u;
    instr.src_reg = 5u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 6u;
    instr.reg = 6u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EOR;
    instr.opcode = 0xB906u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 5u;
    instr.src_reg = 5u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 6u;
    instr.reg = 6u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EOR;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 5u;
    instr.src_reg = 5u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 6u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.form = NG_M68K_FORM_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BSET;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.index_reg = 1u;
    instr.form = NG_M68K_FORM_DREG;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src.index_reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_APOST;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 7u;
    instr.dst.mode = NG_M68K_EA_AIND;
    instr.target = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BCHG;
    instr.byte_length = 6u;
    instr.size = 1u;
    instr.immediate = 7u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 3u;
    instr.dst.displacement = 4;
    instr.form = NG_M68K_FORM_AREG_DISP;
    instr.reg = 3u;
    instr.displacement = 4;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BCHG;
    instr.byte_length = 6u;
    instr.size = 1u;
    instr.immediate = 7u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 3u;
    instr.dst.displacement = 4;
    instr.form = NG_M68K_FORM_AREG_DISP;
    instr.reg = 3u;
    instr.displacement = 6;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0xB0C8u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPA;
    instr.opcode = 0xB2C8u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPA;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.index_reg = 1u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.src.reg = 4u;
    instr.src.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDA;
    instr.opcode = 0xD0FCu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.src.reg = 4u;
    instr.src.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDA;
    instr.opcode = 0xD2FCu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.src.reg = 4u;
    instr.src.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDA;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_IMM;
    instr.src.reg = 4u;
    instr.src.immediate = 0x10000u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.mnemonic = NG_M68K_MOVEA;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 4;
    instr.src.absolute_addr = 0x108u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.mnemonic = NG_M68K_MOVEA;
    instr.opcode = 0x227Bu;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_PC_INDEX;
    instr.src.reg = 3u;
    instr.src.index_reg = 2u;
    instr.src.displacement = 4;
    instr.src.absolute_addr = 0x106u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 1u;
    instr.reg = 1u;
    instr.form = NG_M68K_FORM_PC_INDEX_TO_AREG;
    instr.src_reg = 2u;
    instr.displacement = 4;
    instr.target = 0x106u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.mnemonic = NG_M68K_MOVEA;
    instr.opcode = 0x207Bu;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_PC_INDEX;
    instr.src.reg = 3u;
    instr.src.index_reg = 2u;
    instr.src.displacement = 4;
    instr.src.absolute_addr = 0x106u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 1u;
    instr.reg = 1u;
    instr.form = NG_M68K_FORM_PC_INDEX_TO_AREG;
    instr.src_reg = 2u;
    instr.displacement = 4;
    instr.target = 0x106u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x100u;
    instr.mnemonic = NG_M68K_MOVEA;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_PC_INDEX;
    instr.src.reg = 3u;
    instr.src.index_reg = 2u;
    instr.src.displacement = 4;
    instr.src.absolute_addr = 0x106u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 1u;
    instr.reg = 1u;
    instr.form = NG_M68K_FORM_PC_INDEX_TO_AREG;
    instr.src_reg = 3u;
    instr.displacement = 4;
    instr.target = 0x106u;
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
    instr.opcode = 0x3048u;
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
    instr.opcode = 0x45FAu;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 4u;
    instr.displacement = 2;
    instr.target = 4u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LEA;
    instr.opcode = 0x45FAu;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 4u;
    instr.displacement = 2;
    instr.target = 6u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LEA;
    instr.opcode = 0x45FAu;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 4u;
    instr.displacement = 4;
    instr.target = 4u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LEA;
    instr.opcode = 0x45F9u;
    instr.byte_length = 6u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_ABS_L;
    instr.src.reg = 1u;
    instr.src.absolute_addr = 0x123456u;
    instr.target = 0x123456u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LEA;
    instr.opcode = 0x45F9u;
    instr.byte_length = 6u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_ABS_L;
    instr.src.reg = 1u;
    instr.src.absolute_addr = 0x123456u;
    instr.target = 0x123450u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LEA;
    instr.opcode = 0x45D3u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.src.reg = 3u;
    instr.target = 0x123456u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LEA;
    instr.opcode = 0x43FAu;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 4u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0xC141u;
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
    instr.opcode = 0xC149u;
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
    instr.opcode = 0xC149u;
    instr.byte_length = 2u;
    instr.size = 4u;
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
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 0u;
    instr.src_reg = 0u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 1u;
    instr.reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXG;
    instr.opcode = 0xC58Bu;
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
    instr.opcode = 0xC58Au;
    instr.byte_length = 2u;
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
    instr.opcode = 0xC54Bu;
    instr.byte_length = 2u;
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
    instr.opcode = 0x4E57u;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.reg = 7u;
    instr.displacement = -4;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LINK;
    instr.opcode = 0x4E56u;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.reg = 7u;
    instr.displacement = -4;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LINK;
    instr.opcode = 0x4E5Fu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.reg = 7u;
    instr.displacement = -4;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x4E5Fu;
    instr.byte_length = 2u;
    instr.reg = 7u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_UNLK;
    instr.opcode = 0x4E5Eu;
    instr.byte_length = 2u;
    instr.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_UNLK;
    instr.opcode = 0x4E57u;
    instr.byte_length = 2u;
    instr.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x03CAu;
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
    instr.opcode = 0x03CAu;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src.index_reg = 2u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 2u;
    instr.dst.displacement = 0x10;
    instr.reg = 1u;
    instr.displacement = 0x10;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEP;
    instr.opcode = 0x03CAu;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 2u;
    instr.dst.displacement = 0x10;
    instr.dst.absolute_addr = 0x1000u;
    instr.reg = 1u;
    instr.displacement = 0x10;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEP;
    instr.opcode = 0x03C9u;
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
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEP;
    instr.opcode = 0x034Au;
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
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x030Au;
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
    instr.opcode = 0x030Au;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_ADISP;
    instr.src.reg = 2u;
    instr.src.displacement = -4;
    instr.src.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 1u;
    instr.reg = 1u;
    instr.displacement = -4;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEP;
    instr.opcode = 0x030Au;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_ADISP;
    instr.src.reg = 2u;
    instr.src.displacement = -4;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 1u;
    instr.dst.index_reg = 2u;
    instr.reg = 1u;
    instr.displacement = -4;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEP;
    instr.opcode = 0x030Bu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_ADISP;
    instr.src.reg = 2u;
    instr.src.displacement = -4;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 1u;
    instr.reg = 1u;
    instr.displacement = -4;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEP;
    instr.opcode = 0x038Au;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_ADISP;
    instr.src.reg = 2u;
    instr.src.displacement = -4;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 1u;
    instr.reg = 1u;
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
    instr.opcode = 0xBD8Du;
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
    instr.opcode = 0xBB8Du;
    instr.byte_length = 2u;
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
    instr.opcode = 0xBD4Du;
    instr.byte_length = 2u;
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
    instr.opcode = 0xBD8Du;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.dst.mode = NG_M68K_EA_APOST;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPM;
    instr.opcode = 0xBD8Du;
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
    instr.opcode = 0xBD8Du;
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
    instr.opcode = 0xBD8Du;
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
    instr.opcode = 0xBD8Du;
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
    instr.opcode = 0xBD8Du;
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
    instr.mnemonic = NG_M68K_CMPM;
    instr.opcode = 0xBD8Du;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_APOST;
    instr.src.reg = 5u;
    instr.src.index_reg = 1u;
    instr.src_reg = 5u;
    instr.dst.mode = NG_M68K_EA_APOST;
    instr.dst.reg = 6u;
    instr.reg = 6u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPM;
    instr.opcode = 0xBD8Du;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_APOST;
    instr.src.reg = 5u;
    instr.src_reg = 5u;
    instr.dst.mode = NG_M68K_EA_APOST;
    instr.dst.reg = 6u;
    instr.dst.displacement = 2;
    instr.reg = 6u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPM;
    instr.opcode = 0xBD8Du;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_APOST;
    instr.src.reg = 5u;
    instr.src_reg = 5u;
    instr.dst.mode = NG_M68K_EA_APOST;
    instr.dst.reg = 6u;
    instr.reg = 6u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPM;
    instr.opcode = 0xBD8Du;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_APOST;
    instr.src.reg = 5u;
    instr.src_reg = 5u;
    instr.dst.mode = NG_M68K_EA_APOST;
    instr.dst.reg = 6u;
    instr.reg = 6u;
    instr.target = 0x20u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDX;
    instr.opcode = 0xDD8Du;
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
    instr.mnemonic = NG_M68K_ADDX;
    instr.opcode = 0xDB8Du;
    instr.byte_length = 2u;
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
    instr.opcode = 0x9501u;
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
    instr.mnemonic = NG_M68K_SUBX;
    instr.opcode = 0x9701u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUBX;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_APRE;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDX;
    instr.opcode = 0xDD8Du;
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
    instr.opcode = 0x9501u;
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
    instr.opcode = 0x9501u;
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
    instr.opcode = 0x9501u;
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
    instr.mnemonic = NG_M68K_SUBX;
    instr.opcode = 0x9501u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src.index_reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDX;
    instr.opcode = 0xDD8Du;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_APRE;
    instr.src.reg = 5u;
    instr.src_reg = 5u;
    instr.dst.mode = NG_M68K_EA_APRE;
    instr.dst.reg = 6u;
    instr.dst.displacement = 2;
    instr.reg = 6u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ABCD;
    instr.opcode = 0xC501u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    instr.target = 0x10u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ABCD;
    instr.opcode = 0xC501u;
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
    instr.mnemonic = NG_M68K_ABCD;
    instr.opcode = 0xC701u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.form = NG_M68K_FORM_DREG_TO_DREG;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SBCD;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_APRE;
    instr.dst.mode = NG_M68K_EA_APRE;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SBCD;
    instr.opcode = 0x8509u;
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
    instr.mnemonic = NG_M68K_SBCD;
    instr.opcode = 0x8501u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_APRE;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_APRE;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ABCD;
    instr.opcode = 0xC509u;
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
    instr.opcode = 0x48E7u;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.immediate = 0x8000u;
    instr.dst.mode = NG_M68K_EA_APRE;
    instr.dst.reg = 7u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.opcode = 0x48A7u;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.immediate = 0x8000u;
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
    instr.dst.index_reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.opcode = 0x4CBAu;
    instr.byte_length = 6u;
    instr.size = 2u;
    instr.immediate = 0x0003u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 6u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.opcode = 0x48BAu;
    instr.byte_length = 6u;
    instr.size = 2u;
    instr.immediate = 0x0003u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 6u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.byte_length = 6u;
    instr.size = 2u;
    instr.immediate = 0x0003u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 8u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x0003u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    instr.src.reg = 2u;
    instr.src.displacement = 2;
    instr.src.absolute_addr = 6u;
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
    instr.byte_length = 6u;
    instr.size = 4u;
    instr.immediate = 0x0003u;
    instr.dst.mode = NG_M68K_EA_AINDEX;
    instr.dst.reg = 2u;
    instr.dst.index_reg = 1u;
    instr.dst.index_is_addr = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.opcode = 0x4C9Cu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x0003u;
    instr.src.mode = NG_M68K_EA_APOST;
    instr.src.reg = 4u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.opcode = 0x489Cu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x0003u;
    instr.src.mode = NG_M68K_EA_APOST;
    instr.src.reg = 4u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x0003u;
    instr.src.mode = NG_M68K_EA_APOST;
    instr.src.reg = 4u;
    instr.src.index_reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0xE18Bu;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LSL;
    instr.opcode = 0xE38Bu;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LSL;
    instr.opcode = 0xE08Bu;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LSL;
    instr.opcode = 0xE18Bu;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.dst.index_reg = 1u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LSL;
    instr.opcode = 0xE18Bu;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 3u;
    instr.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LSL;
    instr.opcode = 0xE18Bu;
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
    instr.opcode = 0xE26Fu;
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
    instr.opcode = 0xE46Fu;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    instr.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LSR;
    instr.opcode = 0xE26Fu;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.src.reg = 1u;
    instr.src.index_reg = 1u;
    instr.src_reg = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 7u;
    instr.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LSR;
    instr.opcode = 0xE26Fu;
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
    instr.opcode = 0xE6F8u;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_W;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ROR;
    instr.opcode = 0xE6F9u;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_W;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ROR;
    instr.opcode = 0xE4F8u;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_W;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ROR;
    instr.opcode = 0xE6F8u;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_W;
    instr.dst.index_reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ROR;
    instr.opcode = 0xE6F8u;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_W;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ROXR;
    instr.opcode = 0xE4F9u;
    instr.byte_length = 6u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_L;
    instr.dst.reg = 1u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ROXR;
    instr.byte_length = 6u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_L;
    instr.dst.reg = 1u;
    instr.dst.absolute_addr = 0x123456u;
    instr.absolute_addr = 0x123456u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.mnemonic = NG_M68K_ROL;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.reg = 2u;
    instr.form = NG_M68K_FORM_DREG;
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
    instr.opcode = 0x55C2u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.condition = 5u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.opcode = 0x54C2u;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.condition = 5u;
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.opcode = 0x55C3u;
    instr.byte_length = 2u;
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
    instr.dst.mode = NG_M68K_EA_DREG;
    instr.dst.reg = 2u;
    instr.dst.index_reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.opcode = 0x55EAu;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.condition = 5u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 2u;
    instr.dst.displacement = -4;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.condition = 5u;
    instr.dst.mode = NG_M68K_EA_ADISP;
    instr.dst.reg = 2u;
    instr.dst.index_reg = 1u;
    instr.dst.displacement = -4;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SCC;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.condition = 5u;
    instr.dst.mode = NG_M68K_EA_ABS_W;
    instr.dst.reg = 0u;
    instr.dst.absolute_addr = 0x10000u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x5FCFu;
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
    instr.opcode = 0x5ECFu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.condition = 15u;
    instr.reg = 7u;
    instr.displacement = -18;
    instr.target = 0x100u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.addr = 0x110u;
    instr.mnemonic = NG_M68K_DBCC;
    instr.opcode = 0x5FCEu;
    instr.byte_length = 4u;
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
    instr.opcode = 0x003Cu;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x1Fu;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ORI_TO_CCR;
    instr.opcode = 0x023Cu;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x1Fu;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ORI_TO_CCR;
    instr.opcode = 0x003Cu;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x1Fu;
    instr.reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ORI_TO_CCR;
    instr.opcode = 0x003Cu;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x1Fu;
    instr.form = NG_M68K_FORM_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ANDI_TO_CCR;
    instr.opcode = 0x023Cu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x1Fu;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EORI_TO_CCR;
    instr.opcode = 0x0A3Cu;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0x100u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ORI_TO_SR;
    instr.opcode = 0x007Cu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x2700u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ORI_TO_SR;
    instr.opcode = 0x003Cu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x2700u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ORI_TO_SR;
    instr.opcode = 0x007Cu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x2700u;
    instr.absolute_addr = 0x100u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ORI_TO_SR;
    instr.opcode = 0x007Cu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x2700u;
    instr.displacement = 2;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ANDI_TO_SR;
    instr.opcode = 0x027Cu;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.immediate = 0xFFu;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EORI_TO_SR;
    instr.opcode = 0x0A7Cu;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.immediate = 0x10000u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_USP;
    instr.opcode = 0x4E63u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.reg = 3u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 3u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_USP;
    instr.opcode = 0x4E63u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.reg = 3u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 3u;
    instr.src.index_reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_USP;
    instr.opcode = 0x4E63u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.reg = 3u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 3u;
    instr.dst.reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_USP;
    instr.opcode = 0x4E6Cu;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.reg = 4u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 4u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_USP;
    instr.opcode = 0x4E62u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.reg = 3u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_USP;
    instr.opcode = 0x4E6Bu;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.reg = 3u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.src.reg = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_USP;
    instr.opcode = 0x4E64u;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.reg = 4u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 4u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_USP;
    instr.opcode = 0x4E6Cu;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.reg = 4u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 4u;
    instr.dst.index_reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_USP;
    instr.opcode = 0x4E6Cu;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.reg = 4u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 4u;
    instr.target = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_USP;
    instr.opcode = 0x4E6Cu;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.reg = 4u;
    instr.dst.mode = NG_M68K_EA_AREG;
    instr.dst.reg = 4u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVE_USP;
    instr.opcode = 0x4E6Cu;
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
    instr.opcode = 0x4E4Fu;
    instr.byte_length = 2u;
    instr.immediate = 15u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TRAP;
    instr.opcode = 0x4E4Fu;
    instr.byte_length = 2u;
    instr.immediate = 16u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TRAP;
    instr.opcode = 0x4E44u;
    instr.byte_length = 2u;
    instr.immediate = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TRAP;
    instr.opcode = 0x4E73u;
    instr.byte_length = 2u;
    instr.immediate = 3u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_STOP;
    instr.opcode = 0x4E72u;
    instr.byte_length = 4u;
    instr.immediate = 0x2700u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_STOP;
    instr.opcode = 0x4E72u;
    instr.byte_length = 2u;
    instr.immediate = 0x2700u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_STOP;
    instr.opcode = 0x4E71u;
    instr.byte_length = 4u;
    instr.immediate = 0x2700u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_STOP;
    instr.opcode = 0x4E72u;
    instr.byte_length = 4u;
    instr.immediate = 0x2700u;
    instr.reg = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TRAP;
    instr.opcode = 0x4E43u;
    instr.byte_length = 2u;
    instr.immediate = 3u;
    instr.target = 0x10u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NOP;
    instr.opcode = 0x4E71u;
    instr.byte_length = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NOP;
    instr.opcode = 0x4E75u;
    instr.byte_length = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NOP;
    instr.opcode = 0x4E71u;
    instr.byte_length = 2u;
    instr.form = NG_M68K_FORM_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_RESET;
    instr.byte_length = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_RTE;
    instr.opcode = 0x4E73u;
    instr.byte_length = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_RTE;
    instr.opcode = 0x4E73u;
    instr.byte_length = 2u;
    instr.target = 0x20u;
    CHECK(!ng_m68k_validate(&instr));

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
    instr.opcode = 0x4E76u;
    instr.byte_length = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TRAPV;
    instr.opcode = 0x4E71u;
    instr.byte_length = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_TRAPV;
    instr.opcode = 0x4E76u;
    instr.byte_length = 2u;
    instr.absolute_addr = 0x20u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ILLEGAL;
    instr.opcode = 0x4AFCu;
    instr.byte_length = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ILLEGAL;
    instr.opcode = 0xA123u;
    instr.byte_length = 2u;
    instr.immediate = 10u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ILLEGAL;
    instr.opcode = 0xF456u;
    instr.byte_length = 2u;
    instr.immediate = 11u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ILLEGAL;
    instr.opcode = 0x4AFCu;
    instr.byte_length = 2u;
    instr.immediate = 10u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ILLEGAL;
    instr.opcode = 0xA123u;
    instr.byte_length = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ILLEGAL;
    instr.opcode = 0xA123u;
    instr.byte_length = 2u;
    instr.immediate = 11u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ILLEGAL;
    instr.opcode = 0x4E71u;
    instr.byte_length = 2u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ILLEGAL;
    instr.opcode = 0x4AFCu;
    instr.byte_length = 2u;
    instr.form = NG_M68K_FORM_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ILLEGAL;
    instr.opcode = 0x4AFCu;
    instr.byte_length = 4u;
    CHECK(!ng_m68k_validate(&instr));

    return 0;
}
