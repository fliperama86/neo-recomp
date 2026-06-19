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

static int ea_is_move_source(const NgM68kEa *ea, uint8_t size) {
    if (ea->mode == NG_M68K_EA_AREG) {
        return size != 1u;
    }
    return ea_is_data(ea);
}

static int ea_is_movem_reg_to_mem(const NgM68kEa *ea) {
    switch (ea->mode) {
    case NG_M68K_EA_AIND:
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

static int ea_is_movem_mem_to_reg(const NgM68kEa *ea) {
    return ea->mode == NG_M68K_EA_APOST || ea_is_control(ea);
}

static int no_ea_operands(const NgM68kInstr *instr) {
    return instr->src.mode == NG_M68K_EA_NONE &&
           instr->dst.mode == NG_M68K_EA_NONE;
}

static int valid_size(uint8_t size) {
    return size == 1u || size == 2u || size == 4u;
}

static int valid_word_or_long(uint8_t size) {
    return size == 2u || size == 4u;
}

static int valid_extend_pair(const NgM68kInstr *instr) {
    return (instr->src.mode == NG_M68K_EA_DREG &&
            instr->dst.mode == NG_M68K_EA_DREG) ||
           (instr->src.mode == NG_M68K_EA_APRE &&
            instr->dst.mode == NG_M68K_EA_APRE);
}

static int valid_no_operand_2byte(const NgM68kInstr *instr) {
    return instr->byte_length == 2u &&
           instr->size == 0u &&
           instr->immediate == 0u &&
           instr->reg == 0u &&
           instr->src_reg == 0u &&
           no_ea_operands(instr);
}

static int validate_move_usp(const NgM68kInstr *instr) {
    if (instr->byte_length != 2u ||
        instr->size != 4u ||
        instr->reg >= 8u) {
        return 0;
    }
    if (instr->src.mode == NG_M68K_EA_AREG &&
        instr->src.reg == instr->reg &&
        instr->dst.mode == NG_M68K_EA_NONE) {
        return 1;
    }
    if (instr->dst.mode == NG_M68K_EA_AREG &&
        instr->dst.reg == instr->reg &&
        instr->src.mode == NG_M68K_EA_NONE) {
        return 1;
    }
    return 0;
}

static int validate_btst(const NgM68kInstr *instr) {
    if (instr->src.mode != NG_M68K_EA_NONE &&
        instr->src.mode != NG_M68K_EA_DREG) {
        return 0;
    }
    if (instr->dst.mode == NG_M68K_EA_DREG) {
        return instr->size == 4u;
    }
    if (instr->size != 1u || !ea_is_data(&instr->dst)) {
        return 0;
    }
    if (instr->dst.mode == NG_M68K_EA_IMM &&
        instr->src.mode != NG_M68K_EA_DREG) {
        return 0;
    }
    return 1;
}

static int bit_alterable_ext_length(const NgM68kEa *ea, uint8_t *out_ext) {
    switch (ea->mode) {
    case NG_M68K_EA_DREG:
    case NG_M68K_EA_AIND:
    case NG_M68K_EA_APOST:
    case NG_M68K_EA_APRE:
        if (ea->reg >= 8u) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_ADISP:
    case NG_M68K_EA_AINDEX:
        if (ea->reg >= 8u) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_W:
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_L:
        *out_ext = 4u;
        return 1;
    default:
        return 0;
    }
}

static int validate_altering_bit_op(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;

    if (!ea_is_data_alterable(&instr->dst) ||
        !bit_alterable_ext_length(&instr->dst, &ext_len)) {
        return 0;
    }
    if (instr->dst.mode == NG_M68K_EA_DREG) {
        if (instr->size != 4u) {
            return 0;
        }
    } else if (instr->size != 1u) {
        return 0;
    }

    if (instr->src.mode == NG_M68K_EA_DREG) {
        return instr->src.reg < 8u &&
               instr->src_reg == instr->src.reg &&
               instr->immediate == 0u &&
               instr->byte_length == (uint8_t)(2u + ext_len);
    }

    if (instr->src.mode == NG_M68K_EA_NONE) {
        return instr->src.reg == 0u &&
               instr->src_reg == 0u &&
               instr->immediate <= 0xFFu &&
               instr->byte_length == (uint8_t)(4u + ext_len);
    }

    return 0;
}

static int quick_alterable_ext_length(const NgM68kEa *ea, uint8_t *out_ext) {
    switch (ea->mode) {
    case NG_M68K_EA_DREG:
    case NG_M68K_EA_AREG:
    case NG_M68K_EA_AIND:
    case NG_M68K_EA_APOST:
    case NG_M68K_EA_APRE:
        if (ea->reg >= 8u) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_ADISP:
    case NG_M68K_EA_AINDEX:
        if (ea->reg >= 8u) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_W:
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_L:
        *out_ext = 4u;
        return 1;
    default:
        return 0;
    }
}

static int validate_quick_op(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;

    if (instr->immediate < 1u ||
        instr->immediate > 8u ||
        instr->src.mode != NG_M68K_EA_NONE ||
        instr->src_reg != 0u ||
        !quick_alterable_ext_length(&instr->dst, &ext_len)) {
        return 0;
    }

    if (instr->dst.mode == NG_M68K_EA_AREG) {
        return valid_word_or_long(instr->size) &&
               instr->byte_length == (uint8_t)(2u + ext_len);
    }

    return valid_size(instr->size) &&
           ea_is_data_alterable(&instr->dst) &&
           instr->byte_length == (uint8_t)(2u + ext_len);
}

static int data_source_ext_length(const NgM68kEa *ea,
                                  uint8_t size,
                                  uint8_t *out_ext) {
    switch (ea->mode) {
    case NG_M68K_EA_DREG:
    case NG_M68K_EA_AIND:
    case NG_M68K_EA_APOST:
    case NG_M68K_EA_APRE:
        if (ea->reg >= 8u) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_ADISP:
    case NG_M68K_EA_AINDEX:
        if (ea->reg >= 8u) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_W:
    case NG_M68K_EA_PC_DISP:
    case NG_M68K_EA_PC_INDEX:
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_L:
        *out_ext = 4u;
        return 1;
    case NG_M68K_EA_IMM:
        *out_ext = size == 4u ? 4u : 2u;
        return valid_size(size);
    default:
        return 0;
    }
}

static int addsubcmp_source_ext_length(const NgM68kEa *ea,
                                       uint8_t size,
                                       uint8_t *out_ext) {
    if (ea->mode == NG_M68K_EA_AREG) {
        if (size == 1u || ea->reg >= 8u) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    }
    return data_source_ext_length(ea, size, out_ext);
}

static int memory_alterable_ext_length(const NgM68kEa *ea, uint8_t *out_ext) {
    switch (ea->mode) {
    case NG_M68K_EA_AIND:
    case NG_M68K_EA_APOST:
    case NG_M68K_EA_APRE:
        if (ea->reg >= 8u) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_ADISP:
    case NG_M68K_EA_AINDEX:
        if (ea->reg >= 8u) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_W:
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_L:
        *out_ext = 4u;
        return 1;
    default:
        return 0;
    }
}

static int data_alterable_ext_length(const NgM68kEa *ea, uint8_t *out_ext) {
    if (ea->mode == NG_M68K_EA_DREG) {
        if (ea->reg >= 8u) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    }
    return memory_alterable_ext_length(ea, out_ext);
}

static int validate_ea_to_dreg_binary(const NgM68kInstr *instr,
                                      int allow_areg_source) {
    uint8_t ext_len = 0u;
    int valid_src = allow_areg_source ?
        addsubcmp_source_ext_length(&instr->src, instr->size, &ext_len) :
        data_source_ext_length(&instr->src, instr->size, &ext_len);

    return valid_size(instr->size) &&
           instr->immediate == 0u &&
           instr->src_reg == instr->src.reg &&
           valid_src &&
           instr->dst.mode == NG_M68K_EA_DREG &&
           instr->dst.reg < 8u &&
           instr->byte_length == (uint8_t)(2u + ext_len);
}

static int validate_dreg_to_memory_binary(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;

    return valid_size(instr->size) &&
           instr->immediate == 0u &&
           instr->src.mode == NG_M68K_EA_DREG &&
           instr->src.reg < 8u &&
           instr->src_reg == instr->src.reg &&
           memory_alterable_ext_length(&instr->dst, &ext_len) &&
           instr->byte_length == (uint8_t)(2u + ext_len);
}

static int validate_dreg_to_data_alterable_binary(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;

    return valid_size(instr->size) &&
           instr->immediate == 0u &&
           instr->src.mode == NG_M68K_EA_DREG &&
           instr->src.reg < 8u &&
           instr->src_reg == instr->src.reg &&
           data_alterable_ext_length(&instr->dst, &ext_len) &&
           instr->byte_length == (uint8_t)(2u + ext_len);
}

static int validate_add_sub_or_and(const NgM68kInstr *instr) {
    int allow_areg_source =
        instr->mnemonic == NG_M68K_ADD ||
        instr->mnemonic == NG_M68K_SUB;

    if (instr->dst.mode == NG_M68K_EA_DREG) {
        return validate_ea_to_dreg_binary(instr, allow_areg_source);
    }
    return validate_dreg_to_memory_binary(instr);
}

static int valid_scc_length(uint8_t byte_length) {
    return byte_length == 2u || byte_length == 4u || byte_length == 6u;
}

static int valid_tst_length(uint8_t byte_length) {
    return byte_length == 2u || byte_length == 4u || byte_length == 6u;
}

static int valid_cmpi_length(uint8_t size, uint8_t byte_length) {
    if (size == 4u) {
        return byte_length == 6u || byte_length == 8u || byte_length == 10u;
    }
    return byte_length == 4u || byte_length == 6u || byte_length == 8u;
}

static int valid_immediate_width(uint8_t size, uint32_t immediate) {
    if (size == 1u) {
        return immediate <= 0xFFu;
    }
    if (size == 2u) {
        return immediate <= 0xFFFFu;
    }
    return size == 4u;
}

static int valid_single_ea_length(uint8_t byte_length) {
    return byte_length == 2u || byte_length == 4u || byte_length == 6u;
}

static int valid_word_data_source_length(const NgM68kEa *ea,
                                         uint8_t byte_length) {
    switch (ea->mode) {
    case NG_M68K_EA_DREG:
    case NG_M68K_EA_AIND:
    case NG_M68K_EA_APOST:
    case NG_M68K_EA_APRE:
        return byte_length == 2u;
    case NG_M68K_EA_ADISP:
    case NG_M68K_EA_AINDEX:
    case NG_M68K_EA_ABS_W:
    case NG_M68K_EA_PC_DISP:
    case NG_M68K_EA_PC_INDEX:
    case NG_M68K_EA_IMM:
        return byte_length == 4u;
    case NG_M68K_EA_ABS_L:
        return byte_length == 6u;
    default:
        return 0;
    }
}

static int validate_address_reg_op(const NgM68kInstr *instr) {
    return valid_word_or_long(instr->size) &&
           valid_single_ea_length(instr->byte_length) &&
           instr->immediate == 0u &&
           instr->src.mode != NG_M68K_EA_NONE &&
           instr->dst.mode == NG_M68K_EA_AREG &&
           instr->dst.reg < 8u;
}

static int validate_tst(const NgM68kInstr *instr) {
    return valid_size(instr->size) &&
           valid_tst_length(instr->byte_length) &&
           instr->immediate == 0u &&
           instr->src_reg == 0u &&
           instr->dst.mode == NG_M68K_EA_NONE &&
           ea_is_data_alterable(&instr->src);
}

static int validate_cmpi(const NgM68kInstr *instr) {
    return valid_size(instr->size) &&
           valid_cmpi_length(instr->size, instr->byte_length) &&
           valid_immediate_width(instr->size, instr->immediate) &&
           instr->src_reg == 0u &&
           instr->src.mode == NG_M68K_EA_NONE &&
           ea_is_data_alterable(&instr->dst);
}

static int validate_immediate_to_ea(const NgM68kInstr *instr) {
    return valid_size(instr->size) &&
           valid_cmpi_length(instr->size, instr->byte_length) &&
           valid_immediate_width(instr->size, instr->immediate) &&
           instr->src_reg == 0u &&
           instr->src.mode == NG_M68K_EA_NONE &&
           ea_is_data_alterable(&instr->dst);
}

static int validate_word_data_to_dreg(const NgM68kInstr *instr) {
    return instr->size == 2u &&
           valid_word_data_source_length(&instr->src, instr->byte_length) &&
           instr->immediate == 0u &&
           instr->src_reg == 0u &&
           ea_is_data(&instr->src) &&
           instr->dst.mode == NG_M68K_EA_DREG &&
           instr->dst.reg < 8u;
}

static int validate_immediate_to_ccr_sr(const NgM68kInstr *instr) {
    if (instr->byte_length != 4u || !no_ea_operands(instr)) {
        return 0;
    }

    switch (instr->mnemonic) {
    case NG_M68K_ORI_TO_CCR:
    case NG_M68K_ANDI_TO_CCR:
    case NG_M68K_EORI_TO_CCR:
        return instr->size == 1u && instr->immediate <= 0xFFu;
    case NG_M68K_ORI_TO_SR:
    case NG_M68K_ANDI_TO_SR:
    case NG_M68K_EORI_TO_SR:
        return instr->size == 2u && instr->immediate <= 0xFFFFu;
    default:
        return 0;
    }
}

static int validate_branch(const NgM68kInstr *instr) {
    if ((instr->byte_length != 2u && instr->byte_length != 4u) ||
        instr->size != 0u ||
        instr->immediate != 0u ||
        instr->reg != 0u ||
        instr->src_reg != 0u ||
        !no_ea_operands(instr)) {
        return 0;
    }
    if (instr->mnemonic == NG_M68K_BCC) {
        return instr->condition >= 2u && instr->condition <= 15u;
    }
    return instr->condition == 0u;
}

static int validate_shift_rotate(const NgM68kInstr *instr) {
    if (instr->dst.mode == NG_M68K_EA_DREG) {
        if (!valid_size(instr->size)) {
            return 0;
        }
        if (instr->src.mode != NG_M68K_EA_NONE) {
            return instr->src.mode == NG_M68K_EA_DREG;
        }
        return instr->immediate >= 1u && instr->immediate <= 8u;
    }

    return instr->src.mode == NG_M68K_EA_NONE &&
           instr->size == 2u &&
           instr->immediate == 1u &&
           ea_is_memory_alterable(&instr->dst);
}

int ng_m68k_validate(const NgM68kInstr *instr) {
    if (!instr ||
        instr->byte_length == 0u ||
        instr->mnemonic == NG_M68K_INVALID ||
        instr->mnemonic == NG_M68K_UNKNOWN) {
        return 0;
    }

    switch (instr->mnemonic) {
    case NG_M68K_NOP:
    case NG_M68K_RESET:
    case NG_M68K_RTE:
    case NG_M68K_RTR:
    case NG_M68K_RTS:
    case NG_M68K_TRAPV:
        return valid_no_operand_2byte(instr);
    case NG_M68K_ILLEGAL:
        return instr->byte_length == 2u &&
               instr->size == 0u &&
               (instr->immediate == 0u ||
                instr->immediate == 10u ||
                instr->immediate == 11u) &&
               instr->reg == 0u &&
               instr->src_reg == 0u &&
               no_ea_operands(instr);
    case NG_M68K_STOP:
        return instr->byte_length == 4u &&
               instr->size == 0u &&
               instr->immediate <= 0xFFFFu &&
               no_ea_operands(instr);
    case NG_M68K_TRAP:
        return instr->byte_length == 2u &&
               instr->size == 0u &&
               instr->immediate <= 15u &&
               no_ea_operands(instr);
    case NG_M68K_BRA:
    case NG_M68K_BSR:
    case NG_M68K_BCC:
        return validate_branch(instr);
    case NG_M68K_JMP:
    case NG_M68K_JSR:
        return instr->dst.mode == NG_M68K_EA_NONE &&
               ea_is_control(&instr->src);
    case NG_M68K_PEA:
        return instr->size == 4u &&
               instr->dst.mode == NG_M68K_EA_NONE &&
               ea_is_control(&instr->src);
    case NG_M68K_LEA:
        return instr->byte_length >= 2u &&
               instr->size == 4u &&
               ea_is_control(&instr->src) &&
               instr->dst.mode == NG_M68K_EA_AREG;
    case NG_M68K_MOVE:
        return valid_size(instr->size) &&
               ea_is_move_source(&instr->src, instr->size) &&
               ea_is_data_alterable(&instr->dst);
    case NG_M68K_MOVEA:
        return validate_address_reg_op(instr);
    case NG_M68K_ADDA:
    case NG_M68K_SUBA:
    case NG_M68K_CMPA:
        return validate_address_reg_op(instr);
    case NG_M68K_EXG:
        return instr->size == 4u &&
               ((instr->src.mode == NG_M68K_EA_DREG &&
                 instr->dst.mode == NG_M68K_EA_DREG) ||
                (instr->src.mode == NG_M68K_EA_AREG &&
                 instr->dst.mode == NG_M68K_EA_AREG) ||
                (instr->src.mode == NG_M68K_EA_DREG &&
                 instr->dst.mode == NG_M68K_EA_AREG));
    case NG_M68K_MOVE_USP:
        return validate_move_usp(instr);
    case NG_M68K_LINK:
        return instr->byte_length == 4u &&
               instr->reg < 8u &&
               no_ea_operands(instr);
    case NG_M68K_UNLK:
        return instr->byte_length == 2u &&
               instr->reg < 8u &&
               no_ea_operands(instr);
    case NG_M68K_MOVEP:
        return valid_word_or_long(instr->size) &&
               ((instr->src.mode == NG_M68K_EA_DREG &&
                 instr->dst.mode == NG_M68K_EA_ADISP) ||
                (instr->src.mode == NG_M68K_EA_ADISP &&
                 instr->dst.mode == NG_M68K_EA_DREG));
    case NG_M68K_MOVEM:
        if (!valid_word_or_long(instr->size)) {
            return 0;
        }
        if (instr->src.mode == NG_M68K_EA_NONE &&
            instr->dst.mode != NG_M68K_EA_NONE) {
            return ea_is_movem_reg_to_mem(&instr->dst);
        }
        if (instr->dst.mode == NG_M68K_EA_NONE &&
            instr->src.mode != NG_M68K_EA_NONE) {
            return ea_is_movem_mem_to_reg(&instr->src);
        }
        return 0;
    case NG_M68K_MOVE_SR:
    case NG_M68K_MOVE_CCR:
        if (instr->dst.mode != NG_M68K_EA_NONE) {
            return instr->size == 2u && ea_is_data_alterable(&instr->dst);
        }
        return instr->size == 2u && ea_is_data(&instr->src);
    case NG_M68K_CLR:
    case NG_M68K_NEG:
    case NG_M68K_NEGX:
    case NG_M68K_NOT:
        return valid_size(instr->size) &&
               instr->dst.mode != NG_M68K_EA_NONE &&
               ea_is_data_alterable(&instr->dst);
    case NG_M68K_NBCD:
    case NG_M68K_TAS:
        return instr->size == 1u &&
               instr->dst.mode != NG_M68K_EA_NONE &&
               ea_is_data_alterable(&instr->dst);
    case NG_M68K_ADDI:
    case NG_M68K_SUBI:
    case NG_M68K_ORI:
    case NG_M68K_ANDI:
    case NG_M68K_EORI:
        return validate_immediate_to_ea(instr);
    case NG_M68K_ORI_TO_CCR:
    case NG_M68K_ORI_TO_SR:
    case NG_M68K_ANDI_TO_CCR:
    case NG_M68K_ANDI_TO_SR:
    case NG_M68K_EORI_TO_CCR:
    case NG_M68K_EORI_TO_SR:
        return validate_immediate_to_ccr_sr(instr);
    case NG_M68K_ADD:
    case NG_M68K_SUB:
    case NG_M68K_OR:
    case NG_M68K_AND:
        return validate_add_sub_or_and(instr);
    case NG_M68K_CMP:
        return validate_ea_to_dreg_binary(instr, 1);
    case NG_M68K_CMPM:
        return valid_size(instr->size) &&
               instr->src.mode == NG_M68K_EA_APOST &&
               instr->dst.mode == NG_M68K_EA_APOST;
    case NG_M68K_EOR:
        return validate_dreg_to_data_alterable_binary(instr);
    case NG_M68K_BTST:
        return validate_btst(instr);
    case NG_M68K_BCHG:
    case NG_M68K_BCLR:
    case NG_M68K_BSET:
        return validate_altering_bit_op(instr);
    case NG_M68K_ADDQ:
    case NG_M68K_SUBQ:
        return validate_quick_op(instr);
    case NG_M68K_TST:
        return validate_tst(instr);
    case NG_M68K_CMPI:
        return validate_cmpi(instr);
    case NG_M68K_CHK:
        return validate_word_data_to_dreg(instr);
    case NG_M68K_MULU:
    case NG_M68K_MULS:
    case NG_M68K_DIVU:
    case NG_M68K_DIVS:
        return validate_word_data_to_dreg(instr);
    case NG_M68K_EXT:
        return instr->byte_length == 2u &&
               (instr->size == 2u || instr->size == 4u) &&
               instr->src.mode == NG_M68K_EA_NONE &&
               instr->dst.mode == NG_M68K_EA_DREG;
    case NG_M68K_SWAP:
        return instr->byte_length == 2u &&
               instr->size == 4u &&
               instr->src.mode == NG_M68K_EA_NONE &&
               instr->dst.mode == NG_M68K_EA_DREG;
    case NG_M68K_SCC:
        return valid_scc_length(instr->byte_length) &&
               instr->condition <= 15u &&
               instr->size == 1u &&
               instr->immediate == 0u &&
               instr->reg == 0u &&
               instr->src_reg == 0u &&
               instr->src.mode == NG_M68K_EA_NONE &&
               ea_is_data_alterable(&instr->dst);
    case NG_M68K_DBCC:
        return instr->byte_length == 4u &&
               instr->size == 2u &&
               instr->condition <= 15u &&
               instr->reg < 8u &&
               instr->immediate == 0u &&
               instr->src_reg == 0u &&
               no_ea_operands(instr);
    case NG_M68K_ASL:
    case NG_M68K_ASR:
    case NG_M68K_LSL:
    case NG_M68K_LSR:
    case NG_M68K_ROXL:
    case NG_M68K_ROXR:
    case NG_M68K_ROL:
    case NG_M68K_ROR:
        return validate_shift_rotate(instr);
    case NG_M68K_ADDX:
    case NG_M68K_SUBX:
        return valid_size(instr->size) && valid_extend_pair(instr);
    case NG_M68K_ABCD:
    case NG_M68K_SBCD:
        return instr->size == 1u && valid_extend_pair(instr);
    case NG_M68K_MOVEQ:
        return instr->byte_length == 2u &&
               instr->size == 4u &&
               instr->reg < 8u &&
               no_ea_operands(instr);
    default:
        return 1;
    }
}
