#pragma once

#include "m68k_decode.h"

typedef enum NgM68kJumpTableEntryKind {
    NG_M68K_JUMP_TABLE_ENTRY_ABS32,
    NG_M68K_JUMP_TABLE_ENTRY_ADDRESS,
    NG_M68K_JUMP_TABLE_ENTRY_INLINE_CODE,
    NG_M68K_JUMP_TABLE_ENTRY_DIRECT_DISPATCH,
} NgM68kJumpTableEntryKind;

typedef struct NgM68kJumpTablePattern {
    uint32_t table_addr;
    uint32_t max_entries;
    uint8_t index_reg;
    uint8_t target_reg;
    uint8_t entry_size;
    uint8_t include_terminal_entry;
    NgM68kJumpTableEntryKind entry_kind;
    uint32_t entry_signature;
} NgM68kJumpTablePattern;

typedef struct NgM68kIndexRegBoundState {
    uint32_t max_entries[8];
    uint32_t guarded_max_entries[8];
    uint32_t pending_cmp_entries[8];
    uint8_t scale_bytes[8];
    uint8_t valid[8];
    uint8_t guarded_valid[8];
    uint8_t pending_cmp_valid[8];
} NgM68kIndexRegBoundState;

typedef struct NgM68kStaticAregState {
    uint32_t target[8];
    uint8_t valid[8];
} NgM68kStaticAregState;

int ng_m68k_match_pc_index_jump_table(const NgM68kInstr *load,
                                      const NgM68kInstr *jump,
                                      NgM68kJumpTablePattern *out);

void ng_m68k_static_areg_reset(NgM68kStaticAregState *state);
void ng_m68k_static_areg_update(NgM68kStaticAregState *state,
                                const NgM68kInstr *instr);
void ng_m68k_index_reg_bound_reset(NgM68kIndexRegBoundState *state);
void ng_m68k_index_reg_bound_update(NgM68kIndexRegBoundState *state,
                                    const NgM68kInstr *instr);
void ng_m68k_jump_table_apply_index_bound(
    NgM68kJumpTablePattern *pattern,
    const NgM68kIndexRegBoundState *state);
int ng_m68k_match_static_index_jump_table(
    const NgM68kInstr *load,
    const NgM68kInstr *dispatch,
    const NgM68kStaticAregState *state,
    NgM68kJumpTablePattern *out);
int ng_m68k_match_static_index_branch_table(
    const NgM68kInstr *dispatch,
    const NgM68kStaticAregState *state,
    NgM68kJumpTablePattern *out);
int ng_m68k_match_pc_index_branch_table(
    const NgProgramRom *rom,
    const NgM68kInstr *dispatch,
    NgM68kJumpTablePattern *out);
int ng_m68k_match_pc_index_inline_code_table(
    const NgProgramRom *rom,
    const NgM68kInstr *previous,
    const NgM68kInstr *dispatch,
    NgM68kJumpTablePattern *out);
int ng_m68k_match_repeated_direct_dispatch_table(
    const NgProgramRom *rom,
    const NgM68kInstr *dispatch,
    NgM68kJumpTablePattern *out);
