#include "m68k_analyze.h"

#include "m68k_validate.h"

#include <string.h>

static int has_static_code_target(const NgM68kInstr *instr) {
    if (!instr) {
        return 0;
    }
    switch (instr->src.mode) {
    case NG_M68K_EA_ABS_W:
    case NG_M68K_EA_ABS_L:
    case NG_M68K_EA_PC_DISP:
    case NG_M68K_EA_PC_INDEX:
        return 1;
    default:
        return 0;
    }
}

int ng_m68k_match_pc_index_jump_table(const NgM68kInstr *load,
                                      const NgM68kInstr *jump,
                                      NgM68kJumpTablePattern *out) {
    if (!load || !jump) {
        return 0;
    }
    if (load->mnemonic != NG_M68K_MOVEA ||
        load->form != NG_M68K_FORM_PC_INDEX_TO_AREG ||
        load->size != 4u) {
        return 0;
    }
    if (jump->mnemonic != NG_M68K_JMP ||
        jump->form != NG_M68K_FORM_AREG_INDIRECT ||
        load->reg != jump->reg) {
        return 0;
    }

    if (out) {
        memset(out, 0, sizeof(*out));
        out->table_addr = load->target;
        out->index_reg = load->src_reg;
        out->target_reg = load->reg;
        out->entry_size = 4;
        out->entry_kind = NG_M68K_JUMP_TABLE_ENTRY_ABS32;
    }
    return 1;
}

void ng_m68k_static_areg_reset(NgM68kStaticAregState *state) {
    if (state) {
        memset(state, 0, sizeof(*state));
    }
}

static int static_areg_load_target(const NgM68kInstr *instr,
                                   uint8_t *reg,
                                   uint32_t *target) {
    if (!instr || !reg || !target) {
        return 0;
    }

    if (instr->mnemonic == NG_M68K_LEA &&
        instr->dst.mode == NG_M68K_EA_AREG &&
        has_static_code_target(instr)) {
        *reg = instr->dst.reg;
        *target = instr->target;
        return 1;
    }

    if (instr->mnemonic == NG_M68K_MOVEA &&
        instr->size == 4u &&
        instr->dst.mode == NG_M68K_EA_AREG &&
        instr->src.mode == NG_M68K_EA_IMM) {
        *reg = instr->dst.reg;
        *target = instr->src.immediate;
        return 1;
    }

    return 0;
}

static int instr_writes_areg(const NgM68kInstr *instr, uint8_t *reg) {
    if (!instr || !reg) {
        return 0;
    }

    switch (instr->mnemonic) {
    case NG_M68K_LEA:
    case NG_M68K_MOVEA:
    case NG_M68K_ADDA:
    case NG_M68K_SUBA:
    case NG_M68K_ADDQ:
    case NG_M68K_SUBQ:
        if (instr->dst.mode == NG_M68K_EA_AREG) {
            *reg = instr->dst.reg;
            return 1;
        }
        break;
    default:
        break;
    }
    return 0;
}

void ng_m68k_static_areg_update(NgM68kStaticAregState *state,
                                const NgM68kInstr *instr) {
    if (!state || !instr) {
        return;
    }

    if (instr->mnemonic == NG_M68K_JSR ||
        instr->mnemonic == NG_M68K_BSR) {
        state->valid[0] = 0u;
        state->valid[1] = 0u;
        state->valid[7] = 0u;
        return;
    }

    uint8_t reg = 0;
    uint32_t target = 0;
    if (static_areg_load_target(instr, &reg, &target) && reg < 8u) {
        state->target[reg] = target;
        state->valid[reg] = 1u;
        return;
    }

    if (instr_writes_areg(instr, &reg) && reg < 8u) {
        state->valid[reg] = 0u;
    }
}

int ng_m68k_match_static_index_jump_table(
    const NgM68kInstr *load,
    const NgM68kInstr *dispatch,
    const NgM68kStaticAregState *state,
    NgM68kJumpTablePattern *out) {
    if (!load || !dispatch || !state) {
        return 0;
    }
    if (load->mnemonic != NG_M68K_MOVEA ||
        load->size != 4u ||
        load->src.mode != NG_M68K_EA_AINDEX ||
        load->dst.mode != NG_M68K_EA_AREG ||
        load->src.reg > 7u ||
        !state->valid[load->src.reg]) {
        return 0;
    }
    if ((dispatch->mnemonic != NG_M68K_JMP &&
         dispatch->mnemonic != NG_M68K_JSR) ||
        dispatch->src.mode != NG_M68K_EA_AIND ||
        dispatch->src.reg != load->dst.reg) {
        return 0;
    }

    if (out) {
        memset(out, 0, sizeof(*out));
        out->table_addr =
            (uint32_t)(state->target[load->src.reg] +
                       (int32_t)load->src.displacement) &
            0x00FFFFFFu;
        out->index_reg = load->src.index_reg;
        out->target_reg = load->dst.reg;
        out->entry_size = 4;
        out->entry_kind = NG_M68K_JUMP_TABLE_ENTRY_ABS32;
    }
    return 1;
}

