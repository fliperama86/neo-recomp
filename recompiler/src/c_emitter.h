#pragma once

#include "function_discovery.h"

#include <stdio.h>

typedef struct NgEmitDiagnostics {
    uint32_t unsupported_count;
    uint32_t decode_error_count;
    uint32_t first_unsupported_addr;
    uint32_t first_decode_error_addr;
} NgEmitDiagnostics;

void ng_c_symbol_for_addr(uint32_t addr, char *out, unsigned out_size);
void ng_emit_diagnostics_init(NgEmitDiagnostics *diagnostics);
int ng_emit_c_skeleton(FILE *out, const NgFunctionDiscovery *discovery);
int ng_emit_c_checked(FILE *out,
                      const NgProgramRom *rom,
                      const NgFunctionDiscovery *discovery,
                      NgEmitDiagnostics *diagnostics);
int ng_emit_c_shards(const char *out_dir,
                     const NgProgramRom *rom,
                     const NgFunctionDiscovery *discovery,
                     uint32_t functions_per_shard,
                     NgEmitDiagnostics *diagnostics);
int ng_emit_c(FILE *out,
              const NgProgramRom *rom,
              const NgFunctionDiscovery *discovery);
