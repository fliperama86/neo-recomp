#include "neogeo_map.h"

NgAddressRegion ng_address_region(uint32_t addr) {
    addr &= 0x00FFFFFFu;

    if (addr <= 0x000FFFFFu) {
        return NG_REGION_P_ROM_FIXED;
    }
    if (addr >= 0x00100000u && addr <= 0x0010FFFFu) {
        return NG_REGION_WORK_RAM;
    }
    if (addr >= 0x00200000u && addr <= 0x002FFFFFu) {
        return NG_REGION_P_ROM_BANK;
    }
    if (addr >= 0x00C00000u && addr <= 0x00C1FFFFu) {
        return NG_REGION_BIOS;
    }
    return NG_REGION_UNKNOWN;
}

const char *ng_address_region_name(NgAddressRegion region) {
    switch (region) {
    case NG_REGION_P_ROM_FIXED: return "p_rom_fixed";
    case NG_REGION_WORK_RAM: return "work_ram";
    case NG_REGION_P_ROM_BANK: return "p_rom_bank";
    case NG_REGION_BIOS: return "bios";
    case NG_REGION_UNKNOWN:
    default:
        return "unknown";
    }
}

