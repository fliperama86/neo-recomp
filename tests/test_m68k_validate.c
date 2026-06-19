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
    instr.mnemonic = NG_M68K_MOVE;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_IMM;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BCC;
    instr.byte_length = 2u;
    instr.condition = 16u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BRA;
    instr.byte_length = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BRA;
    instr.byte_length = 6u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BSR;
    instr.byte_length = 4u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BSR;
    instr.byte_length = 2u;
    instr.src.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BCC;
    instr.byte_length = 2u;
    instr.condition = 2u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BCC;
    instr.byte_length = 4u;
    instr.condition = 15u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BCC;
    instr.byte_length = 2u;
    instr.condition = 1u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_BCC;
    instr.byte_length = 6u;
    instr.condition = 6u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEQ;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.reg = 0u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEQ;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.reg = 0u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEQ;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.reg = 0u;
    instr.dst.mode = NG_M68K_EA_DREG;
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
    instr.mnemonic = NG_M68K_CMPI;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_PC_DISP;
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
    instr.mnemonic = NG_M68K_EXT;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXT;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXT;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SWAP;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SWAP;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SWAP;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.dst.mode = NG_M68K_EA_DREG;
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
    instr.dst.mode = NG_M68K_EA_DREG;
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
    instr.dst.mode = NG_M68K_EA_DREG;
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
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_NBCD;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ORI;
    instr.byte_length = 4u;
    instr.size = 1u;
    instr.dst.mode = NG_M68K_EA_PC_DISP;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EORI;
    instr.byte_length = 4u;
    instr.size = 4u;
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
    instr.mnemonic = NG_M68K_SUB;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_AIND;
    CHECK(ng_m68k_validate(&instr));

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
    instr.mnemonic = NG_M68K_EXG;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXG;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_EXG;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_AREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LINK;
    instr.byte_length = 4u;
    instr.reg = 8u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LINK;
    instr.byte_length = 4u;
    instr.reg = 7u;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LINK;
    instr.byte_length = 2u;
    instr.reg = 7u;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LINK;
    instr.byte_length = 4u;
    instr.reg = 7u;
    instr.src.mode = NG_M68K_EA_AREG;
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
    instr.mnemonic = NG_M68K_MOVEP;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_ADISP;
    CHECK(ng_m68k_validate(&instr));

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
    instr.dst.mode = NG_M68K_EA_APOST;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_CMPM;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AIND;
    instr.dst.mode = NG_M68K_EA_APOST;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ADDX;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_APRE;
    instr.dst.mode = NG_M68K_EA_APRE;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SUBX;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_APRE;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ABCD;
    instr.byte_length = 2u;
    instr.size = 1u;
    instr.src.mode = NG_M68K_EA_DREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_SBCD;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_APRE;
    instr.dst.mode = NG_M68K_EA_APRE;
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
    instr.dst.mode = NG_M68K_EA_APRE;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.byte_length = 4u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_PC_DISP;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_MOVEM;
    instr.byte_length = 4u;
    instr.size = 4u;
    instr.src.mode = NG_M68K_EA_APRE;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LSL;
    instr.byte_length = 2u;
    instr.size = 4u;
    instr.immediate = 8u;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_LSR;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.src.mode = NG_M68K_EA_AREG;
    instr.dst.mode = NG_M68K_EA_DREG;
    CHECK(!ng_m68k_validate(&instr));

    memset(&instr, 0, sizeof(instr));
    instr.mnemonic = NG_M68K_ROR;
    instr.byte_length = 2u;
    instr.size = 2u;
    instr.immediate = 1u;
    instr.dst.mode = NG_M68K_EA_ABS_W;
    CHECK(ng_m68k_validate(&instr));

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
    instr.mnemonic = NG_M68K_DBCC;
    instr.byte_length = 4u;
    instr.condition = 15u;
    instr.reg = 8u;
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
