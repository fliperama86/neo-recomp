#pragma once

#include "function_discovery.h"

#include <stdio.h>

void ng_c_symbol_for_addr(uint32_t addr, char *out, unsigned out_size);
int ng_emit_c_skeleton(FILE *out, const NgFunctionDiscovery *discovery);
