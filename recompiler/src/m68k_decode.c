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

static uint32_t branch_target(uint32_t base, uint16_t opcode, uint8_t *out_len) {
    uint8_t disp8 = (uint8_t)(opcode & 0xFFu);
    if (disp8 == 0) {
        *out_len = 4;
        return base + 2u + (int32_t)sign16(0);
    }
    *out_len = 2;
    return base + 2u + (int32_t)sign8(disp8);
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
        return 1;
    }
    if (decode_alu_ea_to_dreg(rom, addr, op, out)) {
        return 1;
    }

    if (op == 0x4E71u) {
        out->mnemonic = NG_M68K_NOP;
        return 1;
    }
    if (op == 0x4E75u) {
        out->mnemonic = NG_M68K_RTS;
        return 1;
    }
    if (op == 0x4EF9u || op == 0x4EB9u) {
        out->mnemonic = (op == 0x4EF9u) ? NG_M68K_JMP : NG_M68K_JSR;
        out->byte_length = 6;
        out->target = ng_program_rom_read32(rom, addr + 2u);
        return 1;
    }
    if (op == 0x4EFAu || op == 0x4EBAu) {
        out->mnemonic = (op == 0x4EFAu) ? NG_M68K_JMP : NG_M68K_JSR;
        out->byte_length = 4;
        out->form = NG_M68K_FORM_PC_RELATIVE;
        out->displacement = sign16(ng_program_rom_read16(rom, addr + 2u));
        out->target = (uint32_t)((int32_t)(addr + 2u) + (int32_t)out->displacement);
        return 1;
    }
    if ((op & 0xFFF8u) == 0x4ED0u) {
        out->mnemonic = NG_M68K_JMP;
        out->byte_length = 2;
        out->form = NG_M68K_FORM_AREG_INDIRECT;
        out->reg = (uint8_t)(op & 7u);
        return 1;
    }
    if ((op & 0xFF00u) == 0x6000u) {
        out->mnemonic = NG_M68K_BRA;
        out->target = branch_target(addr, op, &out->byte_length);
        if ((op & 0xFFu) == 0) {
            out->target = addr + 2u + (int32_t)sign16(ng_program_rom_read16(rom, addr + 2u));
        }
        return 1;
    }
    if ((op & 0xFF00u) == 0x6100u) {
        out->mnemonic = NG_M68K_BSR;
        out->target = branch_target(addr, op, &out->byte_length);
        if ((op & 0xFFu) == 0) {
            out->target = addr + 2u + (int32_t)sign16(ng_program_rom_read16(rom, addr + 2u));
        }
        return 1;
    }
    if ((op & 0xF000u) == 0x6000u) {
        out->mnemonic = NG_M68K_BCC;
        out->condition = (uint8_t)((op >> 8) & 0xFu);
        out->target = branch_target(addr, op, &out->byte_length);
        if ((op & 0xFFu) == 0) {
            out->target = addr + 2u + (int32_t)sign16(ng_program_rom_read16(rom, addr + 2u));
        }
        return 1;
    }
    if ((op & 0xF100u) == 0x7000u) {
        out->mnemonic = NG_M68K_MOVEQ;
        out->byte_length = 2;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->immediate = (uint32_t)(int32_t)sign8((uint8_t)(op & 0xFFu));
        return 1;
    }
    if ((op & 0xF1FFu) == 0x41FAu) {
        out->mnemonic = NG_M68K_LEA;
        out->byte_length = 4;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->target = addr + 4u + (int32_t)sign16(ng_program_rom_read16(rom, addr + 2u));
        return 1;
    }
    if ((op & 0xF1FFu) == 0x41F9u) {
        out->mnemonic = NG_M68K_LEA;
        out->byte_length = 6;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->target = ng_program_rom_read32(rom, addr + 2u);
        return 1;
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
        return 1;
    }
    if (op == 0x08B9u) {
        out->mnemonic = NG_M68K_BCLR;
        out->byte_length = 8;
        out->immediate = ng_program_rom_read16(rom, addr + 2u);
        out->absolute_addr = ng_program_rom_read32(rom, addr + 4u);
        return 1;
    }
    if (op == 0x027Cu) {
        out->mnemonic = NG_M68K_ANDI_TO_SR;
        out->byte_length = 4;
        out->immediate = ng_program_rom_read16(rom, addr + 2u);
        return 1;
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
                return 1;
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
            return 1;
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
        return 1;
    }
    if ((op & 0xFFF8u) == 0x23C8u) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 6;
        out->size = NG_M68K_SIZE_LONG;
        out->reg = (uint8_t)(op & 7u);
        out->form = NG_M68K_FORM_AREG_TO_ABS;
        out->absolute_addr = ng_program_rom_read32(rom, addr + 2u);
        return 1;
    }
    if (op == 0x33FCu) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 8;
        out->size = NG_M68K_SIZE_WORD;
        out->form = NG_M68K_FORM_IMM_TO_ABS;
        out->immediate = ng_program_rom_read16(rom, addr + 2u);
        out->absolute_addr = ng_program_rom_read32(rom, addr + 4u);
        return 1;
    }
    if (op == 0x13FCu) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 8;
        out->size = NG_M68K_SIZE_BYTE;
        out->form = NG_M68K_FORM_IMM_TO_ABS;
        out->immediate = ng_program_rom_read16(rom, addr + 2u) & 0xFFu;
        out->absolute_addr = ng_program_rom_read32(rom, addr + 4u);
        return 1;
    }
    if ((op & 0xFFF8u) == 0x13C0u) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 6;
        out->size = NG_M68K_SIZE_BYTE;
        out->form = NG_M68K_FORM_DREG_TO_ABS;
        out->reg = (uint8_t)(op & 7u);
        out->absolute_addr = ng_program_rom_read32(rom, addr + 2u);
        return 1;
    }
    if ((op & 0xF1FFu) == 0x1039u) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 6;
        out->size = NG_M68K_SIZE_BYTE;
        out->form = NG_M68K_FORM_ABS_TO_DREG;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->absolute_addr = ng_program_rom_read32(rom, addr + 2u);
        return 1;
    }
    if ((op & 0xF1F8u) == 0x1028u) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 4;
        out->size = NG_M68K_SIZE_BYTE;
        out->form = NG_M68K_FORM_AREG_DISP;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->src_reg = (uint8_t)(op & 7u);
        out->displacement = sign16(ng_program_rom_read16(rom, addr + 2u));
        return 1;
    }
    if ((op & 0xF1FFu) == 0x2039u) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 6;
        out->size = NG_M68K_SIZE_LONG;
        out->form = NG_M68K_FORM_ABS_TO_DREG;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->absolute_addr = ng_program_rom_read32(rom, addr + 2u);
        return 1;
    }
    if ((op & 0xF1FFu) == 0x303Cu) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 4;
        out->size = NG_M68K_SIZE_WORD;
        out->form = NG_M68K_FORM_IMM_TO_DREG;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->immediate = ng_program_rom_read16(rom, addr + 2u);
        return 1;
    }
    if ((op & 0xF1FFu) == 0x103Cu) {
        out->mnemonic = NG_M68K_MOVE;
        out->byte_length = 4;
        out->size = NG_M68K_SIZE_BYTE;
        out->form = NG_M68K_FORM_IMM_TO_DREG;
        out->reg = (uint8_t)((op >> 9) & 7u);
        out->immediate = ng_program_rom_read16(rom, addr + 2u) & 0xFFu;
        return 1;
    }
    if ((op & 0xFFF8u) == 0x0C00u) {
        out->mnemonic = NG_M68K_CMPI;
        out->byte_length = 4;
        out->size = NG_M68K_SIZE_BYTE;
        out->form = NG_M68K_FORM_IMM_TO_DREG;
        out->reg = (uint8_t)(op & 7u);
        out->immediate = ng_program_rom_read16(rom, addr + 2u) & 0xFFu;
        return 1;
    }
    if ((op & 0xF138u) == 0xD100u) {
        uint8_t size_code = (uint8_t)((op >> 6) & 3u);
        if (size_code != 3u) {
            out->mnemonic = NG_M68K_ADDX;
            out->byte_length = 2;
            out->form = NG_M68K_FORM_DREG_TO_DREG;
            out->src_reg = (uint8_t)(op & 7u);
            out->reg = (uint8_t)((op >> 9) & 7u);
            if (size_code == 2u) {
                out->size = NG_M68K_SIZE_LONG;
            } else if (size_code == 1u) {
                out->size = NG_M68K_SIZE_WORD;
            } else {
                out->size = NG_M68K_SIZE_BYTE;
            }
            return 1;
        }
    }
    if (op == 0xD040u) {
        out->mnemonic = NG_M68K_ADD;
        out->byte_length = 2;
        out->size = NG_M68K_SIZE_WORD;
        out->form = NG_M68K_FORM_DREG_TO_DREG;
        out->src_reg = 0;
        out->reg = 0;
        return 1;
    }
    if ((op & 0xF138u) == 0x5100u) {
        uint8_t size_code = (uint8_t)((op >> 6) & 3u);
        out->mnemonic = NG_M68K_SUBQ;
        out->byte_length = 2;
        out->form = NG_M68K_FORM_DREG;
        out->reg = (uint8_t)(op & 7u);
        out->immediate = (uint8_t)((op >> 9) & 7u);
        if (out->immediate == 0) {
            out->immediate = 8;
        }
        if (size_code == 2u) {
            out->size = NG_M68K_SIZE_LONG;
        } else if (size_code == 1u) {
            out->size = NG_M68K_SIZE_WORD;
        } else {
            out->size = NG_M68K_SIZE_BYTE;
        }
        return 1;
    }
    if ((op & 0xFF00u) == 0x4200u) {
        uint8_t size_code = (uint8_t)((op >> 6) & 3u);
        uint8_t ea_mode = (uint8_t)((op >> 3) & 7u);
        uint8_t ea_reg = (uint8_t)(op & 7u);
        if (size_code != 3u) {
            out->mnemonic = NG_M68K_CLR;
            out->size = size_from_opcode_bits(size_code);
            out->byte_length = (uint8_t)(2u + decode_ea(rom, addr + 2u,
                                                        ea_mode, ea_reg,
                                                        out->size, &out->dst));
            if (out->dst.mode == NG_M68K_EA_NONE) {
                out->mnemonic = NG_M68K_UNKNOWN;
                out->byte_length = 2;
                return 1;
            }
            if (out->dst.mode == NG_M68K_EA_DREG) {
                out->form = NG_M68K_FORM_DREG;
                out->reg = out->dst.reg;
            } else if (out->dst.mode == NG_M68K_EA_ABS_W ||
                       out->dst.mode == NG_M68K_EA_ABS_L) {
                out->form = NG_M68K_FORM_ABS;
                out->absolute_addr = out->dst.absolute_addr;
            }
            return 1;
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
                return 1;
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
            return 1;
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
        return 1;
    }
    if ((op & 0xFF38u) == 0x4200u) {
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
        return 1;
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
        return 1;
    }
    if ((op & 0xFFF8u) == 0x4A28u) {
        out->mnemonic = NG_M68K_TST;
        out->byte_length = 4;
        out->size = NG_M68K_SIZE_BYTE;
        out->form = NG_M68K_FORM_AREG_DISP;
        out->reg = (uint8_t)(op & 7u);
        out->displacement = sign16(ng_program_rom_read16(rom, addr + 2u));
        return 1;
    }

    return 1;
}

