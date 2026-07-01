#include "c_emitter.h"
#include "function_discovery.h"
#include "p_rom.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NG_BIOS_BASE 0x00C00000u
#define NG_BIOS_DISCOVERY_MAX_CANDIDATES 65536u

static int read_file(const char *path, uint8_t **out_data, uint32_t *out_size) {
    FILE *f = fopen(path, "rb");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", path);
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

static void byteswap_words(uint8_t *data, uint32_t size) {
    for (uint32_t i = 0; i + 1u < size; i += 2u) {
        uint8_t tmp = data[i];
        data[i] = data[i + 1u];
        data[i + 1u] = tmp;
    }
}

static int parse_u32(const char *text, uint32_t *out) {
    if (!text || !out) {
        return 0;
    }
    char *end = NULL;
    unsigned long value = strtoul(text, &end, 0);
    if (end == text || *end != '\0' || value > 0xFFFFFFFFul) {
        return 0;
    }
    *out = (uint32_t)value;
    return 1;
}

static int parse_seed_list(const char *text, uint32_t **out_seeds, uint32_t *out_count) {
    if (!text || !*text || !out_seeds || !out_count) {
        return 0;
    }

    uint32_t capacity = 1u;
    for (const char *p = text; *p; ++p) {
        if (*p == ',') {
            ++capacity;
        }
    }

    uint32_t *seeds = (uint32_t *)calloc(capacity, sizeof(*seeds));
    char *copy = (char *)malloc(strlen(text) + 1u);
    if (!seeds || !copy) {
        free(seeds);
        free(copy);
        return 0;
    }
    strcpy(copy, text);

    uint32_t count = 0;
    for (char *token = strtok(copy, ","); token; token = strtok(NULL, ",")) {
        if (count >= capacity || !parse_u32(token, &seeds[count])) {
            free(seeds);
            free(copy);
            return 0;
        }
        ++count;
    }
    free(copy);

    if (count == 0u) {
        free(seeds);
        return 0;
    }
    *out_seeds = seeds;
    *out_count = count;
    return 1;
}

static void usage(const char *argv0) {
    fprintf(stderr,
            "usage: %s [--no-byteswap] <bios.rom> <entry-addr[,addr...]> <out.c>\n"
            "example: %s ~/.../sp-s2.sp1 0xC00444,0xC004C2 build/bios_recomp.c\n",
            argv0,
            argv0);
}

int main(int argc, char **argv) {
    int argi = 1;
    int byteswap = 1;
    if (argi < argc && strcmp(argv[argi], "--no-byteswap") == 0) {
        byteswap = 0;
        ++argi;
    }
    if (argc - argi != 3) {
        usage(argv[0]);
        return 2;
    }

    const char *bios_path = argv[argi++];
    uint32_t *seeds = NULL;
    uint32_t seed_count = 0;
    if (!parse_seed_list(argv[argi++], &seeds, &seed_count)) {
        usage(argv[0]);
        return 2;
    }
    const char *out_path = argv[argi++];

    uint8_t *bios = NULL;
    uint32_t bios_size = 0;
    if (!read_file(bios_path, &bios, &bios_size)) {
        free(seeds);
        return 1;
    }
    if (byteswap) {
        byteswap_words(bios, bios_size);
    }

    NgProgramRom rom;
    memset(&rom, 0, sizeof(rom));
    rom.size = NG_BIOS_BASE + bios_size;
    rom.data = (uint8_t *)malloc(rom.size ? rom.size : 1u);
    if (!rom.data) {
        free(seeds);
        free(bios);
        return 1;
    }
    memset(rom.data, 0xFF, rom.size);
    memcpy(rom.data + NG_BIOS_BASE, bios, bios_size);
    free(bios);

    NgFunctionDiscovery discovery;
    if (!ng_function_discover_from_game_config_limited(
            &rom,
            seeds,
            seed_count,
            NULL,
            NG_BIOS_DISCOVERY_MAX_CANDIDATES,
            &discovery)) {
        fprintf(stderr, "BIOS discovery failed for %u seed(s)\n", seed_count);
        free(seeds);
        free(rom.data);
        return 1;
    }
    free(seeds);

    fprintf(stderr,
            "BIOS candidates: %u%s\n",
            discovery.count,
            discovery.truncated ? " (truncated)" : "");
    for (uint32_t i = 0; i < discovery.count && i < 32u; ++i) {
        fprintf(stderr, "  [%02u] $%06X\n", i, discovery.addrs[i] & 0x00FFFFFFu);
    }

    FILE *out = fopen(out_path, "w");
    if (!out) {
        fprintf(stderr, "cannot open %s for writing\n", out_path);
        free(rom.data);
        return 1;
    }
    int ok = ng_emit_c(out, &rom, &discovery);
    if (fclose(out) != 0) {
        ok = 0;
    }
    free(rom.data);
    if (!ok) {
        fprintf(stderr, "BIOS C emission failed\n");
        return 1;
    }
    fprintf(stderr, "BIOS C: %s\n", out_path);
    return 0;
}
