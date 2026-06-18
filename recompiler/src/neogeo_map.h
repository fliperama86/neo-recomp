#pragma once

#include <stdint.h>

typedef enum NgAddressRegion {
    NG_REGION_P_ROM_FIXED,
    NG_REGION_WORK_RAM,
    NG_REGION_P_ROM_BANK,
    NG_REGION_BIOS,
    NG_REGION_UNKNOWN,
} NgAddressRegion;

NgAddressRegion ng_address_region(uint32_t addr);
const char *ng_address_region_name(NgAddressRegion region);

