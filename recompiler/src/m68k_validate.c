#include "m68k_validate.h"

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

static int no_ea_operands(const NgM68kInstr *instr) {
    return instr->src.mode == NG_M68K_EA_NONE &&
           instr->dst.mode == NG_M68K_EA_NONE;
}

static int ea_is_empty(const NgM68kEa *ea) {
    return ea->mode == NG_M68K_EA_NONE &&
           ea->reg == 0u &&
           ea->index_reg == 0u &&
           ea->index_is_addr == 0u &&
           ea->index_is_long == 0u &&
           ea->displacement == 0 &&
           ea->absolute_addr == 0u &&
           ea->immediate == 0u;
}

static int no_ea_fields(const NgM68kInstr *instr) {
    return ea_is_empty(&instr->src) && ea_is_empty(&instr->dst);
}

static int ea_is_exact_register(const NgM68kEa *ea, NgM68kEaMode mode) {
    return ea->mode == mode &&
           ea->reg < 8u &&
           ea->index_reg == 0u &&
           ea->index_is_addr == 0u &&
           ea->index_is_long == 0u &&
           ea->displacement == 0 &&
           ea->absolute_addr == 0u &&
           ea->immediate == 0u;
}

static int valid_size(uint8_t size) {
    return size == 1u || size == 2u || size == 4u;
}

static int valid_word_or_long(uint8_t size) {
    return size == 2u || size == 4u;
}

static int valid_bool_field(uint8_t value) {
    return value <= 1u;
}

static int signed_byte_value(int16_t value) {
    return value >= -128 && value <= 127;
}

static uint32_t sign_extend_abs_word_value(uint32_t value) {
    return (uint32_t)(int32_t)(int16_t)(uint16_t)value;
}

static uint32_t add_signed_disp(uint32_t base, int16_t displacement) {
    return (uint32_t)((int64_t)base + (int64_t)displacement);
}

static int ea_simple_register_payload(const NgM68kEa *ea) {
    return ea->reg < 8u &&
           ea->index_reg == 0u &&
           ea->index_is_addr == 0u &&
           ea->index_is_long == 0u &&
           ea->displacement == 0 &&
           ea->absolute_addr == 0u &&
           ea->immediate == 0u;
}

static int ea_address_displacement_payload(const NgM68kEa *ea) {
    return ea->reg < 8u &&
           ea->index_reg == 0u &&
           ea->index_is_addr == 0u &&
           ea->index_is_long == 0u &&
           ea->absolute_addr == 0u &&
           ea->immediate == 0u;
}

static int ea_address_index_payload(const NgM68kEa *ea) {
    return ea->reg < 8u &&
           ea->index_reg < 8u &&
           valid_bool_field(ea->index_is_addr) &&
           valid_bool_field(ea->index_is_long) &&
           signed_byte_value(ea->displacement) &&
           ea->absolute_addr == 0u &&
           ea->immediate == 0u;
}

static int ea_abs_word_payload(const NgM68kEa *ea) {
    return ea->reg == 0u &&
           ea->index_reg == 0u &&
           ea->index_is_addr == 0u &&
           ea->index_is_long == 0u &&
           ea->displacement == 0 &&
           ea->absolute_addr == sign_extend_abs_word_value(ea->absolute_addr) &&
           ea->immediate == 0u;
}

static int ea_abs_long_payload(const NgM68kEa *ea) {
    return ea->reg == 1u &&
           ea->index_reg == 0u &&
           ea->index_is_addr == 0u &&
           ea->index_is_long == 0u &&
           ea->displacement == 0 &&
           ea->immediate == 0u;
}

static int ea_pc_displacement_payload(const NgM68kEa *ea,
                                      uint32_t extension_addr) {
    return ea->reg == 2u &&
           ea->index_reg == 0u &&
           ea->index_is_addr == 0u &&
           ea->index_is_long == 0u &&
           ea->absolute_addr == add_signed_disp(extension_addr, ea->displacement) &&
           ea->immediate == 0u;
}

static int ea_pc_index_payload(const NgM68kEa *ea,
                               uint32_t extension_addr) {
    return ea->reg == 3u &&
           ea->index_reg < 8u &&
           valid_bool_field(ea->index_is_addr) &&
           valid_bool_field(ea->index_is_long) &&
           signed_byte_value(ea->displacement) &&
           ea->absolute_addr == add_signed_disp(extension_addr, ea->displacement) &&
           ea->immediate == 0u;
}

static int ea_immediate_payload(const NgM68kEa *ea, uint8_t size) {
    if (ea->reg != 4u ||
        ea->index_reg != 0u ||
        ea->index_is_addr != 0u ||
        ea->index_is_long != 0u ||
        ea->displacement != 0 ||
        ea->absolute_addr != 0u) {
        return 0;
    }
    if (size == 1u) {
        return ea->immediate <= 0xFFu;
    }
    if (size == 2u) {
        return ea->immediate <= 0xFFFFu;
    }
    return size == 4u;
}

static int valid_extend_pair(const NgM68kInstr *instr) {
    uint16_t base_opcode = 0u;
    uint16_t size_bits = 0u;
    uint16_t form_bits = 0u;
    uint16_t expected_opcode = 0u;

    if (instr->byte_length != 2u ||
        instr->immediate != 0u ||
        instr->condition != 0u ||
        instr->target != 0u ||
        instr->absolute_addr != 0u ||
        instr->displacement != 0 ||
        instr->src_reg != instr->src.reg ||
        instr->reg != instr->dst.reg) {
        return 0;
    }

    switch (instr->mnemonic) {
    case NG_M68K_ADDX:
        base_opcode = 0xD100u;
        break;
    case NG_M68K_SUBX:
        base_opcode = 0x9100u;
        break;
    case NG_M68K_ABCD:
        base_opcode = 0xC100u;
        break;
    case NG_M68K_SBCD:
        base_opcode = 0x8100u;
        break;
    default:
        return 0;
    }

    if (instr->mnemonic == NG_M68K_ADDX ||
        instr->mnemonic == NG_M68K_SUBX) {
        if (!valid_size(instr->size)) {
            return 0;
        }
        size_bits = (instr->size == 1u) ? 0u :
                    ((instr->size == 2u) ? 0x0040u : 0x0080u);
    } else if (instr->size != 1u) {
        return 0;
    }

    if (instr->src.mode == NG_M68K_EA_DREG &&
        instr->dst.mode == NG_M68K_EA_DREG) {
        if (!ea_simple_register_payload(&instr->src) ||
            !ea_simple_register_payload(&instr->dst) ||
            instr->form != NG_M68K_FORM_DREG_TO_DREG) {
            return 0;
        }
        form_bits = 0u;
    } else if (instr->src.mode == NG_M68K_EA_APRE &&
               instr->dst.mode == NG_M68K_EA_APRE) {
        if (!ea_simple_register_payload(&instr->src) ||
            !ea_simple_register_payload(&instr->dst) ||
            instr->form != NG_M68K_FORM_NONE) {
            return 0;
        }
        form_bits = 0x0008u;
    } else {
        return 0;
    }

    expected_opcode = (uint16_t)(base_opcode |
                                 ((uint16_t)instr->dst.reg << 9) |
                                 size_bits |
                                 form_bits |
                                 instr->src.reg);
    return instr->opcode == expected_opcode;
}

static int valid_no_operand_2byte(const NgM68kInstr *instr) {
    return instr->byte_length == 2u &&
           instr->size == 0u &&
           instr->immediate == 0u &&
           instr->reg == 0u &&
           instr->src_reg == 0u &&
           instr->condition == 0u &&
           instr->form == NG_M68K_FORM_NONE &&
           instr->target == 0u &&
           instr->absolute_addr == 0u &&
           instr->displacement == 0 &&
           no_ea_operands(instr);
}

static int valid_fixed_no_operand_2byte(const NgM68kInstr *instr,
                                        uint16_t opcode) {
    return instr->opcode == opcode && valid_no_operand_2byte(instr);
}

static int valid_control_immediate_fields(const NgM68kInstr *instr) {
    return instr->reg == 0u &&
           instr->src_reg == 0u &&
           instr->condition == 0u &&
           instr->form == NG_M68K_FORM_NONE &&
           instr->target == 0u &&
           instr->absolute_addr == 0u &&
           instr->displacement == 0 &&
           no_ea_operands(instr);
}

static int validate_illegal_opcode_metadata(const NgM68kInstr *instr) {
    if (instr->byte_length != 2u ||
        instr->size != 0u ||
        !valid_control_immediate_fields(instr)) {
        return 0;
    }

    if (instr->immediate == 0u) {
        return instr->opcode == 0x4AFCu;
    }
    if (instr->immediate == 10u) {
        return (instr->opcode & 0xF000u) == 0xA000u;
    }
    if (instr->immediate == 11u) {
        return (instr->opcode & 0xF000u) == 0xF000u;
    }

    return 0;
}

static int validate_stop_metadata(const NgM68kInstr *instr) {
    return instr->opcode == 0x4E72u &&
           instr->byte_length == 4u &&
           instr->size == 0u &&
           instr->immediate <= 0xFFFFu &&
           valid_control_immediate_fields(instr);
}

static int validate_trap_metadata(const NgM68kInstr *instr) {
    return (instr->opcode & 0xFFF0u) == 0x4E40u &&
           (instr->opcode & 0x000Fu) == instr->immediate &&
           instr->byte_length == 2u &&
           instr->size == 0u &&
           instr->immediate <= 15u &&
           valid_control_immediate_fields(instr);
}

