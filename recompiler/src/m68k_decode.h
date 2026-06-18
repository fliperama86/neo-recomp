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
    NG_M68K_BRA,
    NG_M68K_BSR,
    NG_M68K_BCC,
    NG_M68K_LEA,
    NG_M68K_MOVEA,
    NG_M68K_MOVEQ,
    NG_M68K_MOVE,
    NG_M68K_ADD,
    NG_M68K_ADDQ,
    NG_M68K_ADDX,
    NG_M68K_SUB,
    NG_M68K_SUBQ,
    NG_M68K_CMP,
    NG_M68K_CLR,
    NG_M68K_NEG,
    NG_M68K_NOT,
    NG_M68K_TST,
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
    NG_M68K_ANDI_TO_SR,
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
