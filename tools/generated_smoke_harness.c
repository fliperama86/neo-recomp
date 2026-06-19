#include "ngrecomp/neogeo_runtime.h"
#include "p_rom.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef NG_GENERATED_SMOKE_HAS_BIOS
void ng_bios_generated_call(uint32_t addr);

static int g_ng_generated_smoke_bios_dispatch_depth;

static void ng_generated_smoke_call_bios(uint32_t addr) {
    ++g_ng_generated_smoke_bios_dispatch_depth;
    ng_bios_generated_call(addr);
    --g_ng_generated_smoke_bios_dispatch_depth;
}
#endif

#ifdef NG_GENERATED_SMOKE_COMBINED_DISPATCH
void ng_cart_generated_call(uint32_t addr);

static int g_ng_generated_smoke_dispatch_active;
static int g_ng_generated_smoke_dispatch_pending;
static uint32_t g_ng_generated_smoke_dispatch_addr;

static void ng_generated_smoke_dispatch_one(uint32_t addr) {
    addr &= 0x00FFFFFFu;
    if (addr >= 0x00C00000u && addr <= 0x00CFFFFFu) {
        uint32_t bios_addr =
            0x00C00000u + ((addr - 0x00C00000u) % NG_NEO_SYSTEM_ROM_BYTES);
        ng_generated_smoke_call_bios(bios_addr);
        return;
    }
    ng_cart_generated_call(addr);
}

void ng_generated_call(uint32_t addr) {
    addr &= 0x00FFFFFFu;
    if (g_ng_generated_smoke_dispatch_active) {
        g_ng_generated_smoke_dispatch_addr = addr;
        g_ng_generated_smoke_dispatch_pending = 1;
        return;
    }
    g_ng_generated_smoke_dispatch_active = 1;
    g_ng_generated_smoke_dispatch_addr = addr;
    g_ng_generated_smoke_dispatch_pending = 1;
    while (g_ng_generated_smoke_dispatch_pending) {
        uint32_t next_addr = g_ng_generated_smoke_dispatch_addr;
        g_ng_generated_smoke_dispatch_pending = 0;
        ng_generated_smoke_dispatch_one(next_addr);
    }
    g_ng_generated_smoke_dispatch_active = 0;
}
#else
void ng_generated_call(uint32_t addr);
#endif

#ifdef NG_GENERATED_SMOKE_HAS_BIOS
static int ng_generated_smoke_bios_dispatch(uint32_t addr) {
    addr &= 0x00FFFFFFu;
    if (addr >= 0x00C00000u && addr <= 0x00CFFFFFu) {
        if (g_ng_generated_smoke_bios_dispatch_depth) {
            return 0;
        }
        ng_generated_smoke_call_bios(addr);
        return 1;
    }
    return 0;
}
#endif

static void ng_generated_smoke_byteswap_words(uint8_t *data, uint32_t size) {
    for (uint32_t i = 0; i + 1u < size; i += 2u) {
        uint8_t tmp = data[i];
        data[i] = data[i + 1u];
        data[i + 1u] = tmp;
    }
}

static int ng_generated_smoke_read_file(const char *path,
                                        uint8_t **out_data,
                                        uint32_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "failed to open file: %s\n", path);
        return 0;
    }
    if (fseek(f, 0, SEEK_END) != 0) {
        fclose(f);
        return 0;
    }
    long size = ftell(f);
    if (size < 0) {
        fclose(f);
        return 0;
    }
    if ((unsigned long)size > 0xFFFFFFFFul) {
        fprintf(stderr, "file too large: %s\n", path);
        fclose(f);
        return 0;
    }
    rewind(f);

    uint8_t *data = (uint8_t *)malloc((size_t)size ? (size_t)size : 1u);
    if (!data) {
        fclose(f);
        return 0;
    }
    if (fread(data, 1, (size_t)size, f) != (size_t)size) {
        free(data);
        fclose(f);
        return 0;
    }
    fclose(f);

    *out_data = data;
    *out_size = (uint32_t)size;
    return 1;
}