static int validate_move_usp(const NgM68kInstr *instr) {
    if (instr->byte_length != 2u ||
        instr->size != 4u ||
        instr->reg >= 8u ||
        instr->immediate != 0u ||
        instr->src_reg != 0u ||
        instr->condition != 0u ||
        instr->form != NG_M68K_FORM_NONE ||
        instr->target != 0u ||
        instr->absolute_addr != 0u ||
        instr->displacement != 0) {
        return 0;
    }
    if (instr->src.mode == NG_M68K_EA_AREG &&
        instr->src.reg == instr->reg &&
        ea_simple_register_payload(&instr->src)) {
        return instr->opcode == (uint16_t)(0x4E60u | instr->reg) &&
               ea_is_empty(&instr->dst);
    }
    if (instr->dst.mode == NG_M68K_EA_AREG &&
        instr->dst.reg == instr->reg &&
        ea_simple_register_payload(&instr->dst)) {
        return instr->opcode == (uint16_t)(0x4E68u | instr->reg) &&
               ea_is_empty(&instr->src);
    }
    return 0;
}

static int exact_data_alterable_ext_length(const NgM68kEa *ea,
                                           uint8_t *out_ext);
static int ea_opcode_field(const NgM68kEa *ea, uint8_t *out_field);

static int exact_bit_data_ext_length(const NgM68kEa *ea,
                                     uint8_t size,
                                     uint32_t extension_addr,
                                     int allow_immediate,
                                     uint8_t *out_ext) {
    switch (ea->mode) {
    case NG_M68K_EA_DREG:
        if (size != 4u || !ea_simple_register_payload(ea)) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_AIND:
    case NG_M68K_EA_APOST:
    case NG_M68K_EA_APRE:
        if (size != 1u || !ea_simple_register_payload(ea)) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_ADISP:
        if (size != 1u || !ea_address_displacement_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_AINDEX:
        if (size != 1u || !ea_address_index_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_W:
        if (size != 1u || !ea_abs_word_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_L:
        if (size != 1u || !ea_abs_long_payload(ea)) {
            return 0;
        }
        *out_ext = 4u;
        return 1;
    case NG_M68K_EA_PC_DISP:
        if (size != 1u || !ea_pc_displacement_payload(ea, extension_addr)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_PC_INDEX:
        if (size != 1u || !ea_pc_index_payload(ea, extension_addr)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_IMM:
        if (!allow_immediate || size != 1u || !ea_immediate_payload(ea, size)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    default:
        return 0;
    }
}

static int validate_bit_destination_legacy_fields(const NgM68kInstr *instr,
                                                  int dynamic_bit_number) {
    if (instr->condition != 0u ||
        instr->target != 0u) {
        return 0;
    }

    if (instr->dst.mode == NG_M68K_EA_DREG) {
        return instr->form == (dynamic_bit_number ? NG_M68K_FORM_DREG_TO_DREG :
                                                    NG_M68K_FORM_DREG) &&
               instr->reg == instr->dst.reg &&
               instr->absolute_addr == 0u &&
               instr->displacement == 0;
    }

    if (instr->dst.mode == NG_M68K_EA_ADISP) {
        return instr->form == NG_M68K_FORM_AREG_DISP &&
               instr->reg == instr->dst.reg &&
               instr->absolute_addr == 0u &&
               instr->displacement == instr->dst.displacement;
    }

    if (instr->dst.mode == NG_M68K_EA_ABS_W ||
        instr->dst.mode == NG_M68K_EA_ABS_L) {
        return instr->form == NG_M68K_FORM_ABS &&
               instr->reg == 0u &&
               instr->absolute_addr == instr->dst.absolute_addr &&
               instr->displacement == 0;
    }

    return instr->form == NG_M68K_FORM_NONE &&
           instr->reg == 0u &&
           instr->absolute_addr == 0u &&
           instr->displacement == 0;
}

static int validate_dynamic_bit_source(const NgM68kInstr *instr) {
    return instr->src.mode == NG_M68K_EA_DREG &&
           ea_simple_register_payload(&instr->src) &&
           instr->src_reg == instr->src.reg &&
           instr->immediate == 0u;
}

static int validate_static_bit_source(const NgM68kInstr *instr) {
    return ea_is_empty(&instr->src) &&
           instr->src_reg == 0u &&
           instr->immediate <= 0xFFu;
}

static int validate_btst(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;
    uint8_t base_len = instr->src.mode == NG_M68K_EA_DREG ? 2u : 4u;
    int dynamic_bit_number = instr->src.mode == NG_M68K_EA_DREG;

    if (dynamic_bit_number) {
        if (!validate_dynamic_bit_source(instr)) {
            return 0;
        }
    } else if (!validate_static_bit_source(instr)) {
        return 0;
    }

    return exact_bit_data_ext_length(&instr->dst,
                                     instr->size,
                                     instr->addr + base_len,
                                     dynamic_bit_number,
                                     &ext_len) &&
           validate_bit_destination_legacy_fields(instr, dynamic_bit_number) &&
           instr->byte_length == (uint8_t)(base_len + ext_len);
}

static int validate_altering_bit_op(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;
    uint8_t base_len = instr->src.mode == NG_M68K_EA_DREG ? 2u : 4u;
    int dynamic_bit_number = instr->src.mode == NG_M68K_EA_DREG;

    if (!exact_data_alterable_ext_length(&instr->dst, &ext_len)) {
        return 0;
    }

    if (instr->dst.mode == NG_M68K_EA_DREG) {
        if (instr->size != 4u) {
            return 0;
        }
    } else if (instr->size != 1u) {
        return 0;
    }

    if (dynamic_bit_number) {
        return validate_dynamic_bit_source(instr) &&
               validate_bit_destination_legacy_fields(instr, 1) &&
               instr->byte_length == (uint8_t)(base_len + ext_len);
    }

    return validate_static_bit_source(instr) &&
           validate_bit_destination_legacy_fields(instr, 0) &&
           instr->byte_length == (uint8_t)(base_len + ext_len);
}

static int quick_alterable_ext_length(const NgM68kEa *ea, uint8_t *out_ext) {
    switch (ea->mode) {
    case NG_M68K_EA_DREG:
    case NG_M68K_EA_AREG:
    case NG_M68K_EA_AIND:
    case NG_M68K_EA_APOST:
    case NG_M68K_EA_APRE:
        if (!ea_simple_register_payload(ea)) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_ADISP:
        if (!ea_address_displacement_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_AINDEX:
        if (!ea_address_index_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_W:
        if (!ea_abs_word_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_L:
        if (!ea_abs_long_payload(ea)) {
            return 0;
        }
        *out_ext = 4u;
        return 1;
    default:
        return 0;
    }
}

static int validate_quick_destination_legacy_fields(const NgM68kInstr *instr) {
    if (instr->dst.mode == NG_M68K_EA_DREG) {
        return instr->form == NG_M68K_FORM_DREG &&
               instr->reg == instr->dst.reg &&
               instr->absolute_addr == 0u &&
               instr->displacement == 0;
    }

    if (instr->dst.mode == NG_M68K_EA_ADISP) {
        return instr->form == NG_M68K_FORM_AREG_DISP &&
               instr->reg == instr->dst.reg &&
               instr->absolute_addr == 0u &&
               instr->displacement == instr->dst.displacement;
    }

    if (instr->dst.mode == NG_M68K_EA_ABS_W ||
        instr->dst.mode == NG_M68K_EA_ABS_L) {
        return instr->form == NG_M68K_FORM_ABS &&
               instr->reg == 0u &&
               instr->absolute_addr == instr->dst.absolute_addr &&
               instr->displacement == 0;
    }

    return instr->form == NG_M68K_FORM_NONE &&
           instr->reg == 0u &&
           instr->absolute_addr == 0u &&
           instr->displacement == 0;
}

static int validate_quick_op(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;
    uint8_t ea_field = 0u;
    uint16_t size_bits = 0u;
    uint16_t expected_opcode = 0u;

    if (instr->immediate < 1u ||
        instr->immediate > 8u ||
        !ea_is_empty(&instr->src) ||
        instr->src_reg != 0u ||
        instr->condition != 0u ||
        instr->target != 0u ||
        !quick_alterable_ext_length(&instr->dst, &ext_len) ||
        !ea_opcode_field(&instr->dst, &ea_field)) {
        return 0;
    }

    if (instr->size == 1u) {
        size_bits = 0u;
    } else if (instr->size == 2u) {
        size_bits = 0x0040u;
    } else if (instr->size == 4u) {
        size_bits = 0x0080u;
    } else {
        return 0;
    }

    expected_opcode =
        (uint16_t)(0x5000u |
                   ((uint16_t)((instr->immediate == 8u) ? 0u : instr->immediate) << 9) |
                   ((instr->mnemonic == NG_M68K_SUBQ) ? 0x0100u : 0u) |
                   size_bits |
                   ea_field);

    if (instr->dst.mode == NG_M68K_EA_AREG) {
        return valid_word_or_long(instr->size) &&
               validate_quick_destination_legacy_fields(instr) &&
               instr->byte_length == (uint8_t)(2u + ext_len) &&
               instr->opcode == expected_opcode;
    }

    return valid_size(instr->size) &&
           ea_is_data_alterable(&instr->dst) &&
           validate_quick_destination_legacy_fields(instr) &&
           instr->byte_length == (uint8_t)(2u + ext_len) &&
           instr->opcode == expected_opcode;
}

static int exact_memory_alterable_ext_length(const NgM68kEa *ea,
                                             uint8_t *out_ext) {
    switch (ea->mode) {
    case NG_M68K_EA_AIND:
    case NG_M68K_EA_APOST:
    case NG_M68K_EA_APRE:
        if (!ea_simple_register_payload(ea)) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_ADISP:
        if (!ea_address_displacement_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_AINDEX:
        if (!ea_address_index_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_W:
        if (!ea_abs_word_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_L:
        if (!ea_abs_long_payload(ea)) {
            return 0;
        }
        *out_ext = 4u;
        return 1;
    default:
        return 0;
    }
}

static int exact_data_alterable_ext_length(const NgM68kEa *ea,
                                           uint8_t *out_ext) {
    if (ea->mode == NG_M68K_EA_DREG) {
        if (!ea_simple_register_payload(ea)) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    }
    return exact_memory_alterable_ext_length(ea, out_ext);
}

static int move_source_ext_length(const NgM68kInstr *instr, uint8_t *out_ext) {
    const NgM68kEa *ea = &instr->src;
    uint32_t ext_addr = instr->addr + 2u;

    switch (ea->mode) {
    case NG_M68K_EA_DREG:
        if (!ea_simple_register_payload(ea)) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_AREG:
        if (instr->size == 1u || !ea_simple_register_payload(ea)) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_AIND:
    case NG_M68K_EA_APOST:
    case NG_M68K_EA_APRE:
        if (!ea_simple_register_payload(ea)) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_ADISP:
        if (!ea_address_displacement_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_AINDEX:
        if (!ea_address_index_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_W:
        if (!ea_abs_word_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_L:
        if (!ea_abs_long_payload(ea)) {
            return 0;
        }
        *out_ext = 4u;
        return 1;
    case NG_M68K_EA_PC_DISP:
        if (!ea_pc_displacement_payload(ea, ext_addr)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_PC_INDEX:
        if (!ea_pc_index_payload(ea, ext_addr)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_IMM:
        if (!ea_immediate_payload(ea, instr->size)) {
            return 0;
        }
        *out_ext = instr->size == 4u ? 4u : 2u;
        return valid_size(instr->size);
    default:
        return 0;
    }
}

static int move_destination_ext_length(const NgM68kEa *ea, uint8_t *out_ext) {
    switch (ea->mode) {
    case NG_M68K_EA_DREG:
        if (!ea_simple_register_payload(ea)) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_AIND:
    case NG_M68K_EA_APOST:
    case NG_M68K_EA_APRE:
        if (!ea_simple_register_payload(ea)) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_ADISP:
        if (!ea_address_displacement_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_AINDEX:
        if (!ea_address_index_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_W:
        if (!ea_abs_word_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_L:
        if (!ea_abs_long_payload(ea)) {
            return 0;
        }
        *out_ext = 4u;
        return 1;
    default:
        return 0;
    }
}

static int exact_data_source_ext_length(const NgM68kInstr *instr,
                                        uint8_t *out_ext) {
    const NgM68kEa *ea = &instr->src;
    uint32_t ext_addr = instr->addr + 2u;

    switch (ea->mode) {
    case NG_M68K_EA_DREG:
    case NG_M68K_EA_AIND:
    case NG_M68K_EA_APOST:
    case NG_M68K_EA_APRE:
        if (!ea_simple_register_payload(ea)) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_ADISP:
        if (!ea_address_displacement_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_AINDEX:
        if (!ea_address_index_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_W:
        if (!ea_abs_word_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_L:
        if (!ea_abs_long_payload(ea)) {
            return 0;
        }
        *out_ext = 4u;
        return 1;
    case NG_M68K_EA_PC_DISP:
        if (!ea_pc_displacement_payload(ea, ext_addr)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_PC_INDEX:
        if (!ea_pc_index_payload(ea, ext_addr)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_IMM:
        if (!ea_immediate_payload(ea, instr->size)) {
            return 0;
        }
        *out_ext = instr->size == 4u ? 4u : 2u;
        return valid_size(instr->size);
    default:
        return 0;
    }
}

static int exact_addsubcmp_source_ext_length(const NgM68kInstr *instr,
                                             uint8_t *out_ext) {
    const NgM68kEa *ea = &instr->src;

    if (ea->mode == NG_M68K_EA_AREG) {
        if (instr->size == 1u || !ea_simple_register_payload(ea)) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    }

    return exact_data_source_ext_length(instr, out_ext);
}

static int validate_move_legacy_fields(const NgM68kInstr *instr) {
    if (instr->src.mode == NG_M68K_EA_AREG &&
        (instr->dst.mode == NG_M68K_EA_ABS_W ||
         instr->dst.mode == NG_M68K_EA_ABS_L)) {
        return instr->form == NG_M68K_FORM_AREG_TO_ABS &&
               instr->reg == instr->src.reg &&
               instr->src_reg == 0u &&
               instr->immediate == 0u &&
               instr->absolute_addr == instr->dst.absolute_addr &&
               instr->displacement == 0;
    }

    if (instr->src.mode == NG_M68K_EA_IMM &&
        (instr->dst.mode == NG_M68K_EA_ABS_W ||
         instr->dst.mode == NG_M68K_EA_ABS_L)) {
        return instr->form == NG_M68K_FORM_IMM_TO_ABS &&
               instr->reg == 0u &&
               instr->src_reg == 0u &&
               instr->immediate == instr->src.immediate &&
               instr->absolute_addr == instr->dst.absolute_addr &&
               instr->displacement == 0;
    }

    if (instr->src.mode == NG_M68K_EA_DREG &&
        (instr->dst.mode == NG_M68K_EA_ABS_W ||
         instr->dst.mode == NG_M68K_EA_ABS_L)) {
        return instr->form == NG_M68K_FORM_DREG_TO_ABS &&
               instr->reg == instr->src.reg &&
               instr->src_reg == 0u &&
               instr->immediate == 0u &&
               instr->absolute_addr == instr->dst.absolute_addr &&
               instr->displacement == 0;
    }

    if ((instr->src.mode == NG_M68K_EA_ABS_W ||
         instr->src.mode == NG_M68K_EA_ABS_L) &&
        instr->dst.mode == NG_M68K_EA_DREG) {
        return instr->form == NG_M68K_FORM_ABS_TO_DREG &&
               instr->reg == instr->dst.reg &&
               instr->src_reg == 0u &&
               instr->immediate == 0u &&
               instr->absolute_addr == instr->src.absolute_addr &&
               instr->displacement == 0;
    }

    if (instr->src.mode == NG_M68K_EA_ADISP &&
        instr->dst.mode == NG_M68K_EA_DREG) {
        return instr->form == NG_M68K_FORM_AREG_DISP &&
               instr->reg == instr->dst.reg &&
               instr->src_reg == instr->src.reg &&
               instr->immediate == 0u &&
               instr->absolute_addr == 0u &&
               instr->displacement == instr->src.displacement;
    }

    if (instr->src.mode == NG_M68K_EA_IMM &&
        instr->dst.mode == NG_M68K_EA_DREG) {
        return instr->form == NG_M68K_FORM_IMM_TO_DREG &&
               instr->reg == instr->dst.reg &&
               instr->src_reg == 0u &&
               instr->immediate == instr->src.immediate &&
               instr->absolute_addr == 0u &&
               instr->displacement == 0;
    }

    if (instr->src.mode == NG_M68K_EA_DREG &&
        instr->dst.mode == NG_M68K_EA_DREG) {
        return instr->form == NG_M68K_FORM_DREG_TO_DREG &&
               instr->reg == instr->dst.reg &&
               instr->src_reg == instr->src.reg &&
               instr->immediate == 0u &&
               instr->absolute_addr == 0u &&
               instr->displacement == 0;
    }

    return instr->form == NG_M68K_FORM_NONE &&
           instr->reg == 0u &&
           instr->src_reg == 0u &&
           instr->immediate == 0u &&
           instr->absolute_addr == 0u &&
           instr->displacement == 0;
}

static int validate_move(const NgM68kInstr *instr) {
    uint8_t src_ext = 0u;
    uint8_t dst_ext = 0u;
    uint8_t src_field = 0u;
    uint8_t dst_field = 0u;
    uint16_t size_base = 0u;
    uint16_t expected_opcode = 0u;

    if (!valid_size(instr->size) ||
        instr->condition != 0u ||
        instr->target != 0u ||
        !move_source_ext_length(instr, &src_ext) ||
        !move_destination_ext_length(&instr->dst, &dst_ext) ||
        !ea_opcode_field(&instr->src, &src_field) ||
        !ea_opcode_field(&instr->dst, &dst_field) ||
        !validate_move_legacy_fields(instr)) {
        return 0;
    }

    if (instr->size == 1u) {
        size_base = 0x1000u;
    } else if (instr->size == 2u) {
        size_base = 0x3000u;
    } else {
        size_base = 0x2000u;
    }

    expected_opcode = (uint16_t)(size_base |
                                 ((uint16_t)(dst_field & 7u) << 9) |
                                 ((uint16_t)((dst_field >> 3) & 7u) << 6) |
                                 src_field);

    return instr->byte_length == (uint8_t)(2u + src_ext + dst_ext) &&
           instr->opcode == expected_opcode;
}

static int exact_control_ea_ext_length(const NgM68kEa *ea,
                                       uint32_t extension_addr,
                                       uint8_t *out_ext) {
    switch (ea->mode) {
    case NG_M68K_EA_AIND:
        if (!ea_simple_register_payload(ea)) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_ADISP:
        if (!ea_address_displacement_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_AINDEX:
        if (!ea_address_index_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_W:
        if (!ea_abs_word_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_L:
        if (!ea_abs_long_payload(ea)) {
            return 0;
        }
        *out_ext = 4u;
        return 1;
    case NG_M68K_EA_PC_DISP:
        if (!ea_pc_displacement_payload(ea, extension_addr)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_PC_INDEX:
        if (!ea_pc_index_payload(ea, extension_addr)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    default:
        return 0;
    }
}

static int exact_control_source_ext_length(const NgM68kInstr *instr,
                                           uint8_t *out_ext) {
    return exact_control_ea_ext_length(&instr->src, instr->addr + 2u, out_ext);
}

static int validate_control_transfer(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;
    uint8_t ea_field = 0u;
    uint16_t expected_opcode = 0u;

    if (instr->size != 0u ||
        instr->dst.mode != NG_M68K_EA_NONE ||
        instr->immediate != 0u ||
        instr->src_reg != 0u ||
        instr->condition != 0u ||
        !exact_control_source_ext_length(instr, &ext_len) ||
        !ea_opcode_field(&instr->src, &ea_field) ||
        instr->byte_length != (uint8_t)(2u + ext_len)) {
        return 0;
    }

    expected_opcode = (uint16_t)((instr->mnemonic == NG_M68K_JMP ?
                                  0x4EC0u : 0x4E80u) | ea_field);
    if (instr->opcode != expected_opcode) {
        return 0;
    }

    if (instr->src.mode == NG_M68K_EA_AIND) {
        return instr->form == NG_M68K_FORM_AREG_INDIRECT &&
               instr->reg == instr->src.reg;
    }

    if (instr->src.mode == NG_M68K_EA_ABS_W ||
        instr->src.mode == NG_M68K_EA_ABS_L) {
        return instr->form == NG_M68K_FORM_ABS &&
               instr->reg == 0u &&
               instr->target == instr->src.absolute_addr;
    }

    if (instr->src.mode == NG_M68K_EA_PC_DISP ||
        instr->src.mode == NG_M68K_EA_PC_INDEX) {
        return instr->form == NG_M68K_FORM_PC_RELATIVE &&
               instr->reg == 0u &&
               instr->target == instr->src.absolute_addr;
    }

    return instr->form == NG_M68K_FORM_NONE &&
           instr->reg == 0u;
}

static int validate_pea(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;
    uint8_t ea_field = 0u;

    return instr->size == 4u &&
           instr->dst.mode == NG_M68K_EA_NONE &&
           instr->immediate == 0u &&
           instr->src_reg == 0u &&
           instr->reg == 0u &&
           instr->condition == 0u &&
           instr->form == NG_M68K_FORM_NONE &&
           exact_control_source_ext_length(instr, &ext_len) &&
           ea_opcode_field(&instr->src, &ea_field) &&
           instr->byte_length == (uint8_t)(2u + ext_len) &&
           instr->opcode == (uint16_t)(0x4840u | ea_field);
}

static int validate_lea(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;
    uint8_t ea_field = 0u;

    return instr->size == 4u &&
           instr->dst.mode == NG_M68K_EA_AREG &&
           instr->dst.reg < 8u &&
           instr->reg == instr->dst.reg &&
           instr->immediate == 0u &&
           instr->src_reg == 0u &&
           instr->condition == 0u &&
           exact_control_source_ext_length(instr, &ext_len) &&
           ea_opcode_field(&instr->src, &ea_field) &&
           instr->byte_length == (uint8_t)(2u + ext_len) &&
           instr->opcode == (uint16_t)(0x41C0u |
                                       ((uint16_t)instr->dst.reg << 9) |
                                       ea_field);
}

static int validate_move_sr_ccr(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;
    uint8_t ea_field = 0u;
    uint16_t expected_opcode = 0u;

    if (instr->size != 2u ||
        instr->immediate != 0u ||
        instr->src_reg != 0u ||
        instr->reg != 0u ||
        instr->condition != 0u ||
        instr->form != NG_M68K_FORM_NONE ||
        instr->target != 0u ||
        instr->absolute_addr != 0u ||
        instr->displacement != 0) {
        return 0;
    }

    if (instr->dst.mode != NG_M68K_EA_NONE) {
        if (instr->mnemonic != NG_M68K_MOVE_SR ||
            instr->src.mode != NG_M68K_EA_NONE ||
            !move_destination_ext_length(&instr->dst, &ext_len)) {
            return 0;
        }
        if (!ea_opcode_field(&instr->dst, &ea_field)) {
            return 0;
        }
        expected_opcode = (uint16_t)(0x40C0u | ea_field);
        return instr->byte_length == (uint8_t)(2u + ext_len) &&
               instr->opcode == expected_opcode;
    }

    if (instr->src.mode == NG_M68K_EA_NONE) {
        return 0;
    }

    if (!exact_data_source_ext_length(instr, &ext_len)) {
        return 0;
    }
    if (!ea_opcode_field(&instr->src, &ea_field)) {
        return 0;
    }
    expected_opcode = (uint16_t)((instr->mnemonic == NG_M68K_MOVE_CCR ?
                                  0x44C0u : 0x46C0u) | ea_field);
    return instr->byte_length == (uint8_t)(2u + ext_len) &&
           instr->opcode == expected_opcode;
}

static int movem_reg_to_mem_ext_length(const NgM68kEa *ea,
                                       uint8_t *out_ext) {
    switch (ea->mode) {
    case NG_M68K_EA_AIND:
    case NG_M68K_EA_APRE:
        if (!ea_simple_register_payload(ea)) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    case NG_M68K_EA_ADISP:
        if (!ea_address_displacement_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_AINDEX:
        if (!ea_address_index_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_W:
        if (!ea_abs_word_payload(ea)) {
            return 0;
        }
        *out_ext = 2u;
        return 1;
    case NG_M68K_EA_ABS_L:
        if (!ea_abs_long_payload(ea)) {
            return 0;
        }
        *out_ext = 4u;
        return 1;
    default:
        return 0;
    }
}

static int movem_mem_to_reg_ext_length(const NgM68kInstr *instr,
                                       uint8_t *out_ext) {
    const NgM68kEa *ea = &instr->src;

    if (ea->mode == NG_M68K_EA_APOST) {
        if (!ea_simple_register_payload(ea)) {
            return 0;
        }
        *out_ext = 0u;
        return 1;
    }
    return exact_control_ea_ext_length(ea, instr->addr + 4u, out_ext);
}

static int validate_movem(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;
    uint8_t ea_field = 0u;
    uint16_t size_bit = 0u;
    uint16_t expected_opcode = 0u;

    if (!valid_word_or_long(instr->size) ||
        instr->immediate > 0xFFFFu ||
        instr->reg != 0u ||
        instr->src_reg != 0u ||
        instr->condition != 0u ||
        instr->form != NG_M68K_FORM_NONE ||
        instr->target != 0u ||
        instr->absolute_addr != 0u ||
        instr->displacement != 0) {
        return 0;
    }

    size_bit = instr->size == 4u ? 0x0040u : 0u;

    if (instr->src.mode == NG_M68K_EA_NONE &&
        instr->dst.mode != NG_M68K_EA_NONE) {
        if (!movem_reg_to_mem_ext_length(&instr->dst, &ext_len) ||
            !ea_opcode_field(&instr->dst, &ea_field) ||
            instr->byte_length != (uint8_t)(4u + ext_len)) {
            return 0;
        }
        expected_opcode = (uint16_t)(0x4880u | size_bit | ea_field);
        return instr->opcode == expected_opcode;
    }

    if (instr->dst.mode == NG_M68K_EA_NONE &&
        instr->src.mode != NG_M68K_EA_NONE) {
        if (!movem_mem_to_reg_ext_length(instr, &ext_len) ||
            !ea_opcode_field(&instr->src, &ea_field) ||
            instr->byte_length != (uint8_t)(4u + ext_len)) {
            return 0;
        }
        expected_opcode = (uint16_t)(0x4C80u | size_bit | ea_field);
        return instr->opcode == expected_opcode;
    }

    return 0;
}

static int validate_movep(const NgM68kInstr *instr) {
    if (instr->byte_length != 4u ||
        !valid_word_or_long(instr->size) ||
        instr->immediate != 0u ||
        instr->condition != 0u ||
        instr->form != NG_M68K_FORM_NONE ||
        instr->target != 0u ||
        instr->absolute_addr != 0u) {
        return 0;
    }

    if (instr->src.mode == NG_M68K_EA_DREG &&
        instr->dst.mode == NG_M68K_EA_ADISP) {
        uint16_t expected_opcode =
            (uint16_t)(0x0188u |
                       ((uint16_t)instr->src.reg << 9) |
                       ((instr->size == 4u) ? 0x0040u : 0u) |
                       instr->dst.reg);
        return instr->src.reg < 8u &&
               instr->dst.reg < 8u &&
               instr->src_reg == instr->src.reg &&
               instr->reg == instr->src.reg &&
               instr->displacement == instr->dst.displacement &&
               instr->opcode == expected_opcode;
    }

    if (instr->src.mode == NG_M68K_EA_ADISP &&
        instr->dst.mode == NG_M68K_EA_DREG) {
        uint16_t expected_opcode =
            (uint16_t)(0x0108u |
                       ((uint16_t)instr->dst.reg << 9) |
                       ((instr->size == 4u) ? 0x0040u : 0u) |
                       instr->src.reg);
        return instr->src.reg < 8u &&
               instr->dst.reg < 8u &&
               instr->src_reg == 0u &&
               instr->reg == instr->dst.reg &&
               instr->displacement == instr->src.displacement &&
               instr->opcode == expected_opcode;
    }

    return 0;
}

static int validate_link_unlk(const NgM68kInstr *instr) {
    if (instr->immediate != 0u ||
        instr->condition != 0u ||
        instr->form != NG_M68K_FORM_NONE ||
        instr->target != 0u ||
        instr->absolute_addr != 0u ||
        instr->src_reg != 0u ||
        instr->reg >= 8u ||
        !no_ea_fields(instr)) {
        return 0;
    }

    if (instr->mnemonic == NG_M68K_LINK) {
        return instr->opcode == (uint16_t)(0x4E50u | instr->reg) &&
               instr->byte_length == 4u &&
               instr->size == 2u;
    }

    return instr->opcode == (uint16_t)(0x4E58u | instr->reg) &&
           instr->byte_length == 2u &&
           instr->size == 0u &&
           instr->displacement == 0;
}

static int validate_exg(const NgM68kInstr *instr) {
    if (instr->byte_length != 2u ||
        instr->size != 4u ||
        instr->immediate != 0u ||
        instr->condition != 0u ||
        instr->form != NG_M68K_FORM_NONE ||
        instr->target != 0u ||
        instr->absolute_addr != 0u ||
        instr->displacement != 0 ||
        instr->src_reg != instr->src.reg ||
        instr->reg != instr->dst.reg) {
        return 0;
    }

    if (ea_is_exact_register(&instr->src, NG_M68K_EA_DREG) &&
        ea_is_exact_register(&instr->dst, NG_M68K_EA_DREG)) {
        uint16_t expected_opcode =
            (uint16_t)(0xC140u | ((uint16_t)instr->src.reg << 9) |
                       instr->dst.reg);
        return instr->opcode == expected_opcode;
    }
    if (ea_is_exact_register(&instr->src, NG_M68K_EA_AREG) &&
        ea_is_exact_register(&instr->dst, NG_M68K_EA_AREG)) {
        uint16_t expected_opcode =
            (uint16_t)(0xC148u | ((uint16_t)instr->src.reg << 9) |
                       instr->dst.reg);
        return instr->opcode == expected_opcode;
    }
    if (ea_is_exact_register(&instr->src, NG_M68K_EA_DREG) &&
        ea_is_exact_register(&instr->dst, NG_M68K_EA_AREG)) {
        uint16_t expected_opcode =
            (uint16_t)(0xC188u | ((uint16_t)instr->src.reg << 9) |
                       instr->dst.reg);
        return instr->opcode == expected_opcode;
    }
    return 0;
}

static int valid_sign_extended_byte(uint32_t value) {
    return value == (uint32_t)(int32_t)(int8_t)(uint8_t)value;
}

static int validate_moveq(const NgM68kInstr *instr) {
    uint16_t expected_opcode =
        (uint16_t)(0x7000u | ((uint16_t)instr->reg << 9) |
                   (uint8_t)instr->immediate);
    return instr->byte_length == 2u &&
           instr->size == 4u &&
           instr->form == NG_M68K_FORM_IMM_TO_DREG &&
           instr->reg < 8u &&
           instr->src_reg == 0u &&
           instr->condition == 0u &&
           instr->target == 0u &&
           instr->absolute_addr == 0u &&
           instr->displacement == 0 &&
           valid_sign_extended_byte(instr->immediate) &&
           ea_is_empty(&instr->src) &&
           ea_is_exact_register(&instr->dst, NG_M68K_EA_DREG) &&
           instr->reg == instr->dst.reg &&
           instr->opcode == expected_opcode;
}

static int validate_ext_swap(const NgM68kInstr *instr) {
    if (instr->byte_length != 2u ||
        instr->form != NG_M68K_FORM_DREG ||
        instr->immediate != 0u ||
        instr->src_reg != 0u ||
        instr->condition != 0u ||
        instr->target != 0u ||
        instr->absolute_addr != 0u ||
        instr->displacement != 0 ||
        !ea_is_empty(&instr->src) ||
        !ea_is_exact_register(&instr->dst, NG_M68K_EA_DREG) ||
        instr->reg != instr->dst.reg) {
        return 0;
    }

    if (instr->mnemonic == NG_M68K_EXT) {
        uint16_t expected_opcode =
            (uint16_t)(((instr->size == 4u) ? 0x48C0u : 0x4880u) |
                       instr->reg);
        return (instr->size == 2u || instr->size == 4u) &&
               instr->opcode == expected_opcode;
    }

    return instr->size == 2u &&
           instr->opcode == (uint16_t)(0x4840u | instr->reg);
}

static int ea_opcode_field(const NgM68kEa *ea, uint8_t *out_field) {
    switch (ea->mode) {
    case NG_M68K_EA_DREG:
        if (ea->reg >= 8u) return 0;
        *out_field = ea->reg;
        return 1;
    case NG_M68K_EA_AREG:
        if (ea->reg >= 8u) return 0;
        *out_field = (uint8_t)(0x08u | ea->reg);
        return 1;
    case NG_M68K_EA_AIND:
        if (ea->reg >= 8u) return 0;
        *out_field = (uint8_t)(0x10u | ea->reg);
        return 1;
    case NG_M68K_EA_APOST:
        if (ea->reg >= 8u) return 0;
        *out_field = (uint8_t)(0x18u | ea->reg);
        return 1;
    case NG_M68K_EA_APRE:
        if (ea->reg >= 8u) return 0;
        *out_field = (uint8_t)(0x20u | ea->reg);
        return 1;
    case NG_M68K_EA_ADISP:
        if (ea->reg >= 8u) return 0;
        *out_field = (uint8_t)(0x28u | ea->reg);
        return 1;
    case NG_M68K_EA_AINDEX:
        if (ea->reg >= 8u) return 0;
        *out_field = (uint8_t)(0x30u | ea->reg);
        return 1;
    case NG_M68K_EA_ABS_W:
        *out_field = 0x38u;
        return 1;
    case NG_M68K_EA_ABS_L:
        *out_field = 0x39u;
        return 1;
    case NG_M68K_EA_PC_DISP:
        *out_field = 0x3Au;
        return 1;
    case NG_M68K_EA_PC_INDEX:
        *out_field = 0x3Bu;
        return 1;
    case NG_M68K_EA_IMM:
        *out_field = 0x3Cu;
        return 1;
    default:
        return 0;
    }
}

static int validate_scc(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;
    uint8_t ea_field = 0u;

    return instr->size == 1u &&
           instr->condition <= 15u &&
           instr->immediate == 0u &&
           instr->reg == 0u &&
           instr->src_reg == 0u &&
           instr->form == NG_M68K_FORM_NONE &&
           instr->target == 0u &&
           instr->absolute_addr == 0u &&
           instr->displacement == 0 &&
           ea_is_empty(&instr->src) &&
           exact_data_alterable_ext_length(&instr->dst, &ext_len) &&
           ea_opcode_field(&instr->dst, &ea_field) &&
           instr->byte_length == (uint8_t)(2u + ext_len) &&
           instr->opcode == (uint16_t)(0x50C0u |
                                       ((uint16_t)instr->condition << 8) |
                                       ea_field);
}

static int validate_dbcc(const NgM68kInstr *instr) {
    uint32_t expected_target =
        (uint32_t)((int64_t)instr->addr + 2 + (int64_t)instr->displacement);

    return instr->byte_length == 4u &&
           instr->size == 2u &&
           instr->condition <= 15u &&
           instr->reg < 8u &&
           instr->immediate == 0u &&
           instr->src_reg == 0u &&
           instr->form == NG_M68K_FORM_NONE &&
           instr->absolute_addr == 0u &&
           no_ea_fields(instr) &&
           instr->target == expected_target &&
           instr->opcode == (uint16_t)(0x50C8u |
                                       ((uint16_t)instr->condition << 8) |
                                       instr->reg);
}

static int validate_ea_to_dreg_binary(const NgM68kInstr *instr,
                                      int allow_areg_source) {
    uint8_t ext_len = 0u;
    uint8_t ea_field = 0u;
    uint16_t base_opcode = 0u;
    uint16_t size_bits = 0u;
    uint16_t expected_opcode = 0u;
    int valid_src = allow_areg_source ?
        exact_addsubcmp_source_ext_length(instr, &ext_len) :
        exact_data_source_ext_length(instr, &ext_len);
    int alu_form =
        instr->mnemonic == NG_M68K_ADD ||
        instr->mnemonic == NG_M68K_SUB ||
        instr->mnemonic == NG_M68K_CMP;

    switch (instr->mnemonic) {
    case NG_M68K_OR:
        base_opcode = 0x8000u;
        break;
    case NG_M68K_SUB:
        base_opcode = 0x9000u;
        break;
    case NG_M68K_CMP:
        base_opcode = 0xB000u;
        break;
    case NG_M68K_AND:
        base_opcode = 0xC000u;
        break;
    case NG_M68K_ADD:
        base_opcode = 0xD000u;
        break;
    default:
        return 0;
    }

    if (instr->size == 1u) {
        size_bits = 0x0000u;
    } else if (instr->size == 2u) {
        size_bits = 0x0040u;
    } else if (instr->size == 4u) {
        size_bits = 0x0080u;
    } else {
        return 0;
    }

    if (instr->immediate != 0u ||
        instr->condition != 0u ||
        instr->target != 0u ||
        instr->absolute_addr != 0u ||
        instr->displacement != 0 ||
        instr->src_reg != instr->src.reg ||
        !valid_src ||
        !ea_opcode_field(&instr->src, &ea_field) ||
        instr->dst.mode != NG_M68K_EA_DREG ||
        !ea_simple_register_payload(&instr->dst) ||
        instr->reg != instr->dst.reg ||
        instr->form != (alu_form ? NG_M68K_FORM_DREG_TO_DREG :
                                  NG_M68K_FORM_NONE) ||
        instr->byte_length != (uint8_t)(2u + ext_len)) {
        return 0;
    }

    expected_opcode = (uint16_t)(base_opcode |
                                 ((uint16_t)instr->dst.reg << 9) |
                                 size_bits |
                                 ea_field);
    return instr->opcode == expected_opcode;
}

static int validate_binary_destination_legacy_fields(const NgM68kInstr *instr,
                                                     int allow_dreg_dst) {
    if (instr->condition != 0u ||
        instr->target != 0u) {
        return 0;
    }

    switch (instr->dst.mode) {
    case NG_M68K_EA_DREG:
        return allow_dreg_dst &&
               ea_simple_register_payload(&instr->dst) &&
               instr->form == NG_M68K_FORM_DREG_TO_DREG &&
               instr->reg == instr->dst.reg &&
               instr->absolute_addr == 0u &&
               instr->displacement == 0;
    case NG_M68K_EA_ADISP:
        return instr->form == NG_M68K_FORM_AREG_DISP &&
               instr->reg == instr->dst.reg &&
               instr->absolute_addr == 0u &&
               instr->displacement == instr->dst.displacement;
    case NG_M68K_EA_ABS_W:
    case NG_M68K_EA_ABS_L:
        return instr->form == NG_M68K_FORM_ABS &&
               instr->reg == 0u &&
               instr->absolute_addr == instr->dst.absolute_addr &&
               instr->displacement == 0;
    default:
        return instr->form == NG_M68K_FORM_NONE &&
               instr->reg == 0u &&
               instr->absolute_addr == 0u &&
               instr->displacement == 0;
    }
}

static int validate_dreg_to_memory_binary(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;
    uint8_t ea_field = 0u;
    uint16_t base_opcode = 0u;
    uint16_t size_bits = 0u;
    uint16_t expected_opcode = 0u;

    switch (instr->mnemonic) {
    case NG_M68K_OR:
        base_opcode = 0x8000u;
        break;
    case NG_M68K_SUB:
        base_opcode = 0x9000u;
        break;
    case NG_M68K_AND:
        base_opcode = 0xC000u;
        break;
    case NG_M68K_ADD:
        base_opcode = 0xD000u;
        break;
    default:
        return 0;
    }

    if (instr->size == 1u) {
        size_bits = 0x0000u;
    } else if (instr->size == 2u) {
        size_bits = 0x0040u;
    } else if (instr->size == 4u) {
        size_bits = 0x0080u;
    } else {
        return 0;
    }

    if (instr->immediate != 0u ||
        instr->src.mode != NG_M68K_EA_DREG ||
        !ea_simple_register_payload(&instr->src) ||
        instr->src_reg != instr->src.reg ||
        !exact_memory_alterable_ext_length(&instr->dst, &ext_len) ||
        !ea_opcode_field(&instr->dst, &ea_field) ||
        !validate_binary_destination_legacy_fields(instr, 0) ||
        instr->byte_length != (uint8_t)(2u + ext_len)) {
        return 0;
    }

    expected_opcode = (uint16_t)(base_opcode |
                                 ((uint16_t)instr->src.reg << 9) |
                                 0x0100u |
                                 size_bits |
                                 ea_field);
    return instr->opcode == expected_opcode;
}

static int validate_dreg_to_data_alterable_binary(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;
    uint8_t ea_field = 0u;
    uint16_t size_bits = 0u;
    uint16_t expected_opcode = 0u;

    if (instr->mnemonic != NG_M68K_EOR) {
        return 0;
    }

    if (instr->size == 1u) {
        size_bits = 0x0000u;
    } else if (instr->size == 2u) {
        size_bits = 0x0040u;
    } else if (instr->size == 4u) {
        size_bits = 0x0080u;
    } else {
        return 0;
    }

    if (instr->immediate != 0u ||
        instr->src.mode != NG_M68K_EA_DREG ||
        !ea_simple_register_payload(&instr->src) ||
        instr->src_reg != instr->src.reg ||
        !exact_data_alterable_ext_length(&instr->dst, &ext_len) ||
        !ea_opcode_field(&instr->dst, &ea_field) ||
        !validate_binary_destination_legacy_fields(instr, 1) ||
        instr->byte_length != (uint8_t)(2u + ext_len)) {
        return 0;
    }

    expected_opcode = (uint16_t)(0xB000u |
                                 ((uint16_t)instr->src.reg << 9) |
                                 0x0100u |
                                 size_bits |
                                 ea_field);
    return instr->opcode == expected_opcode;
}

static int validate_cmpm(const NgM68kInstr *instr) {
    uint16_t size_bits = 0u;
    uint16_t expected_opcode = 0u;

    if (instr->size == 1u) {
        size_bits = 0x0000u;
    } else if (instr->size == 2u) {
        size_bits = 0x0040u;
    } else if (instr->size == 4u) {
        size_bits = 0x0080u;
    } else {
        return 0;
    }

    if (instr->byte_length != 2u ||
        instr->immediate != 0u ||
        instr->condition != 0u ||
        instr->form != NG_M68K_FORM_NONE ||
        instr->target != 0u ||
        instr->absolute_addr != 0u ||
        instr->displacement != 0 ||
        instr->src.mode != NG_M68K_EA_APOST ||
        !ea_simple_register_payload(&instr->src) ||
        instr->src_reg != instr->src.reg ||
        instr->dst.mode != NG_M68K_EA_APOST ||
        !ea_simple_register_payload(&instr->dst) ||
        instr->reg != instr->dst.reg) {
        return 0;
    }

    expected_opcode = (uint16_t)(0xB000u |
                                 ((uint16_t)instr->dst.reg << 9) |
                                 0x0100u |
                                 size_bits |
                                 0x0008u |
                                 instr->src.reg);
    return instr->opcode == expected_opcode;
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

static int valid_immediate_width(uint8_t size, uint32_t immediate) {
    if (size == 1u) {
        return immediate <= 0xFFu;
    }
    if (size == 2u) {
        return immediate <= 0xFFFFu;
    }
    return size == 4u;
}

static int validate_address_reg_op_legacy_fields(const NgM68kInstr *instr) {
    if (instr->reg != instr->dst.reg ||
        instr->immediate != 0u ||
        instr->absolute_addr != 0u ||
        instr->condition != 0u ||
        !ea_simple_register_payload(&instr->dst)) {
        return 0;
    }

    if (instr->mnemonic == NG_M68K_MOVEA &&
        instr->src.mode == NG_M68K_EA_PC_INDEX) {
        return instr->form == NG_M68K_FORM_PC_INDEX_TO_AREG &&
               instr->src_reg == instr->src.index_reg &&
               instr->displacement == instr->src.displacement &&
               instr->target == instr->src.absolute_addr;
    }

    return instr->form == NG_M68K_FORM_NONE &&
           instr->src_reg == 0u &&
           instr->displacement == 0 &&
           instr->target == 0u;
}

static int validate_address_reg_op(const NgM68kInstr *instr) {
    uint8_t src_ext = 0u;
    uint8_t ea_field = 0u;
    uint16_t expected_opcode = 0u;

    if (!valid_word_or_long(instr->size) ||
        !move_source_ext_length(instr, &src_ext) ||
        !ea_opcode_field(&instr->src, &ea_field) ||
        instr->dst.mode != NG_M68K_EA_AREG ||
        !validate_address_reg_op_legacy_fields(instr) ||
        instr->byte_length != (uint8_t)(2u + src_ext)) {
        return 0;
    }

    if (instr->mnemonic == NG_M68K_MOVEA) {
        uint16_t size_base = instr->size == 2u ? 0x3000u : 0x2000u;
        expected_opcode = (uint16_t)(size_base |
                                     ((uint16_t)instr->dst.reg << 9) |
                                     0x0040u |
                                     ea_field);
        return instr->opcode == expected_opcode;
    }

    {
        uint16_t base_opcode = 0u;
        uint16_t opmode_bits = instr->size == 2u ? 0x00C0u : 0x01C0u;

        switch (instr->mnemonic) {
        case NG_M68K_ADDA:
            base_opcode = 0xD000u;
            break;
        case NG_M68K_SUBA:
            base_opcode = 0x9000u;
            break;
        case NG_M68K_CMPA:
            base_opcode = 0xB000u;
            break;
        default:
            return 0;
        }

        expected_opcode = (uint16_t)(base_opcode |
                                     ((uint16_t)instr->dst.reg << 9) |
                                     opmode_bits |
                                     ea_field);
        return instr->opcode == expected_opcode;
    }
}

static int validate_tst_legacy_fields(const NgM68kInstr *instr) {
    if (instr->target != 0u ||
        instr->condition != 0u ||
        instr->src_reg != 0u ||
        instr->immediate != 0u ||
        instr->dst.mode != NG_M68K_EA_NONE) {
        return 0;
    }

    if (instr->src.mode == NG_M68K_EA_DREG) {
        return instr->form == NG_M68K_FORM_DREG &&
               instr->reg == instr->src.reg &&
               instr->absolute_addr == 0u &&
               instr->displacement == 0;
    }

    if (instr->src.mode == NG_M68K_EA_ABS_W ||
        instr->src.mode == NG_M68K_EA_ABS_L) {
        return instr->form == NG_M68K_FORM_ABS &&
               instr->reg == 0u &&
               instr->absolute_addr == instr->src.absolute_addr &&
               instr->displacement == 0;
    }

    if (instr->src.mode == NG_M68K_EA_ADISP) {
        return instr->form == NG_M68K_FORM_AREG_DISP &&
               instr->reg == instr->src.reg &&
               instr->absolute_addr == 0u &&
               instr->displacement == instr->src.displacement;
    }

    return instr->form == NG_M68K_FORM_NONE &&
           instr->reg == 0u &&
           instr->absolute_addr == 0u &&
           instr->displacement == 0;
}

static int validate_tst(const NgM68kInstr *instr) {
    uint8_t src_ext = 0u;
    uint8_t ea_field = 0u;
    uint16_t size_bits = 0u;
    uint16_t expected_opcode = 0u;

    if (instr->size == 1u) {
        size_bits = 0x0000u;
    } else if (instr->size == 2u) {
        size_bits = 0x0040u;
    } else if (instr->size == 4u) {
        size_bits = 0x0080u;
    } else {
        return 0;
    }

    if (!exact_data_source_ext_length(instr, &src_ext) ||
        !ea_opcode_field(&instr->src, &ea_field) ||
        !ea_is_data_alterable(&instr->src) ||
        !validate_tst_legacy_fields(instr) ||
        instr->byte_length != (uint8_t)(2u + src_ext)) {
        return 0;
    }

    expected_opcode = (uint16_t)(0x4A00u | size_bits | ea_field);
    return instr->opcode == expected_opcode;
}

static int validate_immediate_dest_legacy_fields(const NgM68kInstr *instr) {
    if (instr->src_reg != 0u ||
        instr->condition != 0u ||
        instr->target != 0u ||
        instr->src.mode != NG_M68K_EA_NONE) {
        return 0;
    }

    if (instr->dst.mode == NG_M68K_EA_DREG) {
        return instr->form == NG_M68K_FORM_IMM_TO_DREG &&
               instr->reg == instr->dst.reg &&
               instr->absolute_addr == 0u &&
               instr->displacement == 0;
    }

    if (instr->dst.mode == NG_M68K_EA_ABS_W ||
        instr->dst.mode == NG_M68K_EA_ABS_L) {
        return instr->form == NG_M68K_FORM_ABS &&
               instr->reg == 0u &&
               instr->absolute_addr == instr->dst.absolute_addr &&
               instr->displacement == 0;
    }

    if (instr->dst.mode == NG_M68K_EA_ADISP) {
        return instr->form == NG_M68K_FORM_AREG_DISP &&
               instr->reg == instr->dst.reg &&
               instr->absolute_addr == 0u &&
               instr->displacement == instr->dst.displacement;
    }

    return instr->form == NG_M68K_FORM_NONE &&
           instr->reg == 0u &&
           instr->absolute_addr == 0u &&
           instr->displacement == 0;
}

static int validate_cmpi(const NgM68kInstr *instr) {
    uint8_t dst_ext = 0u;
    uint8_t imm_ext = instr->size == 4u ? 4u : 2u;
    uint8_t ea_field = 0u;
    uint16_t size_bits = 0u;
    uint16_t expected_opcode = 0u;

    if (instr->size == 1u) {
        size_bits = 0x0000u;
    } else if (instr->size == 2u) {
        size_bits = 0x0040u;
    } else if (instr->size == 4u) {
        size_bits = 0x0080u;
    } else {
        return 0;
    }

    if (!exact_data_alterable_ext_length(&instr->dst, &dst_ext) ||
        !ea_opcode_field(&instr->dst, &ea_field) ||
        !valid_immediate_width(instr->size, instr->immediate) ||
        !validate_immediate_dest_legacy_fields(instr) ||
        instr->byte_length != (uint8_t)(2u + imm_ext + dst_ext)) {
        return 0;
    }

    expected_opcode = (uint16_t)(0x0C00u | size_bits | ea_field);
    return instr->opcode == expected_opcode;
}

static int validate_immediate_to_ea(const NgM68kInstr *instr) {
    uint8_t dst_ext = 0u;
    uint8_t imm_ext = instr->size == 4u ? 4u : 2u;
    uint8_t ea_field = 0u;
    uint16_t base_opcode = 0u;
    uint16_t size_bits = 0u;
    uint16_t expected_opcode = 0u;

    switch (instr->mnemonic) {
    case NG_M68K_ORI:
        base_opcode = 0x0000u;
        break;
    case NG_M68K_ANDI:
        base_opcode = 0x0200u;
        break;
    case NG_M68K_SUBI:
        base_opcode = 0x0400u;
        break;
    case NG_M68K_ADDI:
        base_opcode = 0x0600u;
        break;
    case NG_M68K_EORI:
        base_opcode = 0x0A00u;
        break;
    default:
        return 0;
    }

    if (instr->size == 1u) {
        size_bits = 0x0000u;
    } else if (instr->size == 2u) {
        size_bits = 0x0040u;
    } else if (instr->size == 4u) {
        size_bits = 0x0080u;
    } else {
        return 0;
    }

    if (!exact_data_alterable_ext_length(&instr->dst, &dst_ext) ||
        !ea_opcode_field(&instr->dst, &ea_field) ||
        !valid_immediate_width(instr->size, instr->immediate) ||
        !validate_immediate_dest_legacy_fields(instr) ||
        instr->byte_length != (uint8_t)(2u + imm_ext + dst_ext)) {
        return 0;
    }

    expected_opcode = (uint16_t)(base_opcode | size_bits | ea_field);
    return instr->opcode == expected_opcode;
}

static int validate_word_data_to_dreg(const NgM68kInstr *instr) {
    uint8_t src_ext = 0u;
    uint8_t ea_field = 0u;
    uint16_t base_opcode = 0u;
    uint16_t expected_opcode = 0u;

    switch (instr->mnemonic) {
    case NG_M68K_CHK:
        base_opcode = 0x4180u;
        break;
    case NG_M68K_MULU:
        base_opcode = 0xC0C0u;
        break;
    case NG_M68K_MULS:
        base_opcode = 0xC1C0u;
        break;
    case NG_M68K_DIVU:
        base_opcode = 0x80C0u;
        break;
    case NG_M68K_DIVS:
        base_opcode = 0x81C0u;
        break;
    default:
        return 0;
    }

    if (instr->size != 2u ||
        !exact_data_source_ext_length(instr, &src_ext) ||
        !ea_opcode_field(&instr->src, &ea_field) ||
        instr->immediate != 0u ||
        instr->src_reg != 0u ||
        instr->condition != 0u ||
        instr->form != NG_M68K_FORM_NONE ||
        instr->target != 0u ||
        instr->absolute_addr != 0u ||
        instr->displacement != 0 ||
        instr->dst.mode != NG_M68K_EA_DREG ||
        !ea_simple_register_payload(&instr->dst) ||
        instr->reg != instr->dst.reg ||
        instr->byte_length != (uint8_t)(2u + src_ext)) {
        return 0;
    }

    expected_opcode = (uint16_t)(base_opcode |
                                 ((uint16_t)instr->dst.reg << 9) |
                                 ea_field);
    return instr->opcode == expected_opcode;
}

static int validate_immediate_to_ccr_sr(const NgM68kInstr *instr) {
    uint16_t expected_opcode = 0u;

    if (instr->byte_length != 4u || !valid_control_immediate_fields(instr)) {
        return 0;
    }

    switch (instr->mnemonic) {
    case NG_M68K_ORI_TO_CCR:
        expected_opcode = 0x003Cu;
        break;
    case NG_M68K_ANDI_TO_CCR:
        expected_opcode = 0x023Cu;
        break;
    case NG_M68K_EORI_TO_CCR:
        expected_opcode = 0x0A3Cu;
        break;
    case NG_M68K_ORI_TO_SR:
        expected_opcode = 0x007Cu;
        break;
    case NG_M68K_ANDI_TO_SR:
        expected_opcode = 0x027Cu;
        break;
    case NG_M68K_EORI_TO_SR:
        expected_opcode = 0x0A7Cu;
        break;
    default:
        return 0;
    }

    if (instr->opcode != expected_opcode) {
        return 0;
    }

    switch (instr->mnemonic) {
    case NG_M68K_ORI_TO_CCR:
    case NG_M68K_ANDI_TO_CCR:
    case NG_M68K_EORI_TO_CCR:
        return instr->size == 1u && instr->immediate <= 0xFFu;
    default:
        return instr->size == 2u && instr->immediate <= 0xFFFFu;
    }
}

static int validate_branch(const NgM68kInstr *instr) {
    uint8_t disp8 = (uint8_t)(instr->opcode & 0xFFu);
    uint32_t expected_target =
        (uint32_t)((int64_t)instr->addr + 2 + (int64_t)instr->displacement);

    if (instr->size != 0u ||
        instr->immediate != 0u ||
        instr->reg != 0u ||
        instr->src_reg != 0u ||
        instr->form != NG_M68K_FORM_NONE ||
        instr->absolute_addr != 0u ||
        !no_ea_fields(instr) ||
        instr->target != expected_target) {
        return 0;
    }

    if (instr->byte_length == 2u) {
        if (disp8 == 0u ||
            instr->displacement != (int16_t)(int8_t)disp8) {
            return 0;
        }
    } else if (instr->byte_length == 4u) {
        if (disp8 != 0u) {
            return 0;
        }
    } else {
        return 0;
    }

    if (instr->mnemonic == NG_M68K_BCC) {
        return (instr->opcode & 0xF000u) == 0x6000u &&
               instr->condition == (uint8_t)((instr->opcode >> 8) & 0xFu) &&
               instr->condition >= 2u &&
               instr->condition <= 15u;
    }

    if (instr->mnemonic == NG_M68K_BRA) {
        return (instr->opcode & 0xFF00u) == 0x6000u &&
               instr->condition == 0u;
    }
    return (instr->opcode & 0xFF00u) == 0x6100u &&
           instr->condition == 0u;
}

static int validate_shift_rotate(const NgM68kInstr *instr) {
    uint8_t ext_len = 0u;
    uint8_t kind = 0u;
    uint8_t dir_left = 0u;
    uint16_t expected_opcode = 0u;

    switch (instr->mnemonic) {
    case NG_M68K_ASR: kind = 0u; dir_left = 0u; break;
    case NG_M68K_ASL: kind = 0u; dir_left = 1u; break;
    case NG_M68K_LSR: kind = 1u; dir_left = 0u; break;
    case NG_M68K_LSL: kind = 1u; dir_left = 1u; break;
    case NG_M68K_ROXR: kind = 2u; dir_left = 0u; break;
    case NG_M68K_ROXL: kind = 2u; dir_left = 1u; break;
    case NG_M68K_ROR: kind = 3u; dir_left = 0u; break;
    case NG_M68K_ROL: kind = 3u; dir_left = 1u; break;
    default: return 0;
    }

    if (instr->dst.mode == NG_M68K_EA_DREG) {
        uint8_t size_code = 0u;
        uint8_t count_field = 0u;

        if (!valid_size(instr->size) ||
            instr->byte_length != 2u ||
            !ea_simple_register_payload(&instr->dst) ||
            instr->reg != instr->dst.reg ||
            instr->condition != 0u ||
            instr->form != NG_M68K_FORM_NONE ||
            instr->target != 0u ||
            instr->absolute_addr != 0u ||
            instr->displacement != 0) {
            return 0;
        }
        size_code = (instr->size == 1u) ? 0u :
                    ((instr->size == 2u) ? 1u : 2u);
        if (instr->src.mode != NG_M68K_EA_NONE) {
            if (instr->src.mode != NG_M68K_EA_DREG ||
                !ea_simple_register_payload(&instr->src) ||
                instr->src_reg != instr->src.reg ||
                instr->immediate != 0u) {
                return 0;
            }
            count_field = instr->src.reg;
            expected_opcode = (uint16_t)(0xE000u |
                                         ((uint16_t)count_field << 9) |
                                         ((uint16_t)dir_left << 8) |
                                         ((uint16_t)size_code << 6) |
                                         0x0020u |
                                         ((uint16_t)kind << 3) |
                                         instr->dst.reg);
            return instr->opcode == expected_opcode;
        }
        if (!ea_is_empty(&instr->src) ||
            instr->src_reg != 0u ||
            instr->immediate < 1u ||
            instr->immediate > 8u) {
            return 0;
        }
        count_field = (instr->immediate == 8u) ? 0u : (uint8_t)instr->immediate;
        expected_opcode = (uint16_t)(0xE000u |
                                     ((uint16_t)count_field << 9) |
                                     ((uint16_t)dir_left << 8) |
                                     ((uint16_t)size_code << 6) |
                                     ((uint16_t)kind << 3) |
                                     instr->dst.reg);
        return instr->opcode == expected_opcode;
    }

    {
        uint8_t ea_field = 0u;
        if (!ea_is_empty(&instr->src) ||
            instr->size != 2u ||
            instr->immediate != 1u ||
            instr->src_reg != 0u ||
            instr->reg != 0u ||
            instr->condition != 0u ||
            instr->form != NG_M68K_FORM_NONE ||
            instr->target != 0u ||
            instr->absolute_addr != 0u ||
            instr->displacement != 0 ||
            !exact_memory_alterable_ext_length(&instr->dst, &ext_len) ||
            !ea_opcode_field(&instr->dst, &ea_field) ||
            instr->byte_length != (uint8_t)(2u + ext_len)) {
            return 0;
        }
        expected_opcode = (uint16_t)(0xE0C0u |
                                     ((uint16_t)kind << 9) |
                                     ((uint16_t)dir_left << 8) |
                                     ea_field);
        return instr->opcode == expected_opcode;
    }
}

static int validate_unary_data_alterable(const NgM68kInstr *instr,
                                         int byte_only) {
    uint8_t ext_len = 0u;
    uint8_t ea_field = 0u;
    uint16_t expected_opcode = 0u;
    uint16_t size_bits = 0u;

    if (!ea_is_empty(&instr->src) ||
        instr->immediate != 0u ||
        instr->src_reg != 0u ||
        instr->condition != 0u ||
        instr->target != 0u ||
        !exact_data_alterable_ext_length(&instr->dst, &ext_len)) {
        return 0;
    }

    if (byte_only) {
        if (instr->size != 1u) {
            return 0;
        }
    } else if (!valid_size(instr->size)) {
        return 0;
    }

    if (instr->byte_length != (uint8_t)(2u + ext_len)) {
        return 0;
    }

    if (!ea_opcode_field(&instr->dst, &ea_field)) {
        return 0;
    }

    if (!byte_only) {
        size_bits = (instr->size == 1u) ? 0u :
                    ((instr->size == 2u) ? 0x0040u : 0x0080u);
    }

    switch (instr->mnemonic) {
    case NG_M68K_NEGX:
        expected_opcode = (uint16_t)(0x4000u | size_bits | ea_field);
        break;
    case NG_M68K_CLR:
        expected_opcode = (uint16_t)(0x4200u | size_bits | ea_field);
        break;
    case NG_M68K_NEG:
        expected_opcode = (uint16_t)(0x4400u | size_bits | ea_field);
        break;
    case NG_M68K_NOT:
        expected_opcode = (uint16_t)(0x4600u | size_bits | ea_field);
        break;
    case NG_M68K_NBCD:
        expected_opcode = (uint16_t)(0x4800u | ea_field);
        break;
    case NG_M68K_TAS:
        expected_opcode = (uint16_t)(0x4AC0u | ea_field);
        break;
    default:
        return 0;
    }
    if (instr->opcode != expected_opcode) {
        return 0;
    }

    if (instr->dst.mode == NG_M68K_EA_DREG) {
        return instr->form == NG_M68K_FORM_DREG &&
               instr->reg == instr->dst.reg &&
               instr->absolute_addr == 0u &&
               instr->displacement == 0;
    }

    if (instr->dst.mode == NG_M68K_EA_ADISP) {
        return instr->form == NG_M68K_FORM_AREG_DISP &&
               instr->reg == instr->dst.reg &&
               instr->absolute_addr == 0u &&
               instr->displacement == instr->dst.displacement;
    }

    if (instr->dst.mode == NG_M68K_EA_ABS_W ||
        instr->dst.mode == NG_M68K_EA_ABS_L) {
        return instr->form == NG_M68K_FORM_ABS &&
               instr->reg == 0u &&
               instr->absolute_addr == instr->dst.absolute_addr &&
               instr->displacement == 0;
    }

    return instr->form == NG_M68K_FORM_NONE &&
           instr->reg == 0u &&
           instr->absolute_addr == 0u &&
           instr->displacement == 0;
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
        return valid_fixed_no_operand_2byte(instr, 0x4E71u);
    case NG_M68K_RESET:
        return valid_fixed_no_operand_2byte(instr, 0x4E70u);
    case NG_M68K_RTE:
        return valid_fixed_no_operand_2byte(instr, 0x4E73u);
    case NG_M68K_RTR:
        return valid_fixed_no_operand_2byte(instr, 0x4E77u);
    case NG_M68K_RTS:
        return valid_fixed_no_operand_2byte(instr, 0x4E75u);
    case NG_M68K_TRAPV:
        return valid_fixed_no_operand_2byte(instr, 0x4E76u);
    case NG_M68K_ILLEGAL:
        return validate_illegal_opcode_metadata(instr);
    case NG_M68K_STOP:
        return validate_stop_metadata(instr);
    case NG_M68K_TRAP:
        return validate_trap_metadata(instr);
    case NG_M68K_BRA:
    case NG_M68K_BSR:
    case NG_M68K_BCC:
        return validate_branch(instr);
    case NG_M68K_JMP:
    case NG_M68K_JSR:
        return validate_control_transfer(instr);
    case NG_M68K_PEA:
        return validate_pea(instr);
    case NG_M68K_LEA:
        return validate_lea(instr);
    case NG_M68K_MOVE:
        return validate_move(instr);
    case NG_M68K_MOVEA:
        return validate_address_reg_op(instr);
    case NG_M68K_ADDA:
    case NG_M68K_SUBA:
    case NG_M68K_CMPA:
        return validate_address_reg_op(instr);
    case NG_M68K_EXG:
        return validate_exg(instr);
    case NG_M68K_MOVE_USP:
        return validate_move_usp(instr);
    case NG_M68K_LINK:
    case NG_M68K_UNLK:
        return validate_link_unlk(instr);
    case NG_M68K_MOVEP:
        return validate_movep(instr);
    case NG_M68K_MOVEM:
        return validate_movem(instr);
    case NG_M68K_MOVE_SR:
    case NG_M68K_MOVE_CCR:
        return validate_move_sr_ccr(instr);
    case NG_M68K_CLR:
    case NG_M68K_NEG:
    case NG_M68K_NEGX:
    case NG_M68K_NOT:
        return validate_unary_data_alterable(instr, 0);
    case NG_M68K_NBCD:
    case NG_M68K_TAS:
        return validate_unary_data_alterable(instr, 1);
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
        return validate_cmpm(instr);
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
    case NG_M68K_SWAP:
        return validate_ext_swap(instr);
    case NG_M68K_SCC:
        return validate_scc(instr);
    case NG_M68K_DBCC:
        return validate_dbcc(instr);
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
        return validate_moveq(instr);
    default:
        return 1;
    }
}
