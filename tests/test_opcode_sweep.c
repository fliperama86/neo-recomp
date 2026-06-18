#include "c_emitter.h"
#include "m68k_decode.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static void write16(NgProgramRom *rom, uint32_t addr, uint16_t value) {
    rom->data[addr] = (uint8_t)(value >> 8);
    rom->data[addr + 1u] = (uint8_t)value;
}

static int read_file(FILE *f, char *out, size_t out_size) {
    long size;
    rewind(f);
    if (fseek(f, 0, SEEK_END) != 0) {
        return 0;
    }
    size = ftell(f);
    if (size < 0 || (size_t)size >= out_size) {
        return 0;
    }
    rewind(f);
    if (fread(out, 1, (size_t)size, f) != (size_t)size) {
        return 0;
    }
    out[size] = 0;
    return 1;
}

int main(void) {
    enum { ROM_SIZE = 64u };
    char text[32768];
    unsigned recognized = 0;
    unsigned unknown = 0;

    for (uint32_t op = 0; op <= 0xFFFFu; ++op) {
        NgProgramRom rom;
        NgM68kInstr instr;
        NgFunctionDiscovery discovery;
        FILE *out;

        rom.size = ROM_SIZE;
        rom.data = (uint8_t *)calloc(ROM_SIZE, 1);
        CHECK(rom.data != NULL);
        write16(&rom, 0, (uint16_t)op);
        for (uint32_t addr = 2u; addr < ROM_SIZE; addr += 2u) {
            write16(&rom, addr, 0x4E75u); /* extension fill or fall-through RTS */
        }

        CHECK(ng_m68k_decode(&rom, 0, &instr));
        if (instr.mnemonic == NG_M68K_UNKNOWN ||
            instr.mnemonic == NG_M68K_INVALID) {
            ++unknown;
            ng_program_rom_free(&rom);
            continue;
        }
        ++recognized;

        ng_function_discovery_init(&discovery);
        discovery.addrs[discovery.count++] = 0u;

        out = tmpfile();
        CHECK(out != NULL);
        CHECK(ng_emit_c(out, &rom, &discovery));
        CHECK(read_file(out, text, sizeof(text)));
        fclose(out);

        if (strstr(text, "ng_log_dispatch_miss(0x00000000u);") != NULL) {
            char formatted[128];
            ng_m68k_format(&instr, formatted, (unsigned)sizeof(formatted));
            fprintf(stderr,
                    "unsupported emission for opcode $%04X decoded as %s (%s)\n",
                    (unsigned)op,
                    ng_m68k_mnemonic_name(instr.mnemonic),
                    formatted);
            ng_program_rom_free(&rom);
            return 1;
        }

        ng_program_rom_free(&rom);
    }

    CHECK(recognized > 0);
    CHECK(unknown > 0);
    return 0;
}
