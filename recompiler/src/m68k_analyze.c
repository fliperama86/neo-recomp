#include "m68k_analyze.h"

#include <string.h>

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
    }
    return 1;
}
