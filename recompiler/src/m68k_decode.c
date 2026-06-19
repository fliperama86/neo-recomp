#include "m68k_decode.h"

#include <stdio.h>
#include <string.h>

#define NG_M68K_SIZE_BYTE 1u
#define NG_M68K_SIZE_WORD 2u
#define NG_M68K_SIZE_LONG 4u

static int8_t sign8(uint8_t value) {
    return (int8_t)value;
}

static int16_t sign16(uint16_t value) {
    return (int16_t)value;
}

static uint32_t sign_extend_abs_w(uint16_t value) {
    return (uint32_t)(int32_t)(int16_t)value;
}

static void populate_branch_target(const NgProgramRom *rom,
                                   uint32_t addr,
                                   uint16_t opcode,
                                   NgM68kInstr *out) {
    uint8_t disp8 = (uint8_t)(opcode & 0xFFu);
    if (disp8 == 0) {
        out->byte_length = 4;
        out->displacement = sign16(ng_program_rom_read16(rom, addr + 2u));
    } else {
        out->byte_length = 2;
        out->displacement = sign8(disp8);
    }
    out->target = (uint32_t)((int64_t)addr + 2 + (int64_t)out->displacement);
}

static uint32_t decode_ea(const NgProgramRom *rom,
                          uint32_t ext_addr,
                          uint8_t mode,
                          uint8_t reg,
                          uint8_t size,
                          NgM68kEa *out) {
    memset(out, 0, sizeof(*out));
    out->reg = reg;

    switch (mode) {
    case 0:
        out->mode = NG_M68K_EA_DREG;
        return 0;
    case 1:
        out->mode = NG_M68K_EA_AREG;
        return 0;
    case 2:
        out->mode = NG_M68K_EA_AIND;
        return 0;
    case 3:
        out->mode = NG_M68K_EA_APOST;
        return 0;
    case 4:
        out->mode = NG_M68K_EA_APRE;
        return 0;
    case 5:
        out->mode = NG_M68K_EA_ADISP;
        out->displacement = sign16(ng_program_rom_read16(rom, ext_addr));
        return 2;
    case 6: {
        uint16_t ext = ng_program_rom_read16(rom, ext_addr);
        out->mode = NG_M68K_EA_AINDEX;
        out->index_is_addr = (uint8_t)((ext >> 15) & 1u);
        out->index_reg = (uint8_t)((ext >> 12) & 7u);
        out->index_is_long = (uint8_t)((ext >> 11) & 1u);
        out->displacement = sign8((uint8_t)(ext & 0xFFu));
        return 2;
    }
    case 7:
        switch (reg) {
        case 0:
            out->mode = NG_M68K_EA_ABS_W;
            out->absolute_addr = sign_extend_abs_w(ng_program_rom_read16(rom, ext_addr));
            return 2;
        case 1:
            out->mode = NG_M68K_EA_ABS_L;
            out->absolute_addr = ng_program_rom_read32(rom, ext_addr);
            return 4;
        case 2:
            out->mode = NG_M68K_EA_PC_DISP;
            out->displacement = sign16(ng_program_rom_read16(rom, ext_addr));
            out->absolute_addr = (uint32_t)((int32_t)ext_addr + (int32_t)out->displacement);
            return 2;
        case 3: {
            uint16_t ext = ng_program_rom_read16(rom, ext_addr);
            out->mode = NG_M68K_EA_PC_INDEX;
            out->index_is_addr = (uint8_t)((ext >> 15) & 1u);
            out->index_reg = (uint8_t)((ext >> 12) & 7u);
            out->index_is_long = (uint8_t)((ext >> 11) & 1u);
            out->displacement = sign8((uint8_t)(ext & 0xFFu));
            out->absolute_addr = (uint32_t)((int32_t)ext_addr + (int32_t)out->displacement);
            return 2;
        }
        case 4:
            out->mode = NG_M68K_EA_IMM;
            if (size == NG_M68K_SIZE_LONG) {
                out->immediate = ng_program_rom_read32(rom, ext_addr);
                return 4;
            }
            out->immediate = ng_program_rom_read16(rom, ext_addr);
            if (size == NG_M68K_SIZE_BYTE) {
                out->immediate &= 0xFFu;
            }
            return 2;
        default:
            out->mode = NG_M68K_EA_NONE;
            return 0;
        }
    default:
        out->mode = NG_M68K_EA_NONE;
        return 0;
    }
}

static void populate_move_legacy_fields(NgM68kInstr *out) {
    if (out->mnemonic == NG_M68K_MOVEA &&
        out->src.mode == NG_M68K_EA_PC_INDEX &&
        out->dst.mode == NG_M68K_EA_AREG) {
        out->form = NG_M68K_FORM_PC_INDEX_TO_AREG;
        out->reg = out->dst.reg;
        out->src_reg = out->src.index_reg;
        out->displacement = out->src.displacement;
        out->target = out->src.absolute_addr;
        return;
    }

    if (out->src.mode == NG_M68K_EA_AREG &&
        (out->dst.mode == NG_M68K_EA_ABS_W || out->dst.mode == NG_M68K_EA_ABS_L)) {
        out->form = NG_M68K_FORM_AREG_TO_ABS;
        out->reg = out->src.reg;
        out->absolute_addr = out->dst.absolute_addr;
        return;
    }
    if (out->src.mode == NG_M68K_EA_IMM &&
        (out->dst.mode == NG_M68K_EA_ABS_W || out->dst.mode == NG_M68K_EA_ABS_L)) {
        out->form = NG_M68K_FORM_IMM_TO_ABS;
        out->immediate = out->src.immediate;
        out->absolute_addr = out->dst.absolute_addr;
        return;
    }
    if (out->src.mode == NG_M68K_EA_DREG &&
        (out->dst.mode == NG_M68K_EA_ABS_W || out->dst.mode == NG_M68K_EA_ABS_L)) {
        out->form = NG_M68K_FORM_DREG_TO_ABS;
        out->reg = out->src.reg;
        out->absolute_addr = out->dst.absolute_addr;
        return;
    }
    if ((out->src.mode == NG_M68K_EA_ABS_W || out->src.mode == NG_M68K_EA_ABS_L) &&
        out->dst.mode == NG_M68K_EA_DREG) {
        out->form = NG_M68K_FORM_ABS_TO_DREG;
        out->reg = out->dst.reg;
        out->absolute_addr = out->src.absolute_addr;
        return;
    }
    if (out->src.mode == NG_M68K_EA_ADISP &&
        out->dst.mode == NG_M68K_EA_DREG) {
        out->form = NG_M68K_FORM_AREG_DISP;
        out->reg = out->dst.reg;
        out->src_reg = out->src.reg;
        out->displacement = out->src.displacement;
        return;
    }
    if (out->src.mode == NG_M68K_EA_IMM &&
        out->dst.mode == NG_M68K_EA_DREG) {
        out->form = NG_M68K_FORM_IMM_TO_DREG;
        out->reg = out->dst.reg;
        out->immediate = out->src.immediate;
        return;
    }
    if (out->src.mode == NG_M68K_EA_DREG &&
        out->dst.mode == NG_M68K_EA_DREG) {
        out->form = NG_M68K_FORM_DREG_TO_DREG;
        out->src_reg = out->src.reg;
        out->reg = out->dst.reg;
        return;
    }
}

static void populate_unary_dst_legacy_fields(NgM68kInstr *out) {
    if (out->dst.mode == NG_M68K_EA_DREG) {
        out->form = NG_M68K_FORM_DREG;
        out->reg = out->dst.reg;
    } else if (out->dst.mode == NG_M68K_EA_ABS_W ||
               out->dst.mode == NG_M68K_EA_ABS_L) {
        out->form = NG_M68K_FORM_ABS;
        out->absolute_addr = out->dst.absolute_addr;
    } else if (out->dst.mode == NG_M68K_EA_ADISP) {
        out->form = NG_M68K_FORM_AREG_DISP;
        out->reg = out->dst.reg;
        out->displacement = out->dst.displacement;
    }
}

static int is_data_ea(uint8_t mode, uint8_t reg);
static int is_data_alterable_ea(uint8_t mode, uint8_t reg);
static int is_memory_alterable_ea(uint8_t mode, uint8_t reg);
static int is_control_ea(uint8_t mode, uint8_t reg);
static int is_movem_reg_to_mem_ea(uint8_t mode, uint8_t reg);
static int is_movem_mem_to_reg_ea(uint8_t mode, uint8_t reg);

static int decode_move(const NgProgramRom *rom, uint32_t addr, uint16_t op, NgM68kInstr *out) {
    uint8_t top = (uint8_t)((op >> 12) & 0xFu);
    uint8_t size;
    uint8_t src_mode;
    uint8_t src_reg;
    uint8_t dst_mode;
    uint8_t dst_reg;
    uint32_t ext_addr;

    if (top == 1u) {
        size = NG_M68K_SIZE_BYTE;
    } else if (top == 2u) {
        size = NG_M68K_SIZE_LONG;
    } else if (top == 3u) {
        size = NG_M68K_SIZE_WORD;
    } else {
        return 0;
    }

    src_mode = (uint8_t)((op >> 3) & 7u);
    src_reg = (uint8_t)(op & 7u);
    dst_mode = (uint8_t)((op >> 6) & 7u);
    dst_reg = (uint8_t)((op >> 9) & 7u);

    if (dst_mode == 1u && size == NG_M68K_SIZE_BYTE) {
        out->mnemonic = NG_M68K_UNKNOWN;
        out->byte_length = 2;
        return 1;
    }
    if (src_mode == 1u && size == NG_M68K_SIZE_BYTE) {
        out->mnemonic = NG_M68K_UNKNOWN;
        out->byte_length = 2;
        return 1;
    }
    if (dst_mode != 1u && !is_data_alterable_ea(dst_mode, dst_reg)) {
        out->mnemonic = NG_M68K_UNKNOWN;
        out->byte_length = 2;
        return 1;
    }

    out->mnemonic = (dst_mode == 1u) ? NG_M68K_MOVEA : NG_M68K_MOVE;
    out->size = size;
    out->byte_length = 2;

    ext_addr = addr + 2u;
    out->byte_length = (uint8_t)(out->byte_length +
        decode_ea(rom, ext_addr, src_mode, src_reg, size, &out->src));
    if (out->src.mode == NG_M68K_EA_NONE) {
        out->mnemonic = NG_M68K_UNKNOWN;
        out->byte_length = 2;
        return 1;
    }
    ext_addr = addr + out->byte_length;
    out->byte_length = (uint8_t)(out->byte_length +
        decode_ea(rom, ext_addr, dst_mode, dst_reg, size, &out->dst));
    if (out->dst.mode == NG_M68K_EA_NONE) {
        out->mnemonic = NG_M68K_UNKNOWN;
        out->byte_length = 2;
        return 1;
    }

    if (out->mnemonic == NG_M68K_MOVEA) {
        out->reg = dst_reg;
    }
    populate_move_legacy_fields(out);
    return 1;
}

static uint8_t size_from_opcode_bits(uint8_t size_code) {
    if (size_code == 2u) {
        return NG_M68K_SIZE_LONG;
    }
    return size_code == 1u ? NG_M68K_SIZE_WORD : NG_M68K_SIZE_BYTE;
}

static int decode_alu_address_reg(const NgProgramRom *rom,
                                  uint32_t addr,
                                  uint16_t op,
                                  NgM68kInstr *out) {
    uint8_t top = (uint8_t)((op >> 12) & 0xFu);
    uint8_t opmode = (uint8_t)((op >> 6) & 7u);
    uint8_t src_mode;
    uint8_t src_reg;
    uint8_t dst_reg;

    if (top != 0xDu && top != 0x9u && top != 0xBu) {
        return 0;
    }
    if (opmode != 3u && opmode != 7u) {
        return 0;
    }

    src_mode = (uint8_t)((op >> 3) & 7u);
    src_reg = (uint8_t)(op & 7u);
    dst_reg = (uint8_t)((op >> 9) & 7u);

    if (top == 0xDu) {
        out->mnemonic = NG_M68K_ADDA;
    } else if (top == 0x9u) {
        out->mnemonic = NG_M68K_SUBA;
    } else {
        out->mnemonic = NG_M68K_CMPA;
    }
    out->size = opmode == 7u ? NG_M68K_SIZE_LONG : NG_M68K_SIZE_WORD;
    out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                src_mode, src_reg,
                                                out->size, &out->src));
    if (out->src.mode == NG_M68K_EA_NONE) {
        out->mnemonic = NG_M68K_UNKNOWN;
        out->byte_length = 2;
        return 1;
    }

    out->dst.mode = NG_M68K_EA_AREG;
    out->dst.reg = dst_reg;
    out->reg = dst_reg;
    return 1;
}