int ng_m68k_match_static_index_branch_table(
    const NgM68kInstr *dispatch,
    const NgM68kStaticAregState *state,
    NgM68kJumpTablePattern *out) {
    if (!dispatch || !state) {
        return 0;
    }
    if ((dispatch->mnemonic != NG_M68K_JMP &&
         dispatch->mnemonic != NG_M68K_JSR) ||
        dispatch->src.mode != NG_M68K_EA_AINDEX ||
        dispatch->src.reg > 7u ||
        !state->valid[dispatch->src.reg]) {
        return 0;
    }

    if (out) {
        memset(out, 0, sizeof(*out));
        out->table_addr =
            (uint32_t)(state->target[dispatch->src.reg] +
                       (int32_t)dispatch->src.displacement) &
            0x00FFFFFFu;
        out->index_reg = dispatch->src.index_reg;
        out->target_reg = dispatch->src.reg;
        out->entry_kind = NG_M68K_JUMP_TABLE_ENTRY_ADDRESS;
    }
    return 1;
}

static int branch_table_terminal_entry_is_inline_body(const NgProgramRom *rom,
                                                      uint32_t table_addr) {
    if (!rom) {
        return 0;
    }

    uint32_t entry_addr = table_addr;
    uint32_t entry_size = 0u;
    uint32_t entry_count = 0u;
    uint32_t last_target = 0u;
    for (uint32_t i = 0; i < 32u; ++i) {
        NgM68kInstr entry;
        if (!ng_m68k_decode(rom, entry_addr, &entry) ||
            !ng_m68k_validate(&entry) ||
            entry.mnemonic != NG_M68K_BRA ||
            entry.byte_length == 0u) {
            break;
        }
        if (entry_size == 0u) {
            entry_size = entry.byte_length;
        } else if (entry.byte_length != entry_size) {
            break;
        }
        last_target = entry.target & 0x00FFFFFFu;
        ++entry_count;
        if (UINT32_MAX - entry_addr < entry_size) {
            return 0;
        }
        entry_addr += entry_size;
    }

    if (entry_count < 8u || entry_size == 0u ||
        last_target <= entry_addr ||
        last_target - entry_addr > 0x20u) {
        return 0;
    }

    NgM68kInstr terminal;
    return ng_m68k_decode(rom, entry_addr, &terminal) &&
           ng_m68k_validate(&terminal) &&
           terminal.byte_length != 0u &&
           terminal.mnemonic != NG_M68K_UNKNOWN &&
           terminal.mnemonic != NG_M68K_INVALID &&
           terminal.mnemonic != NG_M68K_BRA;
}

int ng_m68k_match_pc_index_branch_table(
    const NgProgramRom *rom,
    const NgM68kInstr *dispatch,
    NgM68kJumpTablePattern *out) {
    if (!dispatch) {
        return 0;
    }
    if ((dispatch->mnemonic != NG_M68K_JMP &&
         dispatch->mnemonic != NG_M68K_JSR) ||
        dispatch->src.mode != NG_M68K_EA_PC_INDEX) {
        return 0;
    }

    if (out) {
        memset(out, 0, sizeof(*out));
        out->table_addr = dispatch->src.absolute_addr & 0x00FFFFFFu;
        out->index_reg = dispatch->src.index_reg;
        out->target_reg = 0u;
        out->entry_kind = NG_M68K_JUMP_TABLE_ENTRY_ADDRESS;
        out->include_terminal_entry =
            branch_table_terminal_entry_is_inline_body(
                rom,
                out->table_addr) ? 1u : 0u;
    }
    return 1;
}

static int is_self_add_word(const NgM68kInstr *instr, uint8_t reg) {
    return instr &&
           instr->mnemonic == NG_M68K_ADD &&
           instr->size == 2u &&
           instr->src.mode == NG_M68K_EA_DREG &&
           instr->dst.mode == NG_M68K_EA_DREG &&
           instr->src.reg == reg &&
           instr->dst.reg == reg;
}

