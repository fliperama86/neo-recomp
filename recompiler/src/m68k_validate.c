#include "m68k_validate.h"

static int ea_is_control(const NgM68kEa *ea) {
    switch (ea->mode) {
    case NG_M68K_EA_AIND:
    case NG_M68K_EA_ADISP:
    case NG_M68K_EA_AINDEX:
    case NG_M68K_EA_ABS_W:
    case NG_M68K_EA_ABS_L:
    case NG_M68K_EA_PC_DISP:
    case NG_M68K_EA_PC_INDEX:
        return 1;
    default:
        return 0;
    }
}

static int ea_is_memory_alterable(const NgM68kEa *ea) {
    switch (ea->mode) {
    case NG_M68K_EA_AIND:
    case NG_M68K_EA_APOST:
    case NG_M68K_EA_APRE:
    case NG_M68K_EA_ADISP:
    case NG_M68K_EA_AINDEX:
    case NG_M68K_EA_ABS_W:
    case NG_M68K_EA_ABS_L:
        return 1;
    default:
        return 0;
    }
}

static int ea_is_data_alterable(const NgM68kEa *ea) {
    return ea->mode == NG_M68K_EA_DREG || ea_is_memory_alterable(ea);
}

static int ea_is_data(const NgM68kEa *ea) {
    switch (ea->mode) {
    case NG_M68K_EA_DREG:
    case NG_M68K_EA_AIND:
    case NG_M68K_EA_APOST:
    case NG_M68K_EA_APRE:
    case NG_M68K_EA_ADISP:
    case NG_M68K_EA_AINDEX:
    case NG_M68K_EA_ABS_W:
    case NG_M68K_EA_ABS_L:
    case NG_M68K_EA_PC_DISP:
    case NG_M68K_EA_PC_INDEX:
    case NG_M68K_EA_IMM:
        return 1;
    default:
        return 0;
    }
}

static int valid_size(uint8_t size) {
    return size == 1u || size == 2u || size == 4u;
}

int ng_m68k_validate(const NgM68kInstr *instr) {
    if (!instr ||
        instr->byte_length == 0u ||
        instr->mnemonic == NG_M68K_INVALID ||
        instr->mnemonic == NG_M68K_UNKNOWN) {
        return 0;
    }

    switch (instr->mnemonic) {
    case NG_M68K_JMP:
    case NG_M68K_JSR:
    case NG_M68K_PEA:
        return ea_is_control(&instr->src);
    case NG_M68K_LEA:
        return instr->src.mode != NG_M68K_EA_NONE ?
            ea_is_control(&instr->src) : instr->byte_length >= 4u;
    case NG_M68K_MOVE:
        return valid_size(instr->size) &&
               instr->src.mode != NG_M68K_EA_NONE &&
               ea_is_data_alterable(&instr->dst);
    case NG_M68K_MOVEA:
        return (instr->size == 2u || instr->size == 4u) &&
               instr->dst.mode == NG_M68K_EA_AREG;
    case NG_M68K_MOVE_SR:
    case NG_M68K_MOVE_CCR:
        if (instr->dst.mode != NG_M68K_EA_NONE) {
            return instr->size == 2u && ea_is_data_alterable(&instr->dst);
        }
        return instr->size == 2u && ea_is_data(&instr->src);
    case NG_M68K_CLR:
    case NG_M68K_NEG:
    case NG_M68K_NEGX:
    case NG_M68K_NBCD:
    case NG_M68K_NOT:
    case NG_M68K_TAS:
        return instr->dst.mode != NG_M68K_EA_NONE &&
               ea_is_data_alterable(&instr->dst);
    case NG_M68K_ADDI:
    case NG_M68K_SUBI:
    case NG_M68K_ORI:
    case NG_M68K_ANDI:
    case NG_M68K_EORI:
        return valid_size(instr->size) && ea_is_data_alterable(&instr->dst);
    case NG_M68K_ADD:
    case NG_M68K_SUB:
    case NG_M68K_OR:
    case NG_M68K_AND:
        if (!valid_size(instr->size)) {
            return 0;
        }
        if (instr->dst.mode == NG_M68K_EA_DREG) {
            return ea_is_data(&instr->src);
        }
        return instr->src.mode == NG_M68K_EA_DREG &&
               ea_is_memory_alterable(&instr->dst);
    case NG_M68K_CMP:
        return valid_size(instr->size) &&
               instr->dst.mode == NG_M68K_EA_DREG &&
               ea_is_data(&instr->src);
    case NG_M68K_EOR:
        return valid_size(instr->size) &&
               instr->src.mode == NG_M68K_EA_DREG &&
               ea_is_data_alterable(&instr->dst);
    case NG_M68K_BTST:
        return (instr->dst.mode == NG_M68K_EA_DREG ?
                    instr->size == 4u :
                    instr->size == 1u) &&
               ea_is_data(&instr->dst);
    case NG_M68K_BCHG:
    case NG_M68K_BCLR:
    case NG_M68K_BSET:
        return (instr->dst.mode == NG_M68K_EA_DREG ?
                    instr->size == 4u :
                    instr->size == 1u) &&
               ea_is_data_alterable(&instr->dst);
    case NG_M68K_ADDQ:
    case NG_M68K_SUBQ:
        if (instr->immediate < 1u || instr->immediate > 8u) {
            return 0;
        }
        if (instr->dst.mode == NG_M68K_EA_AREG) {
            return instr->size == 2u || instr->size == 4u;
        }
        return valid_size(instr->size) && ea_is_data_alterable(&instr->dst);
    case NG_M68K_TST:
        return valid_size(instr->size) && ea_is_data_alterable(&instr->src);
    case NG_M68K_CMPI:
        return valid_size(instr->size) && ea_is_data_alterable(&instr->dst);
    case NG_M68K_CHK:
        return instr->size == 2u &&
               ea_is_data(&instr->src) &&
               instr->dst.mode == NG_M68K_EA_DREG;
    case NG_M68K_MULU:
    case NG_M68K_MULS:
    case NG_M68K_DIVU:
    case NG_M68K_DIVS:
        return instr->size == 2u &&
               ea_is_data(&instr->src) &&
               instr->dst.mode == NG_M68K_EA_DREG;
    case NG_M68K_EXT:
        return (instr->size == 2u || instr->size == 4u) &&
               instr->dst.mode == NG_M68K_EA_DREG;
    case NG_M68K_SWAP:
        return instr->dst.mode == NG_M68K_EA_DREG;
    case NG_M68K_BCC:
    case NG_M68K_SCC:
    case NG_M68K_DBCC:
        return instr->condition <= 15u;
    case NG_M68K_MOVEQ:
        return instr->reg < 8u;
    default:
        return 1;
    }
}