static int is_data_alterable_ea(uint8_t mode, uint8_t reg) {
    return mode != 1u && !(mode == 7u && reg >= 2u);
}

static int is_data_ea(uint8_t mode, uint8_t reg) {
    return mode != 1u && !(mode == 7u && reg > 4u);
}

static int is_memory_alterable_ea(uint8_t mode, uint8_t reg) {
    return mode >= 2u && !(mode == 7u && reg >= 2u);
}

static int is_control_ea(uint8_t mode, uint8_t reg) {
    return mode == 2u || mode == 5u || mode == 6u ||
           (mode == 7u && reg <= 3u);
}

static int is_movem_reg_to_mem_ea(uint8_t mode, uint8_t reg) {
    return mode == 2u || mode == 4u || mode == 5u || mode == 6u ||
           (mode == 7u && reg <= 1u);
}

static int is_movem_mem_to_reg_ea(uint8_t mode, uint8_t reg) {
    return mode == 2u || mode == 3u || mode == 5u || mode == 6u ||
           (mode == 7u && reg <= 3u);
}

static int decode_logic_binary(const NgProgramRom *rom,
                               uint32_t addr,
                               uint16_t op,
                               NgM68kInstr *out) {
    uint8_t top = (uint8_t)((op >> 12) & 0xFu);
    uint8_t opmode = (uint8_t)((op >> 6) & 7u);
    uint8_t size_code = (uint8_t)(opmode & 3u);
    uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
    uint8_t ea_reg = (uint8_t)(op & 7u);
    uint8_t reg_field = (uint8_t)((op >> 9) & 7u);
    NgM68kMnemonic mnemonic;

    if (size_code == 3u) {
        return 0;
    }
    if (top == 0x8u) {
        mnemonic = NG_M68K_OR;
    } else if (top == 0xCu) {
        mnemonic = NG_M68K_AND;
    } else if (top == 0xBu) {
        mnemonic = NG_M68K_EOR;
    } else {
        return 0;
    }

    if ((top == 0x8u || top == 0xCu) && opmode <= 2u) {
        if (!is_data_ea(ea_mode, ea_reg)) {
            return 0;
        }
        out->mnemonic = mnemonic;
        out->size = size_from_opcode_bits(size_code);
        out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                    ea_mode, ea_reg,
                                                    out->size, &out->src));
        if (out->src.mode == NG_M68K_EA_NONE) {
            out->mnemonic = NG_M68K_UNKNOWN;
            out->byte_length = 2;
            return 1;
        }
        out->dst.mode = NG_M68K_EA_DREG;
        out->dst.reg = reg_field;
        out->reg = reg_field;
        out->src_reg = ea_reg;
        return 1;
    }

    if ((top == 0x8u || top == 0xCu) && opmode >= 4u && opmode <= 6u) {
        if (!is_memory_alterable_ea(ea_mode, ea_reg)) {
            return 0;
        }
    } else if (top == 0xBu && opmode >= 4u && opmode <= 6u) {
        if (!is_data_alterable_ea(ea_mode, ea_reg)) {
            return 0;
        }
    } else {
        return 0;
    }

    out->mnemonic = mnemonic;
    out->size = size_from_opcode_bits(size_code);
    out->src.mode = NG_M68K_EA_DREG;
    out->src.reg = reg_field;
    out->src_reg = reg_field;
    out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                ea_mode, ea_reg,
                                                out->size, &out->dst));
    if (out->dst.mode == NG_M68K_EA_NONE) {
        out->mnemonic = NG_M68K_UNKNOWN;
        out->byte_length = 2;
        return 1;
    }
    if (out->dst.mode == NG_M68K_EA_DREG) {
        out->form = NG_M68K_FORM_DREG_TO_DREG;
        out->reg = out->dst.reg;
    } else if (out->dst.mode == NG_M68K_EA_ABS_W ||
               out->dst.mode == NG_M68K_EA_ABS_L) {
        out->form = NG_M68K_FORM_ABS;
        out->absolute_addr = out->dst.absolute_addr;
    } else if (out->dst.mode == NG_M68K_EA_ADISP) {
        out->form = NG_M68K_FORM_AREG_DISP;
        out->reg = out->dst.reg;
        out->displacement = out->dst.displacement;
    }
    return 1;
}

static int decode_addsub_dreg_to_ea(const NgProgramRom *rom,
                                    uint32_t addr,
                                    uint16_t op,
                                    NgM68kInstr *out) {
    uint8_t top = (uint8_t)((op >> 12) & 0xFu);
    uint8_t opmode = (uint8_t)((op >> 6) & 7u);
    uint8_t size_code = (uint8_t)(opmode & 3u);
    uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
    uint8_t ea_reg = (uint8_t)(op & 7u);
    uint8_t src_reg = (uint8_t)((op >> 9) & 7u);

    if ((top != 0xDu && top != 0x9u) ||
        opmode < 4u || opmode > 6u ||
        !is_memory_alterable_ea(ea_mode, ea_reg)) {
        return 0;
    }

    out->mnemonic = top == 0xDu ? NG_M68K_ADD : NG_M68K_SUB;
    out->size = size_from_opcode_bits(size_code);
    out->src.mode = NG_M68K_EA_DREG;
    out->src.reg = src_reg;
    out->src_reg = src_reg;
    out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                ea_mode, ea_reg,
                                                out->size, &out->dst));
    if (out->dst.mode == NG_M68K_EA_NONE) {
        out->mnemonic = NG_M68K_UNKNOWN;
        out->byte_length = 2;
        return 1;
    }
    if (out->dst.mode == NG_M68K_EA_ABS_W ||
        out->dst.mode == NG_M68K_EA_ABS_L) {
        out->form = NG_M68K_FORM_ABS;
        out->absolute_addr = out->dst.absolute_addr;
    } else if (out->dst.mode == NG_M68K_EA_ADISP) {
        out->form = NG_M68K_FORM_AREG_DISP;
        out->reg = out->dst.reg;
        out->displacement = out->dst.displacement;
    }
    return 1;
}

static int decode_multiply(const NgProgramRom *rom,
                           uint32_t addr,
                           uint16_t op,
                           NgM68kInstr *out) {
    uint8_t top = (uint8_t)((op >> 12) & 0xFu);
    uint8_t opmode = (uint8_t)((op >> 6) & 7u);
    uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
    uint8_t ea_reg = (uint8_t)(op & 7u);
    uint8_t dst_reg = (uint8_t)((op >> 9) & 7u);

    if (top != 0xCu || (opmode != 3u && opmode != 7u) || ea_mode == 1u) {
        return 0;
    }

    out->mnemonic = opmode == 3u ? NG_M68K_MULU : NG_M68K_MULS;
    out->size = NG_M68K_SIZE_WORD;
    out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                ea_mode, ea_reg,
                                                out->size, &out->src));
    if (out->src.mode == NG_M68K_EA_NONE) {
        out->mnemonic = NG_M68K_UNKNOWN;
        out->byte_length = 2;
        return 1;
    }
    out->dst.mode = NG_M68K_EA_DREG;
    out->dst.reg = dst_reg;
    out->reg = dst_reg;
    return 1;
}

static int decode_divide(const NgProgramRom *rom,
                         uint32_t addr,
                         uint16_t op,
                         NgM68kInstr *out) {
    uint8_t top = (uint8_t)((op >> 12) & 0xFu);
    uint8_t opmode = (uint8_t)((op >> 6) & 7u);
    uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
    uint8_t ea_reg = (uint8_t)(op & 7u);
    uint8_t dst_reg = (uint8_t)((op >> 9) & 7u);

    if (top != 0x8u || (opmode != 3u && opmode != 7u) || ea_mode == 1u) {
        return 0;
    }

    out->mnemonic = opmode == 3u ? NG_M68K_DIVU : NG_M68K_DIVS;
    out->size = NG_M68K_SIZE_WORD;
    out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                ea_mode, ea_reg,
                                                out->size, &out->src));
    if (out->src.mode == NG_M68K_EA_NONE) {
        out->mnemonic = NG_M68K_UNKNOWN;
        out->byte_length = 2;
        return 1;
    }
    out->dst.mode = NG_M68K_EA_DREG;
    out->dst.reg = dst_reg;
    out->reg = dst_reg;
    return 1;
}

static int decode_exchange(uint16_t op, NgM68kInstr *out) {
    uint8_t left_reg = (uint8_t)((op >> 9) & 7u);
    uint8_t right_reg = (uint8_t)(op & 7u);

    if ((op & 0xF1F8u) == 0xC140u) {
        out->mnemonic = NG_M68K_EXG;
        out->byte_length = 2;
        out->size = NG_M68K_SIZE_LONG;
        out->src.mode = NG_M68K_EA_DREG;
        out->src.reg = left_reg;
        out->src_reg = left_reg;
        out->dst.mode = NG_M68K_EA_DREG;
        out->dst.reg = right_reg;
        out->reg = right_reg;
        return 1;
    }
    if ((op & 0xF1F8u) == 0xC148u) {
        out->mnemonic = NG_M68K_EXG;
        out->byte_length = 2;
        out->size = NG_M68K_SIZE_LONG;
        out->src.mode = NG_M68K_EA_AREG;
        out->src.reg = left_reg;
        out->src_reg = left_reg;
        out->dst.mode = NG_M68K_EA_AREG;
        out->dst.reg = right_reg;
        out->reg = right_reg;
        return 1;
    }
    if ((op & 0xF1F8u) == 0xC188u) {
        out->mnemonic = NG_M68K_EXG;
        out->byte_length = 2;
        out->size = NG_M68K_SIZE_LONG;
        out->src.mode = NG_M68K_EA_DREG;
        out->src.reg = left_reg;
        out->src_reg = left_reg;
        out->dst.mode = NG_M68K_EA_AREG;
        out->dst.reg = right_reg;
        out->reg = right_reg;
        return 1;
    }
    return 0;
}

static int decode_shift_rotate(const NgProgramRom *rom,
                               uint32_t addr,
                               uint16_t op,
                               NgM68kInstr *out) {
    uint8_t size_code;
    uint8_t dir_left;
    uint8_t count_is_reg;
    uint8_t kind;
    uint8_t dst_reg;
    uint8_t count_field;

    if ((op & 0xF000u) != 0xE000u) {
        return 0;
    }

    size_code = (uint8_t)((op >> 6) & 3u);
    if (size_code == 3u) {
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if ((op & 0x0800u) != 0u) {
            return 0;
        }
        if (!is_memory_alterable_ea(ea_mode, ea_reg)) {
            return 0;
        }

        dir_left = (uint8_t)((op >> 8) & 1u);
        kind = (uint8_t)((op >> 9) & 3u);
        static const NgM68kMnemonic right_mnemonics[4] = {
            NG_M68K_ASR,
            NG_M68K_LSR,
            NG_M68K_ROXR,
            NG_M68K_ROR,
        };
        static const NgM68kMnemonic left_mnemonics[4] = {
            NG_M68K_ASL,
            NG_M68K_LSL,
            NG_M68K_ROXL,
            NG_M68K_ROL,
        };

        out->mnemonic = dir_left ? left_mnemonics[kind] : right_mnemonics[kind];
        out->size = NG_M68K_SIZE_WORD;
        out->immediate = 1u;
        out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                    ea_mode, ea_reg,
                                                    out->size, &out->dst));
        if (out->dst.mode == NG_M68K_EA_NONE) {
            out->mnemonic = NG_M68K_UNKNOWN;
            out->byte_length = 2;
        }
        return 1;
    }

    dir_left = (uint8_t)((op >> 8) & 1u);
    count_is_reg = (uint8_t)((op >> 5) & 1u);
    kind = (uint8_t)((op >> 3) & 3u);
    dst_reg = (uint8_t)(op & 7u);
    count_field = (uint8_t)((op >> 9) & 7u);

    static const NgM68kMnemonic right_mnemonics[4] = {
        NG_M68K_ASR,
        NG_M68K_LSR,
        NG_M68K_ROXR,
        NG_M68K_ROR,
    };
    static const NgM68kMnemonic left_mnemonics[4] = {
        NG_M68K_ASL,
        NG_M68K_LSL,
        NG_M68K_ROXL,
        NG_M68K_ROL,
    };

    out->mnemonic = dir_left ? left_mnemonics[kind] : right_mnemonics[kind];
    out->size = size_from_opcode_bits(size_code);
    out->byte_length = 2;
    out->dst.mode = NG_M68K_EA_DREG;
    out->dst.reg = dst_reg;
    out->reg = dst_reg;
    if (count_is_reg) {
        out->src.mode = NG_M68K_EA_DREG;
        out->src.reg = count_field;
        out->src_reg = count_field;
    } else {
        out->immediate = count_field == 0u ? 8u : count_field;
    }
    return 1;
}

