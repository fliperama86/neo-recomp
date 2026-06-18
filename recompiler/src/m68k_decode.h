#pragma once

#include <stdint.h>
#include "p_rom.h"

typedef enum NgM68kMnemonic {
    NG_M68K_INVALID,
    NG_M68K_UNKNOWN,
    NG_M68K_NOP,
    NG_M68K_RTS,
    NG_M68K_JMP,
    NG_M68K_JSR,
    NG_M68K_LINK,
    NG_M68K_UNLK,
    NG_M68K_BRA,
    NG_M68K_BSR,
    NG_M68K_BCC,
    NG_M68K_SCC,
    NG_M68K_DBCC,
    NG_M68K_CHK,
    NG_M68K_LEA,
    NG_M68K_MOVEA,
    NG_M68K_MOVEQ,
    NG_M68K_MOVE,
    NG_M68K_MOVEM,
    NG_M68K_MOVEP,
    NG_M68K_MOVE_SR,
    NG_M68K_MOVE_CCR,
    NG_M68K_MOVE_USP,
    NG_M68K_ADD,
    NG_M68K_ADDA,
    NG_M68K_ADDQ,
    NG_M68K_ADDX,
    NG_M68K_ABCD,
    NG_M68K_SUB,
    NG_M68K_SUBA,
    NG_M68K_SUBQ,
    NG_M68K_SUBX,
    NG_M68K_SBCD,
    NG_M68K_CMP,
    NG_M68K_CMPA,
    NG_M68K_CMPM,
    NG_M68K_OR,
    NG_M68K_AND,
    NG_M68K_EOR,
    NG_M68K_MULU,
    NG_M68K_MULS,
    NG_M68K_DIVU,
    NG_M68K_DIVS,
    NG_M68K_EXG,
    NG_M68K_CLR,
    NG_M68K_NEG,
    NG_M68K_NEGX,
    NG_M68K_NBCD,
    NG_M68K_NOT,
    NG_M68K_EXT,
    NG_M68K_SWAP,
    NG_M68K_ASL,
    NG_M68K_ASR,
    NG_M68K_LSL,
    NG_M68K_LSR,
    NG_M68K_ROXL,
    NG_M68K_ROXR,
    NG_M68K_ROL,
    NG_M68K_ROR,
    NG_M68K_PEA,
    NG_M68K_TST,
    NG_M68K_TAS,
    NG_M68K_ADDI,
    NG_M68K_SUBI,
    NG_M68K_CMPI,
    NG_M68K_ORI,
    NG_M68K_ANDI,
    NG_M68K_EORI,
    NG_M68K_BTST,
    NG_M68K_BCHG,
    NG_M68K_BCLR,
    NG_M68K_BSET,
    NG_M68K_ORI_TO_CCR,
    NG_M68K_ORI_TO_SR,
    NG_M68K_ANDI_TO_CCR,
    NG_M68K_ANDI_TO_SR,
    NG_M68K_EORI_TO_CCR,
    NG_M68K_EORI_TO_SR,
} NgM68kMnemonic;

typedef enum NgM68kOperandForm {
    NG_M68K_FORM_NONE,
    NG_M68K_FORM_AREG_TO_ABS,
    NG_M68K_FORM_IMM_TO_ABS,
    NG_M68K_FORM_IMM_TO_DREG,
    NG_M68K_FORM_ABS_TO_DREG,
    NG_M68K_FORM_DREG_TO_ABS,
    NG_M68K_FORM_DREG_TO_DREG,
    NG_M68K_FORM_DREG,
    NG_M68K_FORM_AREG_DISP,
    NG_M68K_FORM_PC_INDEX_TO_AREG,
    NG_M68K_FORM_AREG_INDIRECT,
    NG_M68K_FORM_PC_RELATIVE,
    NG_M68K_FORM_ABS,
} NgM68kOperandForm;

typedef enum NgM68kEaMode {
    NG_M68K_EA_NONE,
    NG_M68K_EA_DREG,
    NG_M68K_EA_AREG,
    NG_M68K_EA_AIND,
    NG_M68K_EA_APOST,
    NG_M68K_EA_APRE,
    NG_M68K_EA_ADISP,
    NG_M68K_EA_AINDEX,
    NG_M68K_EA_ABS_W,
    NG_M68K_EA_ABS_L,
    NG_M68K_EA_PC_DISP,
    NG_M68K_EA_PC_INDEX,
    NG_M68K_EA_IMM,
} NgM68kEaMode;

typedef struct NgM68kEa {
    NgM68kEaMode mode;
    uint8_t reg;
    uint8_t index_reg;
    uint8_t index_is_addr;
    uint8_t index_is_long;
    int16_t displacement;
    uint32_t absolute_addr;
    uint32_t immediate;
} NgM68kEa;

typedef struct NgM68kInstr {
    uint32_t addr;
    uint16_t opcode;
    NgM68kMnemonic mnemonic;
    uint8_t byte_length;
    uint8_t reg;
    uint8_t src_reg;
    uint8_t size;
    uint8_t condition;
    NgM68kOperandForm form;
    uint32_t target;
    uint32_t immediate;
    uint32_t absolute_addr;
    int16_t displacement;
    NgM68kEa src;
    NgM68kEa dst;
} NgM68kInstr;

int ng_m68k_decode(const NgProgramRom *rom, uint32_t addr, NgM68kInstr *out);
const char *ng_m68k_mnemonic_name(NgM68kMnemonic mnemonic);
void ng_m68k_format(const NgM68kInstr *instr, char *out, unsigned out_size);
