#pragma once

#include "p_rom.h"

#define NG_FUNCTION_DISCOVERY_MAX_CANDIDATES 64u
#define NG_FUNCTION_DISCOVERY_MAX_INSTRUCTIONS 64u
#define NG_FUNCTION_DISCOVERY_TABLE_ENTRIES 4u

typedef struct NgFunctionDiscovery {
    uint32_t addrs[NG_FUNCTION_DISCOVERY_MAX_CANDIDATES];
    uint32_t count;
    int truncated;
} NgFunctionDiscovery;

void ng_function_discovery_init(NgFunctionDiscovery *discovery);
int ng_function_discovery_contains(const NgFunctionDiscovery *discovery,
                                   uint32_t addr);
int ng_function_discovery_add(NgFunctionDiscovery *discovery,
                              const NgProgramRom *rom,
                              uint32_t addr);
int ng_function_discover_from_entry(const NgProgramRom *rom,
                                    uint32_t entry,
                                    NgFunctionDiscovery *out);