static int decode_alu_ea_to_dreg(const NgProgramRom *rom,
                                 uint32_t addr,
                                 uint16_t op,
                                 NgM68kInstr *out) {
    uint8_t top = (uint8_t)((op >> 12) & 0xFu);
    uint8_t size_code;
    uint8_t dir;
    uint8_t src_mode;
    uint8_t src_reg;
    uint8_t dst_reg;

    if (top != 0xDu && top != 0x9u && top != 0xBu) {
        return 0;
    }

    size_code = (uint8_t)((op >> 6) & 3u);
    if (size_code == 3u) {
        return 0;
    }

    dir = (uint8_t)((op >> 8) & 1u);
    if (dir != 0u) {
        return 0;
    }

    src_mode = (uint8_t)((op >> 3) & 7u);
    src_reg = (uint8_t)(op & 7u);
    dst_reg = (uint8_t)((op >> 9) & 7u);

    if (!is_data_ea(src_mode, src_reg) &&
        !(src_mode == 1u && size_code != 0u)) {
        return 0;
    }

    out->mnemonic = top == 0xDu ? NG_M68K_ADD :
                    (top == 0x9u ? NG_M68K_SUB : NG_M68K_CMP);
    out->size = size_from_opcode_bits(size_code);
    out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                src_mode, src_reg,
                                                out->size, &out->src));
    if (out->src.mode == NG_M68K_EA_NONE) {
        out->mnemonic = NG_M68K_UNKNOWN;
        out->byte_length = 2;
        return 1;
    }

    out->dst.mode = NG_M68K_EA_DREG;
    out->dst.reg = dst_reg;
    out->form = NG_M68K_FORM_DREG_TO_DREG;
    out->src_reg = src_reg;
    out->reg = dst_reg;
    return 1;
}

static int decode_cmpm(uint16_t op, NgM68kInstr *out) {
    uint8_t size_code = (uint8_t)((op >> 6) & 3u);

    if ((op & 0xF138u) != 0xB108u || size_code == 3u) {
        return 0;
    }

    out->mnemonic = NG_M68K_CMPM;
    out->size = size_from_opcode_bits(size_code);
    out->byte_length = 2;
    out->src.mode = NG_M68K_EA_APOST;
    out->src.reg = (uint8_t)(op & 7u);
    out->dst.mode = NG_M68K_EA_APOST;
    out->dst.reg = (uint8_t)((op >> 9) & 7u);
    out->src_reg = out->src.reg;
    out->reg = out->dst.reg;
    return 1;
}

static int decode_immediate_to_sr_ccr(const NgProgramRom *rom,
                                      uint32_t addr,
                                      uint16_t op,
                                      NgM68kInstr *out) {
    switch (op) {
    case 0x003C:
        out->mnemonic = NG_M68K_ORI_TO_CCR;
        out->size = NG_M68K_SIZE_BYTE;
        out->immediate = ng_program_rom_read16(rom, addr + 2u) & 0xFFu;
        break;
    case 0x007C:
        out->mnemonic = NG_M68K_ORI_TO_SR;
        out->size = NG_M68K_SIZE_WORD;
        out->immediate = ng_program_rom_read16(rom, addr + 2u);
        break;
    case 0x023C:
        out->mnemonic = NG_M68K_ANDI_TO_CCR;
        out->size = NG_M68K_SIZE_BYTE;
        out->immediate = ng_program_rom_read16(rom, addr + 2u) & 0xFFu;
        break;
    case 0x027C:
        out->mnemonic = NG_M68K_ANDI_TO_SR;
        out->size = NG_M68K_SIZE_WORD;
        out->immediate = ng_program_rom_read16(rom, addr + 2u);
        break;
    case 0x0A3C:
        out->mnemonic = NG_M68K_EORI_TO_CCR;
        out->size = NG_M68K_SIZE_BYTE;
        out->immediate = ng_program_rom_read16(rom, addr + 2u) & 0xFFu;
        break;
    case 0x0A7C:
        out->mnemonic = NG_M68K_EORI_TO_SR;
        out->size = NG_M68K_SIZE_WORD;
        out->immediate = ng_program_rom_read16(rom, addr + 2u);
        break;
    default:
        return 0;
    }

    out->byte_length = 4;
    return 1;
}

static int finish_decode(const NgProgramRom *rom, uint32_t addr, NgM68kInstr *out) {
    uint32_t last;

    if (out->byte_length == 0) {
        return 0;
    }
    last = addr + (uint32_t)out->byte_length - 1u;
    if (last < addr || !ng_program_rom_addr_is_mapped(rom, last)) {
        memset(out, 0, sizeof(*out));
        out->addr = addr;
        out->mnemonic = NG_M68K_INVALID;
        return 0;
    }
    return 1;
}