static int inline_code_entry_is_repeated(const NgProgramRom *rom,
                                         uint32_t table_addr,
                                         uint32_t *out_signature) {
    if (!rom || !out_signature ||
        !ng_program_rom_addr_is_mapped(rom, table_addr) ||
        !ng_program_rom_addr_is_mapped(rom, table_addr + 7u)) {
        return 0;
    }

    NgM68kInstr first;
    NgM68kInstr second;
    if (!ng_m68k_decode(rom, table_addr, &first) ||
        !ng_m68k_validate(&first) ||
        first.mnemonic == NG_M68K_BRA ||
        first.mnemonic == NG_M68K_BSR ||
        first.mnemonic == NG_M68K_JMP ||
        first.mnemonic == NG_M68K_JSR ||
        first.mnemonic == NG_M68K_RTS ||
        first.mnemonic == NG_M68K_RTE ||
        first.mnemonic == NG_M68K_RTR ||
        first.byte_length == 0u ||
        first.byte_length >= 4u) {
        return 0;
    }
    if (!ng_m68k_decode(rom, table_addr + first.byte_length, &second) ||
        !ng_m68k_validate(&second) ||
        second.byte_length == 0u ||
        first.byte_length + second.byte_length != 4u) {
        return 0;
    }

    uint32_t signature = ng_program_rom_read32(rom, table_addr);
    if (ng_program_rom_read32(rom, table_addr + 4u) != signature) {
        return 0;
    }

    *out_signature = signature;
    return 1;
}

int ng_m68k_match_pc_index_inline_code_table(
    const NgProgramRom *rom,
    const NgM68kInstr *previous,
    const NgM68kInstr *dispatch,
    NgM68kJumpTablePattern *out) {
    if (!rom || !previous || !dispatch) {
        return 0;
    }
    if (dispatch->mnemonic != NG_M68K_JMP ||
        dispatch->src.mode != NG_M68K_EA_PC_INDEX ||
        !is_self_add_word(previous, dispatch->src.index_reg)) {
        return 0;
    }

    uint32_t signature = 0u;
    uint32_t table_addr = dispatch->src.absolute_addr & 0x00FFFFFFu;
    if (!inline_code_entry_is_repeated(rom, table_addr, &signature)) {
        return 0;
    }

    if (out) {
        memset(out, 0, sizeof(*out));
        out->table_addr = table_addr;
        out->index_reg = dispatch->src.index_reg;
        out->target_reg = 0u;
        out->entry_size = 4u;
        out->entry_kind = NG_M68K_JUMP_TABLE_ENTRY_INLINE_CODE;
        out->entry_signature = signature;
    }
    return 1;
}

static int is_direct_dispatch_entry(const NgM68kInstr *instr,
                                    NgM68kMnemonic mnemonic,
                                    uint8_t byte_length) {
    if (!instr ||
        instr->mnemonic != mnemonic ||
        instr->byte_length == 0u ||
        instr->byte_length != byte_length) {
        return 0;
    }
    switch (instr->src.mode) {
    case NG_M68K_EA_ABS_W:
    case NG_M68K_EA_ABS_L:
    case NG_M68K_EA_PC_DISP:
        return 1;
    default:
        return 0;
    }
}

int ng_m68k_match_repeated_direct_dispatch_table(
    const NgProgramRom *rom,
    const NgM68kInstr *dispatch,
    NgM68kJumpTablePattern *out) {
    if (!rom ||
        !dispatch ||
        dispatch->mnemonic != NG_M68K_JMP ||
        dispatch->byte_length == 0u ||
        !is_direct_dispatch_entry(dispatch,
                                  dispatch->mnemonic,
                                  dispatch->byte_length)) {
        return 0;
    }

    NgM68kInstr second;
    NgM68kInstr third;
    uint32_t second_addr = dispatch->addr + dispatch->byte_length;
    uint32_t third_addr = second_addr + dispatch->byte_length;
    if (!ng_m68k_decode(rom, second_addr, &second) ||
        !ng_m68k_validate(&second) ||
        !is_direct_dispatch_entry(&second,
                                  dispatch->mnemonic,
                                  dispatch->byte_length) ||
        !ng_m68k_decode(rom, third_addr, &third) ||
        !ng_m68k_validate(&third) ||
        !is_direct_dispatch_entry(&third,
                                  dispatch->mnemonic,
                                  dispatch->byte_length)) {
        return 0;
    }

    if (out) {
        memset(out, 0, sizeof(*out));
        out->table_addr = dispatch->addr;
        out->entry_size = dispatch->byte_length;
        out->entry_kind = NG_M68K_JUMP_TABLE_ENTRY_DIRECT_DISPATCH;
    }
    return 1;
}