const char *ng_m68k_mnemonic_name(NgM68kMnemonic mnemonic) {
    switch (mnemonic) {
    case NG_M68K_INVALID: return "INVALID";
    case NG_M68K_UNKNOWN: return "UNKNOWN";
    case NG_M68K_NOP: return "NOP";
    case NG_M68K_RTS: return "RTS";
    case NG_M68K_JMP: return "JMP";
    case NG_M68K_JSR: return "JSR";
    case NG_M68K_BRA: return "BRA";
    case NG_M68K_BSR: return "BSR";
    case NG_M68K_BCC: return "BCC";
    case NG_M68K_LEA: return "LEA";
    case NG_M68K_MOVEA: return "MOVEA";
    case NG_M68K_MOVEQ: return "MOVEQ";
    case NG_M68K_MOVE: return "MOVE";
    case NG_M68K_ADD: return "ADD";
    case NG_M68K_ADDX: return "ADDX";
    case NG_M68K_SUB: return "SUB";
    case NG_M68K_SUBQ: return "SUBQ";
    case NG_M68K_CMP: return "CMP";
    case NG_M68K_CLR: return "CLR";
    case NG_M68K_TST: return "TST";
    case NG_M68K_CMPI: return "CMPI";
    case NG_M68K_ANDI: return "ANDI";
    case NG_M68K_BCLR: return "BCLR";
    case NG_M68K_ANDI_TO_SR: return "ANDI_SR";
    default: return "?";
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
        snprintf(out, out_size, "%s", ng_m68k_mnemonic_name(instr->mnemonic));
        break;
    case NG_M68K_JMP:
    case NG_M68K_JSR:
        if (instr->form == NG_M68K_FORM_AREG_INDIRECT) {
            snprintf(out, out_size, "%s (A%u)",
                     ng_m68k_mnemonic_name(instr->mnemonic), instr->reg);
            break;
        }
        snprintf(out, out_size, "%s $%06X",
                 ng_m68k_mnemonic_name(instr->mnemonic), instr->target & 0xFFFFFFu);
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
    case NG_M68K_LEA:
        snprintf(out, out_size, "LEA $%06X,A%u", instr->target & 0xFFFFFFu, instr->reg);
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
    case NG_M68K_BCLR:
        snprintf(out, out_size, "BCLR #%u,$%06X",
                 (unsigned)instr->immediate, instr->absolute_addr & 0xFFFFFFu);
        break;
    case NG_M68K_ANDI_TO_SR:
        snprintf(out, out_size, "ANDI #$%04X,SR", (unsigned)instr->immediate);
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
    case NG_M68K_ADD:
    case NG_M68K_SUB:
    case NG_M68K_CMP:
        if (instr->src.mode != NG_M68K_EA_NONE &&
            instr->dst.mode == NG_M68K_EA_DREG) {
            char src[64];
            format_ea_operand(&instr->src, instr->size, src, (unsigned)sizeof(src));
            snprintf(out, out_size, "%s.%c %s,D%u",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     src, instr->dst.reg);
        } else {
            snprintf(out, out_size, "%s.%c D%u,D%u",
                     ng_m68k_mnemonic_name(instr->mnemonic),
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     instr->src_reg, instr->reg);
        }
        break;
    case NG_M68K_ADDX:
        snprintf(out, out_size, "ADDX.%c D%u,D%u",
                 instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                 (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                 instr->src_reg, instr->reg);
        break;
    case NG_M68K_SUBQ:
        snprintf(out, out_size, "SUBQ.%c #%u,D%u",
                 instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                 (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                 (unsigned)instr->immediate, instr->reg);
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
    case NG_M68K_CMPI:
        if (instr->dst.mode != NG_M68K_EA_NONE) {
            char dst[64];
            format_ea_operand(&instr->dst, instr->size, dst, (unsigned)sizeof(dst));
            snprintf(out, out_size, "CMPI.%c #$%X,%s",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     (unsigned)instr->immediate, dst);
        } else {
            snprintf(out, out_size, "CMPI.%c #$%X,D%u",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     (unsigned)instr->immediate, instr->reg);
        }
        break;
    case NG_M68K_ANDI:
        if (instr->form == NG_M68K_FORM_AREG_DISP) {
            snprintf(out, out_size, "ANDI.%c #$%X,($%X,A%u)",
                     instr->size == NG_M68K_SIZE_BYTE ? 'B' :
                     (instr->size == NG_M68K_SIZE_LONG ? 'L' : 'W'),
                     (unsigned)instr->immediate,
                     (unsigned)(uint16_t)instr->displacement,
                     instr->reg);
        } else {
            snprintf(out, out_size, "ANDI");
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