int ng_m68k_decode(const NgProgramRom *rom, uint32_t addr, NgM68kInstr *out) {
    memset(out, 0, sizeof(*out));
    out->addr = addr;
    out->mnemonic = NG_M68K_INVALID;

    if (!ng_program_rom_addr_is_mapped(rom, addr) ||
        !ng_program_rom_addr_is_mapped(rom, addr + 1u)) {
        return 0;
    }

    uint16_t op = ng_program_rom_read16(rom, addr);
    out->opcode = op;
    out->mnemonic = NG_M68K_UNKNOWN;
    out->byte_length = 2;

    if (decode_move(rom, addr, op, out)) {
        return finish_decode(rom, addr, out);
    }
    if (decode_alu_address_reg(rom, addr, op, out)) {
        return finish_decode(rom, addr, out);
    }
    if (decode_logic_binary(rom, addr, op, out)) {
        return finish_decode(rom, addr, out);
    }
    if (decode_addsub_dreg_to_ea(rom, addr, op, out)) {
        return finish_decode(rom, addr, out);
    }
    if (decode_exchange(op, out)) {
        return finish_decode(rom, addr, out);
    }
    if (decode_shift_rotate(rom, addr, op, out)) {
        return finish_decode(rom, addr, out);
    }
    if (decode_multiply(rom, addr, op, out)) {
        return finish_decode(rom, addr, out);
    }
    if (decode_divide(rom, addr, op, out)) {
        return finish_decode(rom, addr, out);
    }
    if (decode_alu_ea_to_dreg(rom, addr, op, out)) {
        return finish_decode(rom, addr, out);
    }
    if (decode_cmpm(op, out)) {
        return finish_decode(rom, addr, out);
    }

    if (op == 0x4AFCu) {
        out->mnemonic = NG_M68K_ILLEGAL;
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFFF0u) == 0x4E40u) {
        out->mnemonic = NG_M68K_TRAP;
        out->immediate = op & 0x000Fu;
        return finish_decode(rom, addr, out);
    }
    if (op == 0x4E70u) {
        out->mnemonic = NG_M68K_RESET;
        return finish_decode(rom, addr, out);
    }
    if (op == 0x4E71u) {
        out->mnemonic = NG_M68K_NOP;
        return finish_decode(rom, addr, out);
    }
    if (op == 0x4E72u) {
        out->mnemonic = NG_M68K_STOP;
        out->byte_length = 4;
        out->immediate = ng_program_rom_read16(rom, addr + 2u);
        return finish_decode(rom, addr, out);
    }
    if (op == 0x4E73u) {
        out->mnemonic = NG_M68K_RTE;
        return finish_decode(rom, addr, out);
    }
    if (op == 0x4E75u) {
        out->mnemonic = NG_M68K_RTS;
        return finish_decode(rom, addr, out);
    }
    if (op == 0x4E76u) {
        out->mnemonic = NG_M68K_TRAPV;
        return finish_decode(rom, addr, out);
    }
    if (op == 0x4E77u) {
        out->mnemonic = NG_M68K_RTR;
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFFF8u) == 0x4E50u) {
        out->mnemonic = NG_M68K_LINK;
        out->byte_length = 4;
        out->size = NG_M68K_SIZE_WORD;
        out->reg = (uint8_t)(op & 7u);
        out->displacement = sign16(ng_program_rom_read16(rom, addr + 2u));
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFFF8u) == 0x4E58u) {
        out->mnemonic = NG_M68K_UNLK;
        out->byte_length = 2;
        out->reg = (uint8_t)(op & 7u);
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFFF8u) == 0x4E60u ||
        (op & 0xFFF8u) == 0x4E68u) {
        out->mnemonic = NG_M68K_MOVE_USP;
        out->size = NG_M68K_SIZE_LONG;
        out->byte_length = 2;
        out->reg = (uint8_t)(op & 7u);
        if ((op & 0x0008u) == 0u) {
            out->src.mode = NG_M68K_EA_AREG;
            out->src.reg = out->reg;
        } else {
            out->dst.mode = NG_M68K_EA_AREG;
            out->dst.reg = out->reg;
        }
        return finish_decode(rom, addr, out);
    }
    if (op == 0x4EF9u || op == 0x4EB9u) {
        out->mnemonic = (op == 0x4EF9u) ? NG_M68K_JMP : NG_M68K_JSR;
        out->byte_length = 6;
        out->target = ng_program_rom_read32(rom, addr + 2u);
        out->form = NG_M68K_FORM_ABS;
        out->src.mode = NG_M68K_EA_ABS_L;
        out->src.reg = 1u;
        out->src.absolute_addr = out->target;
        return finish_decode(rom, addr, out);
    }
    if (op == 0x4EFAu || op == 0x4EBAu) {
        out->mnemonic = (op == 0x4EFAu) ? NG_M68K_JMP : NG_M68K_JSR;
        out->byte_length = 4;
        out->form = NG_M68K_FORM_PC_RELATIVE;
        out->displacement = sign16(ng_program_rom_read16(rom, addr + 2u));
        out->target = (uint32_t)((int32_t)(addr + 2u) + (int32_t)out->displacement);
        out->src.mode = NG_M68K_EA_PC_DISP;
        out->src.reg = 2u;
        out->src.displacement = out->displacement;
        out->src.absolute_addr = out->target;
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFFF8u) == 0x4ED0u) {
        out->mnemonic = NG_M68K_JMP;
        out->byte_length = 2;
        out->form = NG_M68K_FORM_AREG_INDIRECT;
        out->reg = (uint8_t)(op & 7u);
        out->src.mode = NG_M68K_EA_AIND;
        out->src.reg = out->reg;
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFFC0u) == 0x4E80u ||
        (op & 0xFFC0u) == 0x4EC0u) {
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if (is_control_ea(ea_mode, ea_reg)) {
            out->mnemonic = ((op & 0xFFC0u) == 0x4E80u) ?
                NG_M68K_JSR : NG_M68K_JMP;
            out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->src));
            if (out->src.mode == NG_M68K_EA_AIND) {
                out->form = NG_M68K_FORM_AREG_INDIRECT;
                out->reg = out->src.reg;
            } else if (out->src.mode == NG_M68K_EA_PC_DISP ||
                       out->src.mode == NG_M68K_EA_PC_INDEX) {
                out->form = NG_M68K_FORM_PC_RELATIVE;
                out->target = out->src.absolute_addr;
            } else if (out->src.mode == NG_M68K_EA_ABS_W ||
                       out->src.mode == NG_M68K_EA_ABS_L) {
                out->form = NG_M68K_FORM_ABS;
                out->target = out->src.absolute_addr;
            }
            if (out->src.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
            }
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xFF00u) == 0x6000u) {
        out->mnemonic = NG_M68K_BRA;
        populate_branch_target(rom, addr, op, out);
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFF00u) == 0x6100u) {
        out->mnemonic = NG_M68K_BSR;
        populate_branch_target(rom, addr, op, out);
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xF000u) == 0x6000u) {
        out->mnemonic = NG_M68K_BCC;
        out->condition = (uint8_t)((op >> 8) & 0xFu);
        populate_branch_target(rom, addr, op, out);
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xF100u) == 0x7000u) {
        out->mnemonic = NG_M68K_MOVEQ;
        out->size = NG_M68K_SIZE_LONG;
        out->byte_length = 2;
        out->form = NG_M68K_FORM_IMM_TO_DREG;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->immediate = (uint32_t)(int32_t)sign8((uint8_t)(op & 0xFFu));
        out->dst.mode = NG_M68K_EA_DREG;
        out->dst.reg = out->reg;
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xF1FFu) == 0x41FAu) {
        out->mnemonic = NG_M68K_LEA;
        out->size = NG_M68K_SIZE_LONG;
        out->byte_length = 4;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->displacement = sign16(ng_program_rom_read16(rom, addr + 2u));
        out->target = (uint32_t)((int32_t)(addr + 2u) + (int32_t)out->displacement);
        out->src.mode = NG_M68K_EA_PC_DISP;
        out->src.reg = 2u;
        out->src.displacement = out->displacement;
        out->src.absolute_addr = out->target;
        out->dst.mode = NG_M68K_EA_AREG;
        out->dst.reg = out->reg;
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xF1C0u) == 0x4180u) {
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if (ea_mode != 1u) {
            out->mnemonic = NG_M68K_CHK;
            out->size = NG_M68K_SIZE_WORD;
            out->reg = (uint8_t)((op >> 9) & 7u);
            out->dst.mode = NG_M68K_EA_DREG;
            out->dst.reg = out->reg;
            out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->src));
            if (out->src.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
            }
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xF1C0u) == 0x41C0u) {
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if (is_control_ea(ea_mode, ea_reg)) {
            out->mnemonic = NG_M68K_LEA;
            out->size = NG_M68K_SIZE_LONG;
            out->reg = (uint8_t)((op >> 9) & 7u);
            out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->src));
            out->dst.mode = NG_M68K_EA_AREG;
            out->dst.reg = out->reg;
            if (out->src.mode == NG_M68K_EA_ABS_W ||
                out->src.mode == NG_M68K_EA_ABS_L ||
                out->src.mode == NG_M68K_EA_PC_DISP ||
                out->src.mode == NG_M68K_EA_PC_INDEX) {
                out->target = out->src.absolute_addr;
            }
            if (out->src.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
            }
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xF1FFu) == 0x41F9u) {
        out->mnemonic = NG_M68K_LEA;
        out->size = NG_M68K_SIZE_LONG;
        out->byte_length = 6;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->target = ng_program_rom_read32(rom, addr + 2u);
        out->src.mode = NG_M68K_EA_ABS_L;
        out->src.reg = 1u;
        out->src.absolute_addr = out->target;
        out->dst.mode = NG_M68K_EA_AREG;
        out->dst.reg = out->reg;
        return finish_decode(rom, addr, out);
    }
    if (op == 0x207Bu) {
        uint16_t ext = ng_program_rom_read16(rom, addr + 2u);
        out->mnemonic = NG_M68K_MOVEA;
        out->byte_length = 4;
        out->size = NG_M68K_SIZE_LONG;
        out->form = NG_M68K_FORM_PC_INDEX_TO_AREG;
        out->reg = 0;
        out->src_reg = (uint8_t)((ext >> 12) & 7u);
        out->displacement = (int8_t)(ext & 0xFFu);
        out->target = (uint32_t)((int32_t)(addr + 2u) + (int32_t)out->displacement);
        return finish_decode(rom, addr, out);
    }
    if (decode_immediate_to_sr_ccr(rom, addr, op, out)) {
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xF138u) == 0x0108u) {
        uint8_t data_reg = (uint8_t)((op >> 9) & 7u);
        uint8_t addr_reg = (uint8_t)(op & 7u);
        uint8_t dreg_to_mem = (uint8_t)((op >> 7) & 1u);
        out->mnemonic = NG_M68K_MOVEP;
        out->size = (op & 0x0040u) ? NG_M68K_SIZE_LONG : NG_M68K_SIZE_WORD;
        out->byte_length = 4;
        out->displacement = sign16(ng_program_rom_read16(rom, addr + 2u));
        out->reg = data_reg;
        if (dreg_to_mem) {
            out->src.mode = NG_M68K_EA_DREG;
            out->src.reg = data_reg;
            out->src_reg = data_reg;
            out->dst.mode = NG_M68K_EA_ADISP;
            out->dst.reg = addr_reg;
            out->dst.displacement = out->displacement;
        } else {
            out->src.mode = NG_M68K_EA_ADISP;
            out->src.reg = addr_reg;
            out->src.displacement = out->displacement;
            out->dst.mode = NG_M68K_EA_DREG;
            out->dst.reg = data_reg;
        }
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFFC0u) == 0x40C0u) {
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if (!is_data_alterable_ea(ea_mode, ea_reg)) {
            out->mnemonic = NG_M68K_UNKNOWN;
            return finish_decode(rom, addr, out);
        }
        out->mnemonic = NG_M68K_MOVE_SR;
        out->size = NG_M68K_SIZE_WORD;
        out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                    ea_mode, ea_reg,
                                                    out->size, &out->dst));
        if (out->dst.mode == NG_M68K_EA_NONE) {
            out->mnemonic = NG_M68K_UNKNOWN;
            out->byte_length = 2;
        }
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFFC0u) == 0x44C0u ||
        (op & 0xFFC0u) == 0x46C0u) {
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if (!is_data_ea(ea_mode, ea_reg)) {
            out->mnemonic = NG_M68K_UNKNOWN;
            return finish_decode(rom, addr, out);
        }
        out->mnemonic = ((op & 0xFFC0u) == 0x46C0u) ?
            NG_M68K_MOVE_SR : NG_M68K_MOVE_CCR;
        out->size = NG_M68K_SIZE_WORD;
        out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                    ea_mode, ea_reg,
                                                    out->size, &out->src));
        if (out->src.mode == NG_M68K_EA_NONE) {
            out->mnemonic = NG_M68K_UNKNOWN;
            out->byte_length = 2;
        }
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFF00u) == 0x0800u) {
        uint8_t op_kind = (uint8_t)((op >> 6) & 3u);
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if ((op_kind == 0u &&
             is_data_ea(ea_mode, ea_reg) &&
             !(ea_mode == 7u && ea_reg == 4u)) ||
            (op_kind != 0u && is_data_alterable_ea(ea_mode, ea_reg))) {
            static const NgM68kMnemonic mnemonics[4] = {
                NG_M68K_BTST,
                NG_M68K_BCHG,
                NG_M68K_BCLR,
                NG_M68K_BSET,
            };
            out->mnemonic = mnemonics[op_kind];
            out->size = ea_mode == 0u ? NG_M68K_SIZE_LONG : NG_M68K_SIZE_BYTE;
            out->immediate = ng_program_rom_read16(rom, addr + 2u) & 0xFFu;
            out->byte_length = (uint8_t)(4u + decode_ea(rom, addr + 4u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->dst));
            if (out->dst.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
                return finish_decode(rom, addr, out);
            }
            if (out->dst.mode == NG_M68K_EA_DREG) {
                out->form = NG_M68K_FORM_DREG;
                out->reg = out->dst.reg;
            } else if (out->dst.mode == NG_M68K_EA_ABS_W ||
                       out->dst.mode == NG_M68K_EA_ABS_L) {
                out->form = NG_M68K_FORM_ABS;
                out->absolute_addr = out->dst.absolute_addr;
            } else if (out->dst.mode == NG_M68K_EA_ADISP) {
                out->form = NG_M68K_FORM_AREG_DISP;
                out->reg = out->dst.reg;
                out->displacement = out->dst.displacement;
            }
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xF100u) == 0x0100u) {
        uint8_t op_kind = (uint8_t)((op >> 6) & 3u);
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if ((op_kind == 0u && is_data_ea(ea_mode, ea_reg)) ||
            (op_kind != 0u && is_data_alterable_ea(ea_mode, ea_reg))) {
            static const NgM68kMnemonic mnemonics[4] = {
                NG_M68K_BTST,
                NG_M68K_BCHG,
                NG_M68K_BCLR,
                NG_M68K_BSET,
            };
            out->mnemonic = mnemonics[op_kind];
            out->size = ea_mode == 0u ? NG_M68K_SIZE_LONG : NG_M68K_SIZE_BYTE;
            out->src.mode = NG_M68K_EA_DREG;
            out->src.reg = (uint8_t)((op >> 9) & 7u);
            out->src_reg = out->src.reg;
            out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->dst));
            if (out->dst.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
                return finish_decode(rom, addr, out);
            }
            if (out->dst.mode == NG_M68K_EA_DREG) {
                out->form = NG_M68K_FORM_DREG_TO_DREG;
                out->reg = out->dst.reg;
            } else if (out->dst.mode == NG_M68K_EA_ABS_W ||
                       out->dst.mode == NG_M68K_EA_ABS_L) {
                out->form = NG_M68K_FORM_ABS;
                out->absolute_addr = out->dst.absolute_addr;
            } else if (out->dst.mode == NG_M68K_EA_ADISP) {
                out->form = NG_M68K_FORM_AREG_DISP;
                out->reg = out->dst.reg;
                out->displacement = out->dst.displacement;
            }
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xFF00u) == 0x0000u ||
        (op & 0xFF00u) == 0x0A00u) {
        uint8_t size_code = (uint8_t)((op >> 6) & 3u);
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        uint8_t imm_len;
        uint32_t ext_addr;
        if (size_code != 3u && ea_mode != 1u &&
            !(ea_mode == 7u && ea_reg >= 2u)) {
            out->mnemonic = ((op & 0xFF00u) == 0x0000u) ?
                NG_M68K_ORI : NG_M68K_EORI;
            out->size = size_from_opcode_bits(size_code);
            if (out->size == NG_M68K_SIZE_LONG) {
                out->immediate = ng_program_rom_read32(rom, addr + 2u);
                imm_len = 4u;
            } else {
                out->immediate = ng_program_rom_read16(rom, addr + 2u);
                if (out->size == NG_M68K_SIZE_BYTE) {
                    out->immediate &= 0xFFu;
                }
                imm_len = 2u;
            }
            ext_addr = addr + 2u + imm_len;
            out->byte_length = (uint8_t)(2u + imm_len +
                decode_ea(rom, ext_addr, ea_mode, ea_reg,
                          out->size, &out->dst));
            if (out->dst.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
                return finish_decode(rom, addr, out);
            }
            if (out->dst.mode == NG_M68K_EA_DREG) {
                out->form = NG_M68K_FORM_IMM_TO_DREG;
                out->reg = out->dst.reg;
            } else if (out->dst.mode == NG_M68K_EA_ADISP) {
                out->form = NG_M68K_FORM_AREG_DISP;
                out->reg = out->dst.reg;
                out->displacement = out->dst.displacement;
            } else if (out->dst.mode == NG_M68K_EA_ABS_W ||
                       out->dst.mode == NG_M68K_EA_ABS_L) {
                out->form = NG_M68K_FORM_ABS;
                out->absolute_addr = out->dst.absolute_addr;
            }
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xFF00u) == 0x0400u ||
        (op & 0xFF00u) == 0x0600u) {
        uint8_t size_code = (uint8_t)((op >> 6) & 3u);
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        uint8_t imm_len;
        uint32_t ext_addr;
        if (size_code != 3u && ea_mode != 1u &&
            !(ea_mode == 7u && ea_reg >= 2u)) {
            out->mnemonic = ((op & 0xFF00u) == 0x0400u) ?
                NG_M68K_SUBI : NG_M68K_ADDI;
            out->size = size_from_opcode_bits(size_code);
            if (out->size == NG_M68K_SIZE_LONG) {
                out->immediate = ng_program_rom_read32(rom, addr + 2u);
                imm_len = 4u;
            } else {
                out->immediate = ng_program_rom_read16(rom, addr + 2u);
                if (out->size == NG_M68K_SIZE_BYTE) {
                    out->immediate &= 0xFFu;
                }
                imm_len = 2u;
            }
            ext_addr = addr + 2u + imm_len;
            out->byte_length = (uint8_t)(2u + imm_len +
                decode_ea(rom, ext_addr, ea_mode, ea_reg,
                          out->size, &out->dst));
            if (out->dst.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
                return finish_decode(rom, addr, out);
            }
            if (out->dst.mode == NG_M68K_EA_DREG) {
                out->form = NG_M68K_FORM_IMM_TO_DREG;
                out->reg = out->dst.reg;
            } else if (out->dst.mode == NG_M68K_EA_ADISP) {
                out->form = NG_M68K_FORM_AREG_DISP;
                out->reg = out->dst.reg;
                out->displacement = out->dst.displacement;
            } else if (out->dst.mode == NG_M68K_EA_ABS_W ||
                       out->dst.mode == NG_M68K_EA_ABS_L) {
                out->form = NG_M68K_FORM_ABS;
                out->absolute_addr = out->dst.absolute_addr;
            }
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xFF00u) == 0x0C00u) {
        uint8_t size_code = (uint8_t)((op >> 6) & 3u);
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        uint8_t imm_len;
        uint32_t ext_addr;
        if (size_code != 3u && ea_mode != 1u &&
            !(ea_mode == 7u && ea_reg >= 2u)) {
            out->mnemonic = NG_M68K_CMPI;
            out->size = size_from_opcode_bits(size_code);
            if (out->size == NG_M68K_SIZE_LONG) {
                out->immediate = ng_program_rom_read32(rom, addr + 2u);
                imm_len = 4u;
            } else {
                out->immediate = ng_program_rom_read16(rom, addr + 2u);
                if (out->size == NG_M68K_SIZE_BYTE) {
                    out->immediate &= 0xFFu;
                }
                imm_len = 2u;
            }
            ext_addr = addr + 2u + imm_len;
            out->byte_length = (uint8_t)(2u + imm_len +
                decode_ea(rom, ext_addr, ea_mode, ea_reg,
                          out->size, &out->dst));
            if (out->dst.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
                return finish_decode(rom, addr, out);
            }
            if (out->dst.mode == NG_M68K_EA_DREG) {
                out->form = NG_M68K_FORM_IMM_TO_DREG;
                out->reg = out->dst.reg;
            } else if (out->dst.mode == NG_M68K_EA_ADISP) {
                out->form = NG_M68K_FORM_AREG_DISP;
                out->reg = out->dst.reg;
                out->displacement = out->dst.displacement;
            } else if (out->dst.mode == NG_M68K_EA_ABS_W ||
                       out->dst.mode == NG_M68K_EA_ABS_L) {
                out->form = NG_M68K_FORM_ABS;
                out->absolute_addr = out->dst.absolute_addr;
            }
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xFF00u) == 0x0200u) {
        uint8_t size_code = (uint8_t)((op >> 6) & 3u);
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        uint8_t imm_len;
        uint32_t ext_addr;
        if (size_code != 3u && ea_mode != 1u &&
            !(ea_mode == 7u && ea_reg >= 2u)) {
            out->mnemonic = NG_M68K_ANDI;
            out->size = size_from_opcode_bits(size_code);
            if (out->size == NG_M68K_SIZE_LONG) {
                out->immediate = ng_program_rom_read32(rom, addr + 2u);
                imm_len = 4u;
            } else {
                out->immediate = ng_program_rom_read16(rom, addr + 2u);
                if (out->size == NG_M68K_SIZE_BYTE) {
                    out->immediate &= 0xFFu;
                }
                imm_len = 2u;
            }
            ext_addr = addr + 2u + imm_len;
            out->byte_length = (uint8_t)(2u + imm_len +
                decode_ea(rom, ext_addr, ea_mode, ea_reg,
                          out->size, &out->dst));
            if (out->dst.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
                return finish_decode(rom, addr, out);
            }
            if (out->dst.mode == NG_M68K_EA_DREG) {
                out->form = NG_M68K_FORM_IMM_TO_DREG;
                out->reg = out->dst.reg;
            } else if (out->dst.mode == NG_M68K_EA_ADISP) {
                out->form = NG_M68K_FORM_AREG_DISP;
                out->reg = out->dst.reg;
                out->displacement = out->dst.displacement;
            } else if (out->dst.mode == NG_M68K_EA_ABS_W ||
                       out->dst.mode == NG_M68K_EA_ABS_L) {
                out->form = NG_M68K_FORM_ABS;
                out->absolute_addr = out->dst.absolute_addr;
            }
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xFFF8u) == 0x0228u) {
        out->mnemonic = NG_M68K_ANDI;
        out->byte_length = 6;
        out->size = NG_M68K_SIZE_BYTE;
        out->form = NG_M68K_FORM_AREG_DISP;
        out->reg = (uint8_t)(op & 7u);
        out->immediate = ng_program_rom_read16(rom, addr + 2u) & 0xFFu;
        out->displacement = sign16(ng_program_rom_read16(rom, addr + 4u));
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFFF8u) == 0x23C8u) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 6;
        out->size = NG_M68K_SIZE_LONG;
        out->reg = (uint8_t)(op & 7u);
        out->form = NG_M68K_FORM_AREG_TO_ABS;
        out->absolute_addr = ng_program_rom_read32(rom, addr + 2u);
        return finish_decode(rom, addr, out);
    }
    if (op == 0x33FCu) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 8;
        out->size = NG_M68K_SIZE_WORD;
        out->form = NG_M68K_FORM_IMM_TO_ABS;
        out->immediate = ng_program_rom_read16(rom, addr + 2u);
        out->absolute_addr = ng_program_rom_read32(rom, addr + 4u);
        return finish_decode(rom, addr, out);
    }
    if (op == 0x13FCu) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 8;
        out->size = NG_M68K_SIZE_BYTE;
        out->form = NG_M68K_FORM_IMM_TO_ABS;
        out->immediate = ng_program_rom_read16(rom, addr + 2u) & 0xFFu;
        out->absolute_addr = ng_program_rom_read32(rom, addr + 4u);
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFFF8u) == 0x13C0u) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 6;
        out->size = NG_M68K_SIZE_BYTE;
        out->form = NG_M68K_FORM_DREG_TO_ABS;
        out->reg = (uint8_t)(op & 7u);
        out->absolute_addr = ng_program_rom_read32(rom, addr + 2u);
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xF1FFu) == 0x1039u) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 6;
        out->size = NG_M68K_SIZE_BYTE;
        out->form = NG_M68K_FORM_ABS_TO_DREG;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->absolute_addr = ng_program_rom_read32(rom, addr + 2u);
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xF1F8u) == 0x1028u) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 4;
        out->size = NG_M68K_SIZE_BYTE;
        out->form = NG_M68K_FORM_AREG_DISP;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->src_reg = (uint8_t)(op & 7u);
        out->displacement = sign16(ng_program_rom_read16(rom, addr + 2u));
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xF1FFu) == 0x2039u) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 6;
        out->size = NG_M68K_SIZE_LONG;
        out->form = NG_M68K_FORM_ABS_TO_DREG;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->absolute_addr = ng_program_rom_read32(rom, addr + 2u);
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xF1FFu) == 0x303Cu) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 4;
        out->size = NG_M68K_SIZE_WORD;
        out->form = NG_M68K_FORM_IMM_TO_DREG;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->immediate = ng_program_rom_read16(rom, addr + 2u);
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xF1FFu) == 0x103Cu) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 4;
        out->size = NG_M68K_SIZE_BYTE;
        out->form = NG_M68K_FORM_IMM_TO_DREG;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->immediate = ng_program_rom_read16(rom, addr + 2u) & 0xFFu;
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFFF8u) == 0x0C00u) {
        out->mnemonic = NG_M68K_CMPI;
        out->byte_length = 4;
        out->size = NG_M68K_SIZE_BYTE;
        out->form = NG_M68K_FORM_IMM_TO_DREG;
        out->reg = (uint8_t)(op & 7u);
        out->immediate = ng_program_rom_read16(rom, addr + 2u) & 0xFFu;
        return finish_decode(rom, addr, out);
    }
    if (((op & 0xF100u) == 0xD100u ||
         (op & 0xF100u) == 0x9100u) &&
        (((op >> 3) & 7u) == 0u || ((op >> 3) & 7u) == 1u)) {
        uint8_t size_code = (uint8_t)((op >> 6) & 3u);
        if (size_code != 3u) {
            uint8_t mem_form = (uint8_t)(((op >> 3) & 7u) == 1u);
            out->mnemonic = ((op & 0xF000u) == 0xD000u) ?
                NG_M68K_ADDX : NG_M68K_SUBX;
            out->byte_length = 2;
            if (size_code == 2u) {
                out->size = NG_M68K_SIZE_LONG;
            } else if (size_code == 1u) {
                out->size = NG_M68K_SIZE_WORD;
            } else {
                out->size = NG_M68K_SIZE_BYTE;
            }
            if (mem_form) {
                out->src.mode = NG_M68K_EA_APRE;
                out->src.reg = (uint8_t)(op & 7u);
                out->dst.mode = NG_M68K_EA_APRE;
                out->dst.reg = (uint8_t)((op >> 9) & 7u);
            } else {
                out->form = NG_M68K_FORM_DREG_TO_DREG;
                out->src.mode = NG_M68K_EA_DREG;
                out->src.reg = (uint8_t)(op & 7u);
                out->dst.mode = NG_M68K_EA_DREG;
                out->dst.reg = (uint8_t)((op >> 9) & 7u);
            }
            out->src_reg = (uint8_t)(op & 7u);
            out->reg = (uint8_t)((op >> 9) & 7u);
            return finish_decode(rom, addr, out);
        }
    }
    if (((op & 0xF1F0u) == 0xC100u ||
         (op & 0xF1F0u) == 0x8100u) &&
        (((op >> 3) & 7u) == 0u || ((op >> 3) & 7u) == 1u)) {
        uint8_t mem_form = (uint8_t)(((op >> 3) & 7u) == 1u);
        out->mnemonic = ((op & 0xF000u) == 0xC000u) ?
            NG_M68K_ABCD : NG_M68K_SBCD;
        out->size = NG_M68K_SIZE_BYTE;
        out->byte_length = 2;
        if (mem_form) {
            out->src.mode = NG_M68K_EA_APRE;
            out->src.reg = (uint8_t)(op & 7u);
            out->dst.mode = NG_M68K_EA_APRE;
            out->dst.reg = (uint8_t)((op >> 9) & 7u);
        } else {
            out->form = NG_M68K_FORM_DREG_TO_DREG;
            out->src.mode = NG_M68K_EA_DREG;
            out->src.reg = (uint8_t)(op & 7u);
            out->dst.mode = NG_M68K_EA_DREG;
            out->dst.reg = (uint8_t)((op >> 9) & 7u);
        }
        out->src_reg = (uint8_t)(op & 7u);
        out->reg = (uint8_t)((op >> 9) & 7u);
        return finish_decode(rom, addr, out);
    }
    if (op == 0xD040u) {
        out->mnemonic = NG_M68K_ADD;
        out->byte_length = 2;
        out->size = NG_M68K_SIZE_WORD;
        out->form = NG_M68K_FORM_DREG_TO_DREG;
        out->src_reg = 0;
        out->reg = 0;
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xF000u) == 0x5000u && ((op >> 6) & 3u) == 3u) {
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        out->condition = (uint8_t)((op >> 8) & 0xFu);
        if (ea_mode == 1u) {
            out->mnemonic = NG_M68K_DBCC;
            out->byte_length = 4;
            out->size = NG_M68K_SIZE_WORD;
            out->reg = ea_reg;
            out->displacement = sign16(ng_program_rom_read16(rom, addr + 2u));
            out->target = (uint32_t)((int32_t)(addr + 2u) + (int32_t)out->displacement);
            return finish_decode(rom, addr, out);
        }
        if (is_data_alterable_ea(ea_mode, ea_reg)) {
            out->mnemonic = NG_M68K_SCC;
            out->size = NG_M68K_SIZE_BYTE;
            out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->dst));
            if (out->dst.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
            }
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xF000u) == 0x5000u) {
        uint8_t size_code = (uint8_t)((op >> 6) & 3u);
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if (size_code != 3u &&
            !(ea_mode == 1u && size_code == 0u) &&
            (ea_mode == 1u || is_data_alterable_ea(ea_mode, ea_reg))) {
            out->mnemonic = (op & 0x0100u) ? NG_M68K_SUBQ : NG_M68K_ADDQ;
            out->byte_length = 2;
            out->size = size_from_opcode_bits(size_code);
            out->immediate = (uint8_t)((op >> 9) & 7u);
            if (out->immediate == 0) {
                out->immediate = 8;
            }
            out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->dst));
            if (out->dst.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
                return finish_decode(rom, addr, out);
            }
            if (out->dst.mode == NG_M68K_EA_DREG) {
                out->form = NG_M68K_FORM_DREG;
                out->reg = out->dst.reg;
            } else if (out->dst.mode == NG_M68K_EA_ABS_W ||
                       out->dst.mode == NG_M68K_EA_ABS_L) {
                out->form = NG_M68K_FORM_ABS;
                out->absolute_addr = out->dst.absolute_addr;
            } else if (out->dst.mode == NG_M68K_EA_ADISP) {
                out->form = NG_M68K_FORM_AREG_DISP;
                out->reg = out->dst.reg;
                out->displacement = out->dst.displacement;
            }
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xFF00u) == 0x4200u) {
        uint8_t size_code = (uint8_t)((op >> 6) & 3u);
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if (size_code != 3u && is_data_alterable_ea(ea_mode, ea_reg)) {
            out->mnemonic = NG_M68K_CLR;
            out->size = size_from_opcode_bits(size_code);
            out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->dst));
            if (out->dst.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
                return finish_decode(rom, addr, out);
            }
            populate_unary_dst_legacy_fields(out);
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xFF00u) == 0x4000u ||
        (op & 0xFF00u) == 0x4400u ||
        (op & 0xFF00u) == 0x4600u) {
        uint8_t size_code = (uint8_t)((op >> 6) & 3u);
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if (size_code != 3u && ea_mode != 1u &&
            !(ea_mode == 7u && ea_reg >= 2u)) {
            out->mnemonic = ((op & 0xFF00u) == 0x4000u) ? NG_M68K_NEGX :
                (((op & 0xFF00u) == 0x4400u) ? NG_M68K_NEG : NG_M68K_NOT);
            out->size = size_from_opcode_bits(size_code);
            out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->dst));
            if (out->dst.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
                return finish_decode(rom, addr, out);
            }
            populate_unary_dst_legacy_fields(out);
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xFFC0u) == 0x4800u) {
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if (is_data_alterable_ea(ea_mode, ea_reg)) {
            out->mnemonic = NG_M68K_NBCD;
            out->size = NG_M68K_SIZE_BYTE;
            out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->dst));
            if (out->dst.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
            }
            populate_unary_dst_legacy_fields(out);
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xFFF8u) == 0x4840u) {
        out->mnemonic = NG_M68K_SWAP;
        out->size = NG_M68K_SIZE_WORD;
        out->byte_length = 2;
        out->form = NG_M68K_FORM_DREG;
        out->reg = (uint8_t)(op & 7u);
        out->dst.mode = NG_M68K_EA_DREG;
        out->dst.reg = out->reg;
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFFF8u) == 0x4880u ||
        (op & 0xFFF8u) == 0x48C0u) {
        out->mnemonic = NG_M68K_EXT;
        out->size = (op & 0x0040u) ? NG_M68K_SIZE_LONG : NG_M68K_SIZE_WORD;
        out->byte_length = 2;
        out->form = NG_M68K_FORM_DREG;
        out->reg = (uint8_t)(op & 7u);
        out->dst.mode = NG_M68K_EA_DREG;
        out->dst.reg = out->reg;
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFF80u) == 0x4880u) {
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if (is_movem_reg_to_mem_ea(ea_mode, ea_reg)) {
            out->mnemonic = NG_M68K_MOVEM;
            out->size = (op & 0x0040u) ? NG_M68K_SIZE_LONG : NG_M68K_SIZE_WORD;
            out->immediate = ng_program_rom_read16(rom, addr + 2u);
            out->byte_length = (uint8_t)(4u + decode_ea(rom, addr + 4u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->dst));
            if (out->dst.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
            }
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xFF80u) == 0x4C80u) {
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if (is_movem_mem_to_reg_ea(ea_mode, ea_reg)) {
            out->mnemonic = NG_M68K_MOVEM;
            out->size = (op & 0x0040u) ? NG_M68K_SIZE_LONG : NG_M68K_SIZE_WORD;
            out->immediate = ng_program_rom_read16(rom, addr + 2u);
            out->byte_length = (uint8_t)(4u + decode_ea(rom, addr + 4u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->src));
            if (out->src.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
            }
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xFFC0u) == 0x4840u) {
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if (is_control_ea(ea_mode, ea_reg)) {
            out->mnemonic = NG_M68K_PEA;
            out->size = NG_M68K_SIZE_LONG;
            out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->src));
            if (out->src.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
            }
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xFFC0u) == 0x4AC0u) {
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if (is_data_alterable_ea(ea_mode, ea_reg)) {
            out->mnemonic = NG_M68K_TAS;
            out->size = NG_M68K_SIZE_BYTE;
            out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->dst));
            if (out->dst.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
            }
            populate_unary_dst_legacy_fields(out);
            return finish_decode(rom, addr, out);
        }
    }
    if ((op & 0xFF00u) == 0x4A00u) {
        uint8_t size_code = (uint8_t)((op >> 6) & 3u);
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if (size_code != 3u && ea_mode != 1u &&
            !(ea_mode == 7u && ea_reg >= 2u)) {
            out->mnemonic = NG_M68K_TST;
            out->size = size_from_opcode_bits(size_code);
            out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->src));
            if (out->src.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
                return finish_decode(rom, addr, out);
            }
            if (out->src.mode == NG_M68K_EA_DREG) {
                out->form = NG_M68K_FORM_DREG;
                out->reg = out->src.reg;
            } else if (out->src.mode == NG_M68K_EA_ABS_W ||
                       out->src.mode == NG_M68K_EA_ABS_L) {
                out->form = NG_M68K_FORM_ABS;
                out->absolute_addr = out->src.absolute_addr;
            } else if (out->src.mode == NG_M68K_EA_ADISP) {
                out->form = NG_M68K_FORM_AREG_DISP;
                out->reg = out->src.reg;
                out->displacement = out->src.displacement;
            }
            return finish_decode(rom, addr, out);
        }
    }
    if (op == 0x4239u || op == 0x4279u || op == 0x42B9u) {
        out->mnemonic = NG_M68K_CLR;
        out->byte_length = 6;
        if (op == 0x4239u) {
            out->size = NG_M68K_SIZE_BYTE;
        } else if (op == 0x42B9u) {
            out->size = NG_M68K_SIZE_LONG;
        } else {
            out->size = NG_M68K_SIZE_WORD;
        }
        out->form = NG_M68K_FORM_ABS;
        out->absolute_addr = ng_program_rom_read32(rom, addr + 2u);
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFF38u) == 0x4200u &&
        (op & 0x00C0u) != 0x00C0u) {
        out->mnemonic = NG_M68K_CLR;
        out->byte_length = 2;
        out->form = NG_M68K_FORM_DREG;
        out->reg = (uint8_t)(op & 7u);
        if ((op & 0x00C0u) == 0x0080u) {
            out->size = NG_M68K_SIZE_LONG;
        } else if ((op & 0x00C0u) == 0x0040u) {
            out->size = NG_M68K_SIZE_WORD;
        } else {
            out->size = NG_M68K_SIZE_BYTE;
        }
        return finish_decode(rom, addr, out);
    }
    if (op == 0x4A39u || op == 0x4A79u || op == 0x4AB9u) {
        out->mnemonic = NG_M68K_TST;
        out->byte_length = 6;
        if (op == 0x4A39u) {
            out->size = NG_M68K_SIZE_BYTE;
        } else if (op == 0x4AB9u) {
            out->size = NG_M68K_SIZE_LONG;
        } else {
            out->size = NG_M68K_SIZE_WORD;
        }
        out->form = NG_M68K_FORM_ABS;
        out->absolute_addr = ng_program_rom_read32(rom, addr + 2u);
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xFFF8u) == 0x4A28u) {
        out->mnemonic = NG_M68K_TST;
        out->byte_length = 4;
        out->size = NG_M68K_SIZE_BYTE;
        out->form = NG_M68K_FORM_AREG_DISP;
        out->reg = (uint8_t)(op & 7u);
        out->displacement = sign16(ng_program_rom_read16(rom, addr + 2u));
        return finish_decode(rom, addr, out);
    }
    if ((op & 0xF000u) == 0xA000u ||
        (op & 0xF000u) == 0xF000u) {
        out->mnemonic = NG_M68K_ILLEGAL;
        out->immediate = ((op & 0xF000u) == 0xA000u) ? 10u : 11u;
        return finish_decode(rom, addr, out);
    }

    return finish_decode(rom, addr, out);
}

