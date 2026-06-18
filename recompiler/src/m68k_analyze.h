#pragma once

#include "m68k_decode.h"

typedef struct NgM68kJumpTablePattern {
    uint32_t table_addr;
    uint8_t index_reg;
    uint8_t target_reg;
    uint8_t entry_size;
} NgM68kJumpTablePattern;

int ng_m68k_match_pc_index_jump_table(const NgM68kInstr *load,
                                      const NgM68kInstr *jump,
                                      NgM68kJumpTablePattern *out);
