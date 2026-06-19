#include "dispatch_audit.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(expr) do { \
    if (!(expr)) { \
        fprintf(stderr, "CHECK failed: %s at %s:%d\n", #expr, __FILE__, __LINE__); \
        return 1; \
    } \
} while (0)

static NgProgramRom make_rom(uint32_t size) {
    NgProgramRom rom;
    rom.size = size;
    rom.data = (uint8_t *)calloc(size ? size : 1u, 1);
    return rom;
}

static void write16(NgProgramRom *rom, uint32_t addr, uint16_t value) {
    rom->data[addr] = (uint8_t)(value >> 8);
    rom->data[addr + 1] = (uint8_t)value;
}

static void write32(NgProgramRom *rom, uint32_t addr, uint32_t value) {
    write16(rom, addr, (uint16_t)(value >> 16));
    write16(rom, addr + 2u, (uint16_t)value);
}

static int read_file(FILE *f, char *buf, size_t buf_size) {
    long size;
    rewind(f);
    if (fseek(f, 0, SEEK_END) != 0) return 0;
    size = ftell(f);
    if (size < 0 || (size_t)size >= buf_size) return 0;
    rewind(f);
    if (fread(buf, 1, (size_t)size, f) != (size_t)size) return 0;
    buf[size] = '\0';
    return 1;
}

int main(void) {
    NgProgramRom rom = make_rom(0xC0u);
    CHECK(rom.data != NULL);

    write16(&rom, 0x10u, 0x4EB9u); /* JSR $000080 */
    write32(&rom, 0x12u, 0x00000080u);
    write16(&rom, 0x16u, 0x207Bu); /* MOVEA.L (4,PC,D0.W),A0 */
    write16(&rom, 0x18u, 0x0004u);
    write16(&rom, 0x1Au, 0x4ED0u); /* JMP (A0) via table at $1C */
    write32(&rom, 0x1Cu, 0x00000090u);
    write32(&rom, 0x20u, 0x000000A0u);
    write32(&rom, 0x24u, 0x00000300u); /* unmapped target */
    write32(&rom, 0x28u, 0x000000B0u);

    write16(&rom, 0x60u, 0x4E91u); /* JSR (A1), computed */
    write16(&rom, 0x62u, 0x4E75u);
    write16(&rom, 0x70u, 0x4EB9u); /* JSR unmapped $000300 */
    write32(&rom, 0x72u, 0x00000300u);
    write16(&rom, 0x76u, 0x4E75u);

    write16(&rom, 0x80u, 0x4E75u);
    write16(&rom, 0x90u, 0x4E75u);
    write16(&rom, 0xA0u, 0x4E75u);
    write16(&rom, 0xB0u, 0x4E75u);

    const uint32_t seeds[] = {0x10u, 0x60u, 0x70u};
    NgFunctionDiscovery discovery;
    CHECK(ng_function_discover_from_seeds(&rom, seeds, 3u, &discovery));

    NgDispatchAudit audit;
    CHECK(ng_dispatch_audit_build(&rom, &discovery, &audit));
    CHECK(audit.direct_count == 2u);
    CHECK(audit.missing_direct_count == 1u);
    CHECK(audit.computed_count == 1u);
    CHECK(audit.jump_table_count == 1u);
    CHECK(audit.jump_table_resolved_entries == 3u);
    CHECK(audit.jump_table_missing_entries == 1u);
    CHECK(!audit.truncated);

    CHECK(audit.count == 4u);
    CHECK(audit.sites[0].kind == NG_DISPATCH_AUDIT_DIRECT);
    CHECK(audit.sites[0].site_addr == 0x10u);
    CHECK(audit.sites[0].target_addr == 0x80u);
    CHECK(audit.sites[0].target_in_discovery);

    CHECK(audit.sites[1].kind == NG_DISPATCH_AUDIT_JUMP_TABLE);
    CHECK(audit.sites[1].site_addr == 0x1Au);
    CHECK(audit.sites[1].table_addr == 0x1Cu);
    CHECK(audit.sites[1].resolved_entries == 3u);
    CHECK(audit.sites[1].missing_entries == 1u);

    CHECK(audit.sites[2].kind == NG_DISPATCH_AUDIT_COMPUTED);
    CHECK(audit.sites[2].site_addr == 0x60u);
    CHECK(!audit.sites[2].target_known);

    CHECK(audit.sites[3].kind == NG_DISPATCH_AUDIT_DIRECT);
    CHECK(audit.sites[3].site_addr == 0x70u);
    CHECK(audit.sites[3].target_addr == 0x300u);
    CHECK(!audit.sites[3].target_in_discovery);

    FILE *out = tmpfile();
    char text[2048];
    CHECK(out != NULL);
    CHECK(ng_dispatch_audit_write(out, &audit));
    CHECK(read_file(out, text, sizeof(text)));
    fclose(out);
    CHECK(strstr(text, "dispatch audit: sites=4 direct=2 missing_direct=1 computed=1 jump_tables=1") != NULL);
    CHECK(strstr(text, "$000010 DIRECT JSR target=$000080 discovered=yes") != NULL);
    CHECK(strstr(text, "$00001A JUMP_TABLE JMP table=$00001C resolved=3 missing=1") != NULL);
    CHECK(strstr(text, "$000060 COMPUTED JSR target=<runtime>") != NULL);
    CHECK(strstr(text, "$000070 DIRECT JSR target=$000300 discovered=no") != NULL);

    ng_program_rom_free(&rom);
    return 0;
}