const char *ng_m68k_mnemonic_name(NgM68kMnemonic mnemonic) {
    switch (mnemonic) {
    case NG_M68K_INVALID: return "INVALID";
    case NG_M68K_UNKNOWN: return "UNKNOWN";
    case NG_M68K_NOP: return "NOP";
    case NG_M68K_ILLEGAL: return "ILLEGAL";
    case NG_M68K_RESET: return "RESET";
    case NG_M68K_STOP: return "STOP";
    case NG_M68K_RTS: return "RTS";
    case NG_M68K_RTE: return "RTE";
    case NG_M68K_RTR: return "RTR";
    case NG_M68K_TRAP: return "TRAP";
    case NG_M68K_TRAPV: return "TRAPV";
    case NG_M68K_JMP: return "JMP";
    case NG_M68K_JSR: return "JSR";
    case NG_M68K_LINK: return "LINK";
    case NG_M68K_UNLK: return "UNLK";
    case NG_M68K_BRA: return "BRA";
    case NG_M68K_BSR: return "BSR";
    case NG_M68K_BCC: return "BCC";
    case NG_M68K_SCC: return "SCC";
    case NG_M68K_DBCC: return "DBCC";
    case NG_M68K_CHK: return "CHK";
    case NG_M68K_LEA: return "LEA";
    case NG_M68K_MOVEA: return "MOVEA";
    case NG_M68K_MOVEQ: return "MOVEQ";
    case NG_M68K_MOVE: return "MOVE";
    case NG_M68K_MOVEM: return "MOVEM";
    case NG_M68K_MOVEP: return "MOVEP";
    case NG_M68K_MOVE_SR: return "MOVE_SR";
    case NG_M68K_MOVE_CCR: return "MOVE_CCR";
    case NG_M68K_MOVE_USP: return "MOVE_USP";
    case NG_M68K_ADD: return "ADD";
    case NG_M68K_ADDA: return "ADDA";
    case NG_M68K_ADDQ: return "ADDQ";
    case NG_M68K_ADDX: return "ADDX";
    case NG_M68K_ABCD: return "ABCD";
    case NG_M68K_SUB: return "SUB";
    case NG_M68K_SUBA: return "SUBA";
    case NG_M68K_SUBQ: return "SUBQ";
    case NG_M68K_SUBX: return "SUBX";
    case NG_M68K_SBCD: return "SBCD";
    case NG_M68K_CMP: return "CMP";
    case NG_M68K_CMPA: return "CMPA";
    case NG_M68K_CMPM: return "CMPM";
    case NG_M68K_OR: return "OR";
    case NG_M68K_AND: return "AND";
    case NG_M68K_EOR: return "EOR";
    case NG_M68K_MULU: return "MULU";
    case NG_M68K_MULS: return "MULS";
    case NG_M68K_DIVU: return "DIVU";
    case NG_M68K_DIVS: return "DIVS";
    case NG_M68K_EXG: return "EXG";
    case NG_M68K_CLR: return "CLR";
    case NG_M68K_NEG: return "NEG";
    case NG_M68K_NEGX: return "NEGX";
    case NG_M68K_NBCD: return "NBCD";
    case NG_M68K_NOT: return "NOT";
    case NG_M68K_EXT: return "EXT";
    case NG_M68K_SWAP: return "SWAP";
    case NG_M68K_ASL: return "ASL";
    case NG_M68K_ASR: return "ASR";
    case NG_M68K_LSL: return "LSL";
    case NG_M68K_LSR: return "LSR";
    case NG_M68K_ROXL: return "ROXL";
    case NG_M68K_ROXR: return "ROXR";
    case NG_M68K_ROL: return "ROL";
    case NG_M68K_ROR: return "ROR";
    case NG_M68K_PEA: return "PEA";
    case NG_M68K_TST: return "TST";
    case NG_M68K_TAS: return "TAS";
    case NG_M68K_ADDI: return "ADDI";
    case NG_M68K_SUBI: return "SUBI";
    case NG_M68K_CMPI: return "CMPI";
    case NG_M68K_ORI: return "ORI";
    case NG_M68K_ANDI: return "ANDI";
    case NG_M68K_EORI: return "EORI";
    case NG_M68K_BTST: return "BTST";
    case NG_M68K_BCHG: return "BCHG";
    case NG_M68K_BCLR: return "BCLR";
    case NG_M68K_BSET: return "BSET";
    case NG_M68K_ORI_TO_CCR: return "ORI_CCR";
    case NG_M68K_ORI_TO_SR: return "ORI_SR";
    case NG_M68K_ANDI_TO_CCR: return "ANDI_CCR";
    case NG_M68K_ANDI_TO_SR: return "ANDI_SR";
    case NG_M68K_EORI_TO_CCR: return "EORI_CCR";
    case NG_M68K_EORI_TO_SR: return "EORI_SR";
    default: return "?";
    }
}