int ng_generated_smoke_run_with_bios(const char *neo_path,
                                     const char *bios_path) {
    if (!neo_path || !*neo_path) {
        fprintf(stderr, "missing .neo path\n");
        return 2;
    }
    if (bios_path && !*bios_path) {
        fprintf(stderr, "missing BIOS path\n");
        return 2;
    }
    ng_neogeo_set_external_dispatch(NULL);
    ng_neogeo_set_system_rom(NULL, 0);

    NgProgramRom rom;
    if (!ng_program_rom_load_neo(&rom, neo_path)) {
        fprintf(stderr, "failed to load neo image: %s\n", neo_path);
        return 1;
    }

    uint32_t cart_entry = 0;
    if (!ng_program_rom_cart_entry(&rom, &cart_entry)) {
        fprintf(stderr, "failed to locate cartridge entry in %s\n", neo_path);
        ng_program_rom_free(&rom);
        return 1;
    }

    uint8_t *bios_data = NULL;
    uint32_t bios_size = 0;
    if (bios_path) {
        if (!ng_generated_smoke_read_file(bios_path, &bios_data, &bios_size)) {
            fprintf(stderr, "failed to load BIOS image: %s\n", bios_path);
            ng_program_rom_free(&rom);
            return 1;
        }
        ng_generated_smoke_byteswap_words(bios_data, bios_size);
    }

    ng_neogeo_reset_runtime();
    ng_neogeo_set_auto_vblank_interval(bios_path ? 256u : 0u);
    ng_neogeo_set_program_rom(rom.data, rom.size);
    ng_neogeo_set_system_rom(bios_data, bios_size);
#ifdef NG_GENERATED_SMOKE_HAS_BIOS
    ng_neogeo_set_external_dispatch(bios_path ?
        ng_generated_smoke_bios_dispatch : NULL);
#else
    ng_neogeo_set_external_dispatch(NULL);
    if (bios_path) {
        fprintf(stderr,
                "BIOS image loaded for bus reads; rebuild harness with "
                "NG_GENERATED_SMOKE_HAS_BIOS to execute BIOS code\n");
    }
#endif
    memset(&g_ng_m68k, 0, sizeof(g_ng_m68k));
    g_ng_m68k.ssp = ng_program_rom_initial_ssp(&rom);
    g_ng_m68k.usp = 0;
    g_ng_m68k.a[7] = g_ng_m68k.ssp;
    g_ng_m68k.sr = 0x2700u;

    fprintf(stderr,
            "starting cart entry $%06X ssp=$%08X\n",
            cart_entry & 0x00FFFFFFu,
            g_ng_m68k.ssp);
    ng_generated_call(cart_entry);
    fprintf(stderr,
            "returned pc=$%06X sr=$%04X sp=$%08X\n",
            g_ng_m68k.pc & 0x00FFFFFFu,
            g_ng_m68k.sr,
            g_ng_m68k.a[7]);

    ng_neogeo_set_external_dispatch(NULL);
    ng_neogeo_set_auto_vblank_interval(0);
    ng_neogeo_set_system_rom(NULL, 0);
    ng_neogeo_set_program_rom(NULL, 0);
    free(bios_data);
    ng_program_rom_free(&rom);
    return 0;
}

int ng_generated_smoke_run(const char *neo_path) {
    return ng_generated_smoke_run_with_bios(neo_path, NULL);
}

#ifndef NG_GENERATED_SMOKE_NO_MAIN
static void usage(const char *argv0) {
    fprintf(stderr, "usage: %s [--bios <bios.rom>] <game.neo>\n", argv0);
}

int main(int argc, char **argv) {
    const char *bios_path = NULL;
    const char *neo_path = NULL;

    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--bios") == 0) {
            if (++i >= argc) {
                usage(argv[0]);
                return 2;
            }
            bios_path = argv[i];
        } else if (strcmp(argv[i], "--help") == 0 ||
                   strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else if (!neo_path) {
            neo_path = argv[i];
        } else {
            usage(argv[0]);
            return 2;
        }
    }

    if (!neo_path) {
        usage(argv[0]);
        return 2;
    }
    return ng_generated_smoke_run_with_bios(neo_path, bios_path);
}
#endif
