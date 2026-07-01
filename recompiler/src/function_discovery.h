#pragma once

#include "game_config.h"
#include "p_rom.h"

#define NG_FUNCTION_DISCOVERY_MAX_CANDIDATES 131072u
#define NG_FUNCTION_DISCOVERY_MAX_INSTRUCTIONS 64u
#define NG_FUNCTION_DISCOVERY_TABLE_ENTRIES 4u
#define NG_FUNCTION_DISCOVERY_BANK_NONE 0xFFFFFFFFu

typedef struct NgFunctionDiscovery {
    uint32_t addrs[NG_FUNCTION_DISCOVERY_MAX_CANDIDATES];
    uint32_t banks[NG_FUNCTION_DISCOVERY_MAX_CANDIDATES];
    uint8_t banked[NG_FUNCTION_DISCOVERY_MAX_CANDIDATES];
    uint32_t count;
    uint32_t max_candidates;
    int truncated;
} NgFunctionDiscovery;

void ng_function_discovery_init(NgFunctionDiscovery *discovery);
void ng_function_discovery_set_max_candidates(NgFunctionDiscovery *discovery,
                                              uint32_t max_candidates);
int ng_function_discovery_contains(const NgFunctionDiscovery *discovery,
                                   uint32_t addr);
int ng_function_discovery_contains_bank(const NgFunctionDiscovery *discovery,
                                        uint32_t addr,
                                        uint32_t bank);
int ng_function_discovery_contains_for_rom(const NgFunctionDiscovery *discovery,
                                           const NgProgramRom *rom,
                                           uint32_t addr);
uint32_t ng_function_discovery_bank_at(const NgFunctionDiscovery *discovery,
                                       uint32_t index);
int ng_function_discovery_add(NgFunctionDiscovery *discovery,
                              const NgProgramRom *rom,
                              uint32_t addr);
int ng_function_discover_from_entry(const NgProgramRom *rom,
                                    uint32_t entry,
                                    NgFunctionDiscovery *out);
int ng_function_discover_from_seeds(const NgProgramRom *rom,
                                    const uint32_t *seeds,
                                    uint32_t seed_count,
                                    NgFunctionDiscovery *out);
int ng_function_discover_from_game_config(const NgProgramRom *rom,
                                          const uint32_t *seeds,
                                          uint32_t seed_count,
                                          const NgGameConfig *config,
                                          NgFunctionDiscovery *out);
int ng_function_discover_from_game_config_limited(
    const NgProgramRom *rom,
    const uint32_t *seeds,
    uint32_t seed_count,
    const NgGameConfig *config,
    uint32_t max_candidates,
    NgFunctionDiscovery *out);