static const char *imm_sr_ccr_op_name(NgM68kMnemonic mnemonic) {
    switch (mnemonic) {
    case NG_M68K_ORI_TO_CCR:
    case NG_M68K_ORI_TO_SR:
        return "ORI";
    case NG_M68K_ANDI_TO_CCR:
    case NG_M68K_ANDI_TO_SR:
        return "ANDI";
    case NG_M68K_EORI_TO_CCR:
    case NG_M68K_EORI_TO_SR:
        return "EORI";
    default:
        return "?";
    }
}

static const char *imm_sr_ccr_dst_name(NgM68kMnemonic mnemonic) {
    switch (mnemonic) {
    case NG_M68K_ORI_TO_CCR:
    case NG_M68K_ANDI_TO_CCR:
    case NG_M68K_EORI_TO_CCR:
        return "CCR";
    case NG_M68K_ORI_TO_SR:
    case NG_M68K_ANDI_TO_SR:
    case NG_M68K_EORI_TO_SR:
        return "SR";
    default:
        return "?";
    }
}

static void format_ea_operand(const NgM68kEa *ea,
                              uint8_t size,
                              char *out,
                              unsigned out_size) {
    (void)size;
    switch (ea->mode) {
    case NG_M68K_EA_DREG:
        snprintf(out, out_size, "D%u", ea->reg);
        break;
    case NG_M68K_EA_AREG:
        snprintf(out, out_size, "A%u", ea->reg);
        break;
    case NG_M68K_EA_AIND:
        snprintf(out, out_size, "(A%u)", ea->reg);
        break;
    case NG_M68K_EA_APOST:
        snprintf(out, out_size, "(A%u)+", ea->reg);
        break;
    case NG_M68K_EA_APRE:
        snprintf(out, out_size, "-(A%u)", ea->reg);
        break;
    case NG_M68K_EA_ADISP:
        snprintf(out, out_size, "($%X,A%u)",
                 (unsigned)(uint16_t)ea->displacement, ea->reg);
        break;
    case NG_M68K_EA_AINDEX:
        snprintf(out, out_size, "($%X,A%u,%c%u.%c)",
                 (unsigned)(uint8_t)ea->displacement,
                 ea->reg,
                 ea->index_is_addr ? 'A' : 'D',
                 ea->index_reg,
                 ea->index_is_long ? 'L' : 'W');
        break;
    case NG_M68K_EA_ABS_W:
    case NG_M68K_EA_ABS_L:
        snprintf(out, out_size, "$%06X", ea->absolute_addr & 0xFFFFFFu);
        break;
    case NG_M68K_EA_PC_DISP:
        snprintf(out, out_size, "($%06X,PC)", ea->absolute_addr & 0xFFFFFFu);
        break;
    case NG_M68K_EA_PC_INDEX:
        snprintf(out, out_size, "($%06X,PC,%c%u.%c)",
                 ea->absolute_addr & 0xFFFFFFu,
                 ea->index_is_addr ? 'A' : 'D',
                 ea->index_reg,
                 ea->index_is_long ? 'L' : 'W');
        break;
    case NG_M68K_EA_IMM:
        snprintf(out, out_size, "#$%X", (unsigned)ea->immediate);
        break;
    case NG_M68K_EA_NONE:
    default:
        snprintf(out, out_size, "?");
        break;
    }
}

