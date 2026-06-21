#pragma once

#include <stdint.h>

#include "m68k_decode.h"

uint8_t ng_m68k_base_cycles_for_opcode(uint16_t opcode);
uint8_t ng_m68k_base_cycles_for_instr(const NgM68kInstr *instr);