void ng_m68k_format(const NgM68kInstr *instr, char *out, unsigned out_size) {
    switch (instr->mnemonic) {
    case NG_M68K_NOP:
    case NG_M68K_RTS:
    case NG_M68K_RTE:
    case NG_M68K_RTR:
    case NG_M68K_RESET:
    case NG_M68K_TRAPV:
        snprintf(out, out_size, "%s", ng_m68k_mnemonic_name(instr->mnemonic));
        break;
    case NG_M68K_ILLEGAL:
        if (instr->immediate == 10u) {
            snprintf(out, out_size, "A-LINE $%04X", instr->opcode);
        } else if (instr->immediate == 11u) {
            snprintf(out, out_size, "F-LINE $%04X", instr->opcode);
        } else {
            snprintf(out, out_size, "ILLEGAL");
        }
        break;
    case NG_M68K_STOP:
        snprintf(out, out_size, "STOP #$%04X", (unsigned)(instr->immediate & 0xFFFFu));
        break;
    case NG_M68K_TRAP:
        snprintf(out, out_size, "TRAP #%u", (unsigned)(instr->immediate & 0x0Fu));
        break;
    case NG_M68K_JMP:
    case NG_M68K_JSR:
        if (instr->src.mode != NG_M68K_EA_NONE &&
            instr->form != NG_M68K_FORM_ABS &&
            instr->form != NG_M68K_FORM_PC_RELATIVE) {
            char src[64];
            format_ea_operand(&instr->src, instr->size, src, (unsigned)sizeof(src));
            snprintf(out, out_size, "%s %s",
                     ng_m68k_mnemonic_name(instr->mnemonic), src);
            break;
        }
        snprintf(out, out_size, "%s $%06X",
                 ng_m68k_mnemonic_name(instr->mnemonic), instr->target & 0xFFFFFFu);
        break;
    case NG_M68K_LINK:
        snprintf(out, out_size, "LINK A%u,#%d", instr->reg, instr->displacement);
        break;
    case NG_M68K_UNLK:
        snprintf(out, out_size, "UNLK A%u", instr->reg);
        break;
    case NG_M68K_BRA:
    case NG_M68K_BSR:
        snprintf(out, out_size, "%s $%06X",
                 ng_m68k_mnemonic_name(instr->mnemonic), instr->target & 0xFFFFFFu);
        break;
    case NG_M68K_BCC:
        snprintf(out, out_size, "Bcc.%X $%06X",
                 instr->condition, instr->target & 0xFFFFFFu);
        break;
    case NG_M68K_SCC:
        if (instr->dst.mode != NG_M68K_EA_NONE) {
            char dst[64];
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "Scc.%X %s", instr->condition, dst);
        } else {
            snprintf(out, out_size, "Scc.%X ?", instr->condition);
        }
        break;
    case NG_M68K_DBCC:
        snprintf(out, out_size, "DBcc.%X D%u,$%06X",
                 instr->condition, instr->reg, instr->target & 0xFFFFFFu);
        break;
    case NG_M68K_CHK:
        if (instr->src.mode != NG_M68K_EA_NONE) {
            char src[64];
            format_ea_operand(&instr->src, instr->size, src, (unsigned)sizeof(src));
            snprintf(out, out_size, "CHK.W %s,D%u", src, instr->reg);
        } else {
            snprintf(out, out_size, "CHK.W ?,D%u", instr->reg);
        }
        break;
    case NG_M68K_LEA:
        if (instr->src.mode != NG_M68K_EA_NONE) {
            char src[64];
            format_ea_operand(&instr->src, instr->size, src, (unsigned)sizeof(src));
            snprintf(out, out_size, "LEA %s,A%u", src, instr->reg);
        } else {
            snprintf(out, out_size, "LEA $%06X,A%u", instr->target & 0xFFFFFFu, instr->reg);
        }
        break;
    case NG_M68K_MOVEA:
        if (instr->form == NG_M68K_FORM_PC_INDEX_TO_AREG) {
            snprintf(out, out_size, "MOVEA.L ($%06X,PC,D%u.W),A%u",
                     instr->target & 0xFFFFFFu, instr->src_reg, instr->reg);
        } else {
            char src[64];
            char dst[64];
            format_ea_operand(&instr->src, instr->size, src, (unsigned)sizeof(src));
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "MOVEA.%c %s,%s",
                     instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W',
                     src, dst);
        }
        break;
    case NG_M68K_MOVEQ:
        snprintf(out, out_size, "MOVEQ #%d,D%u", (int32_t)instr->immediate, instr->reg);
        break;
    case NG_M68K_MOVEM:
        if (instr->dst.mode != NG_M68K_EA_NONE) {
            char dst[64];
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "MOVEM.%c #$%04X,%s",
                     instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W',
                     (unsigned)(instr->immediate & 0xFFFFu), dst);
        } else if (instr->src.mode != NG_M68K_EA_NONE) {
            char src[64];
            format_ea_operand(&instr->src, instr->size, src, (unsigned)sizeof(src));
            snprintf(out, out_size, "MOVEM.%c %s,#$%04X",
                     instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W',
                     src, (unsigned)(instr->immediate & 0xFFFFu));
        } else {
            snprintf(out, out_size, "MOVEM.%c #$%04X,?",
                     instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W',
                     (unsigned)(instr->immediate & 0xFFFFu));
        }
        break;
    case NG_M68K_MOVEP:
        if (instr->src.mode == NG_M68K_EA_DREG) {
            snprintf(out, out_size, "MOVEP.%c D%u,($%X,A%u)",
                     instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W',
                     instr->src.reg,
                     (unsigned)(uint16_t)instr->dst.displacement,
                     instr->dst.reg);
        } else {
            snprintf(out, out_size, "MOVEP.%c ($%X,A%u),D%u",
                     instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W',
                     (unsigned)(uint16_t)instr->src.displacement,
                     instr->src.reg,
                     instr->dst.reg);
        }
        break;
    case NG_M68K_MOVE_SR:
    case NG_M68K_MOVE_CCR:
        if (instr->dst.mode != NG_M68K_EA_NONE) {
            char dst[64];
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "MOVE %s,%s",
                     instr->mnemonic == NG_M68K_MOVE_SR ? "SR" : "CCR", dst);
        } else if (instr->src.mode != NG_M68K_EA_NONE) {
            char src[64];
            format_ea_operand(&instr->src, instr->size, src, (unsigned)sizeof(src));
            snprintf(out, out_size, "MOVE %s,%s",
                     src, instr->mnemonic == NG_M68K_MOVE_SR ? "SR" : "CCR");
        } else {
            snprintf(out, out_size, "MOVE ?,%s",
                     instr->mnemonic == NG_M68K_MOVE_SR ? "SR" : "CCR");
        }
        break;
    case NG_M68K_MOVE_USP:
        if (instr->src.mode == NG_M68K_EA_AREG) {
            snprintf(out, out_size, "MOVE A%u,USP", instr->src.reg);
        } else if (instr->dst.mode == NG_M68K_EA_AREG) {
            snprintf(out, out_size, "MOVE USP,A%u", instr->dst.reg);
        } else {
            snprintf(out, out_size, "MOVE USP,?");
        }
        break;
    case NG_M68K_BTST:
    case NG_M68K_BCHG:
    case NG_M68K_BCLR:
    case NG_M68K_BSET:
        if (instr->dst.mode != NG_M68K_EA_NONE) {
            char dst[64];
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            if (instr->src.mode == NG_M68K_EA_DREG) {
                snprintf(out, out_size, "%s D%u,%s",
                         ng_m68k_mnemonic_name(instr->mnemonic),
                         instr->src.reg, dst);
            } else {
                snprintf(out, out_size, "%s #%u,%s",
                         ng_m68k_mnemonic_name(instr->mnemonic),
                         (unsigned)instr->immediate, dst);
            }
        } else {
            snprintf(out, out_size, "%s #%u,$%06X",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     (unsigned)instr->immediate,
                     instr->absolute_addr & 0xFFFFFFu);
        }
        break;
    case NG_M68K_ORI_TO_CCR:
    case NG_M68K_ORI_TO_SR:
    case NG_M68K_ANDI_TO_CCR:
    case NG_M68K_ANDI_TO_SR:
    case NG_M68K_EORI_TO_CCR:
    case NG_M68K_EORI_TO_SR:
        snprintf(out, out_size, "%s #$%0*X,%s",
                 imm_sr_ccr_op_name(instr->mnemonic),
                 instr->size == NG_M68K_SIZE_BYTE ? 2 : 4,
                 (unsigned)instr->immediate,
                 imm_sr_ccr_dst_name(instr->mnemonic));
        break;
    case NG_M68K_MOVE:
        if (instr->form == NG_M68K_FORM_IMM_TO_ABS) {
            snprintf(out, out_size, "MOVE.%c #$%X,$%06X",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     (unsigned)instr->immediate, instr->absolute_addr & 0xFFFFFFu);
        } else if (instr->form == NG_M68K_FORM_AREG_TO_ABS) {
            snprintf(out, out_size, "MOVE.%c A%u,$%06X",
                     instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W',
                     instr->reg, instr->absolute_addr & 0xFFFFFFu);
        } else if (instr->form == NG_M68K_FORM_DREG_TO_ABS) {
            snprintf(out, out_size, "MOVE.%c D%u,$%06X",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     instr->reg, instr->absolute_addr & 0xFFFFFFu);
        } else if (instr->form == NG_M68K_FORM_AREG_DISP) {
            snprintf(out, out_size, "MOVE.%c ($%X,A%u),D%u",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     (unsigned)(uint16_t)instr->displacement,
                     instr->src_reg, instr->reg);
        } else if (instr->form == NG_M68K_FORM_ABS_TO_DREG) {
            snprintf(out, out_size, "MOVE.%c $%06X,D%u",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     instr->absolute_addr & 0xFFFFFFu, instr->reg);
        } else if (instr->form == NG_M68K_FORM_DREG_TO_DREG) {
            snprintf(out, out_size, "MOVE.%c D%u,D%u",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     instr->src_reg, instr->reg);
        } else if (instr->src.mode != NG_M68K_EA_NONE &&
                   instr->dst.mode != NG_M68K_EA_NONE) {
            char src[64];
            char dst[64];
            format_ea_operand(&instr->src, instr->size, src, (unsigned)sizeof(src));
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "MOVE.%c %s,%s",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     src, dst);
        } else {
            snprintf(out, out_size, "MOVE.%c #$%X,D%u",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     (unsigned)instr->immediate, instr->reg);
        }
        break;
    case NG_M68K_ADDA:
    case NG_M68K_SUBA:
    case NG_M68K_CMPA:
        if (instr->src.mode != NG_M68K_EA_NONE) {
            char src[64];
            format_ea_operand(&instr->src, instr->size, src, (unsigned)sizeof(src));
            snprintf(out, out_size, "%s.%c %s,A%u",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W',
                     src, instr->dst.reg);
        } else {
            snprintf(out, out_size, "%s.%c $%06X,A%u",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W',
                     instr->absolute_addr & 0xFFFFFFu, instr->reg);
        }
        break;
    case NG_M68K_OR:
    case NG_M68K_AND:
    case NG_M68K_EOR:
        if (instr->src.mode != NG_M68K_EA_NONE &&
            instr->dst.mode != NG_M68K_EA_NONE) {
            char src[64];
            char dst[64];
            format_ea_operand(&instr->src, instr->size, src, (unsigned)sizeof(src));
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "%s.%c %s,%s",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     src, dst);
        } else {
            snprintf(out, out_size, "%s", ng_m68k_mnemonic_name(instr->mnemonic));
        }
        break;
    case NG_M68K_MULU:
    case NG_M68K_MULS:
    case NG_M68K_DIVU:
    case NG_M68K_DIVS:
        if (instr->src.mode != NG_M68K_EA_NONE) {
            char src[64];
            format_ea_operand(&instr->src, instr->size, src, (unsigned)sizeof(src));
            snprintf(out, out_size, "%s.W %s,D%u",
                     ng_m68k_mnemonic_name(instr->mnemonic), src, instr->dst.reg);
        } else {
            snprintf(out, out_size, "%s.W ?,D%u",
                     ng_m68k_mnemonic_name(instr->mnemonic), instr->reg);
        }
        break;
    case NG_M68K_EXG:
        if (instr->src.mode != NG_M68K_EA_NONE &&
            instr->dst.mode != NG_M68K_EA_NONE) {
            char src[64];
            char dst[64];
            format_ea_operand(&instr->src, instr->size, src, (unsigned)sizeof(src));
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "EXG %s,%s", src, dst);
        } else {
            snprintf(out, out_size, "EXG ?");
        }
        break;
    case NG_M68K_ADD:
    case NG_M68K_SUB:
    case NG_M68K_CMP:
        if (instr->src.mode != NG_M68K_EA_NONE &&
            instr->dst.mode != NG_M68K_EA_NONE) {
            char src[64];
            char dst[64];
            format_ea_operand(&instr->src, instr->size, src, (unsigned)sizeof(src));
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "%s.%c %s,%s",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     src, dst);
        } else {
            snprintf(out, out_size, "%s.%c D%u,D%u",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     instr->src_reg, instr->reg);
        }
        break;
    case NG_M68K_CMPM:
        snprintf(out, out_size, "CMPM.%c (A%u)+,(A%u)+",
                 instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                 (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                 instr->src.reg, instr->dst.reg);
        break;
    case NG_M68K_ADDX:
    case NG_M68K_SUBX:
    case NG_M68K_ABCD:
    case NG_M68K_SBCD:
        if (instr->src.mode == NG_M68K_EA_APRE &&
            instr->dst.mode == NG_M68K_EA_APRE) {
            snprintf(out, out_size, "%s.%c -(A%u),-(A%u)",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     instr->src.reg, instr->dst.reg);
        } else {
            snprintf(out, out_size, "%s.%c D%u,D%u",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     instr->src_reg, instr->reg);
        }
        break;
    case NG_M68K_ADDQ:
    case NG_M68K_SUBQ:
        if (instr->dst.mode != NG_M68K_EA_NONE) {
            char dst[64];
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "%s.%c #%u,%s",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     (unsigned)instr->immediate, dst);
        } else {
            snprintf(out, out_size, "%s.%c #%u,D%u",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     (unsigned)instr->immediate, instr->reg);
        }
        break;
    case NG_M68K_CLR:
        if (instr->form == NG_M68K_FORM_DREG) {
            snprintf(out, out_size, "CLR.%c D%u",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     instr->reg);
        } else if (instr->dst.mode != NG_M68K_EA_NONE) {
            char dst[64];
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "CLR.%c %s",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     dst);
        } else {
            snprintf(out, out_size, "CLR.%c $%06X",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     instr->absolute_addr & 0xFFFFFFu);
        }
        break;
    case NG_M68K_NEG:
    case NG_M68K_NEGX:
    case NG_M68K_NBCD:
    case NG_M68K_NOT:
        if (instr->dst.mode != NG_M68K_EA_NONE) {
            char dst[64];
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "%s.%c %s",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     dst);
        } else if (instr->form == NG_M68K_FORM_DREG) {
            snprintf(out, out_size, "%s.%c D%u",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     instr->reg);
        } else {
            snprintf(out, out_size, "%s.%c $%06X",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     instr->absolute_addr & 0xFFFFFFu);
        }
        break;
    case NG_M68K_EXT:
        snprintf(out, out_size, "EXT.%c D%u",
                 instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W',
                 instr->reg);
        break;
    case NG_M68K_SWAP:
        snprintf(out, out_size, "SWAP D%u", instr->reg);
        break;
    case NG_M68K_ASL:
    case NG_M68K_ASR:
    case NG_M68K_LSL:
    case NG_M68K_LSR:
    case NG_M68K_ROXL:
    case NG_M68K_ROXR:
    case NG_M68K_ROL:
    case NG_M68K_ROR:
        if (instr->dst.mode != NG_M68K_EA_DREG) {
            char dst[64];
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "%s.W %s",
                     ng_m68k_mnemonic_name(instr->mnemonic), dst);
        } else if (instr->src.mode == NG_M68K_EA_DREG) {
            snprintf(out, out_size, "%s.%c D%u,D%u",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     instr->src.reg, instr->dst.reg);
        } else {
            snprintf(out, out_size, "%s.%c #%u,D%u",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     (unsigned)instr->immediate, instr->dst.reg);
        }
        break;
    case NG_M68K_PEA:
        if (instr->src.mode != NG_M68K_EA_NONE) {
            char src[64];
            format_ea_operand(&instr->src, instr->size, src, (unsigned)sizeof(src));
            snprintf(out, out_size, "PEA %s", src);
        } else {
            snprintf(out, out_size, "PEA ?");
        }
        break;
    case NG_M68K_TST:
        if (instr->src.mode != NG_M68K_EA_NONE) {
            char src[64];
            format_ea_operand(&instr->src, instr->size, src, (unsigned)sizeof(src));
            snprintf(out, out_size, "TST.%c %s",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     src);
        } else if (instr->form == NG_M68K_FORM_AREG_DISP) {
            snprintf(out, out_size, "TST.%c ($%X,A%u)",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     (unsigned)(uint16_t)instr->displacement,
                     instr->reg);
        } else {
            snprintf(out, out_size, "TST.%c $%06X",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     instr->absolute_addr & 0xFFFFFFu);
        }
        break;
    case NG_M68K_TAS:
        if (instr->dst.mode != NG_M68K_EA_NONE) {
            char dst[64];
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "TAS %s", dst);
        } else {
            snprintf(out, out_size, "TAS ?");
        }
        break;
    case NG_M68K_ADDI:
    case NG_M68K_SUBI:
    case NG_M68K_CMPI:
        if (instr->dst.mode != NG_M68K_EA_NONE) {
            char dst[64];
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "%s.%c #$%X,%s",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     (unsigned)instr->immediate, dst);
        } else {
            snprintf(out, out_size, "%s.%c #$%X,D%u",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     (unsigned)instr->immediate, instr->reg);
        }
        break;
    case NG_M68K_ORI:
    case NG_M68K_ANDI:
    case NG_M68K_EORI:
        if (instr->dst.mode != NG_M68K_EA_NONE) {
            char dst[64];
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "%s.%c #$%X,%s",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     (unsigned)instr->immediate, dst);
        } else if (instr->form == NG_M68K_FORM_AREG_DISP) {
            snprintf(out, out_size, "%s.%c #$%X,($%X,A%u)",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     (unsigned)instr->immediate,
                     (unsigned)(uint16_t)instr->displacement,
                     instr->reg);
        } else {
            snprintf(out, out_size, "%s", ng_m68k_mnemonic_name(instr->mnemonic));
        }
        break;
    case NG_M68K_UNKNOWN:
        snprintf(out, out_size, "DC.W $%04X", instr->opcode);
        break;
    case NG_M68K_INVALID:
    default:
        snprintf(out, out_size, "INVALID");
        break;
    }
}
