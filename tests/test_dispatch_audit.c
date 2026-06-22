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
    memset(&rom, 0, sizeof(rom));
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
    write16(&rom, 0x68u, 0x4EBBu); /* JSR (4,PC,D0.W), computed */
    write16(&rom, 0x6Au, 0x0004u);
    write16(&rom, 0x6Cu, 0x4E75u);
    write16(&rom, 0x70u, 0x4EB9u); /* JSR unmapped $000300 */
    write32(&rom, 0x72u, 0x00000300u);
    write16(&rom, 0x76u, 0x4E75u);
    write16(&rom, 0x78u, 0x4EB9u); /* JSR BIOS/system ROM target */
    write32(&rom, 0x7Au, 0x00C00444u);
    write16(&rom, 0x7Eu, 0x4E75u);

    write16(&rom, 0x80u, 0x4E75u);
    write16(&rom, 0x90u, 0x4E75u);
    write16(&rom, 0xA0u, 0x4E75u);
    write16(&rom, 0xB0u, 0x4E75u);

    const uint32_t seeds[] = {0x10u, 0x60u, 0x68u, 0x70u, 0x78u};
    static NgFunctionDiscovery discovery;
    CHECK(ng_function_discover_from_seeds(&rom, seeds, 5u, &discovery));

    static NgDispatchAudit audit;
    CHECK(ng_dispatch_audit_build(&rom, &discovery, &audit));
    CHECK(audit.direct_count == 3u);
    CHECK(audit.missing_direct_count == 1u);
    CHECK(audit.external_direct_count == 1u);
    CHECK(audit.computed_count == 2u);
    CHECK(audit.runtime_computed_count == 0u);
    CHECK(audit.jump_table_count == 1u);
    CHECK(audit.jump_table_resolved_entries == 3u);
    CHECK(audit.jump_table_missing_entries == 1u);
    CHECK(ng_dispatch_audit_has_gaps(&audit));
    CHECK(!audit.truncated);

    CHECK(audit.count == 6u);
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

    CHECK(audit.sites[3].kind == NG_DISPATCH_AUDIT_COMPUTED);
    CHECK(audit.sites[3].site_addr == 0x68u);
    CHECK(!audit.sites[3].target_known);

    CHECK(audit.sites[4].kind == NG_DISPATCH_AUDIT_DIRECT);
    CHECK(audit.sites[4].site_addr == 0x70u);
    CHECK(audit.sites[4].target_addr == 0x300u);
    CHECK(!audit.sites[4].target_in_discovery);
    CHECK(!audit.sites[4].target_external);

    CHECK(audit.sites[5].kind == NG_DISPATCH_AUDIT_DIRECT);
    CHECK(audit.sites[5].site_addr == 0x78u);
    CHECK(audit.sites[5].target_addr == 0xC00444u);
    CHECK(!audit.sites[5].target_in_discovery);
    CHECK(audit.sites[5].target_external);

    FILE *out = tmpfile();
    char text[2048];
    CHECK(out != NULL);
    CHECK(ng_dispatch_audit_write(out, &audit));
    CHECK(read_file(out, text, sizeof(text)));
    fclose(out);
    CHECK(strstr(text, "dispatch audit: sites=6 direct=3 missing_direct=1 external_direct=1 computed=2 runtime_computed=0 jump_tables=1") != NULL);
    CHECK(strstr(text, "$000010 DIRECT JSR target=$000080 discovered=yes") != NULL);
    CHECK(strstr(text, "$00001A JUMP_TABLE JMP table=$00001C resolved=3 missing=1") != NULL);
    CHECK(strstr(text, "$000060 COMPUTED JSR target=<runtime>") != NULL);
    CHECK(strstr(text, "$000068 COMPUTED JSR target=<runtime>") != NULL);
    CHECK(strstr(text, "$000070 DIRECT JSR target=$000300 discovered=no") != NULL);
    CHECK(strstr(text, "$000078 DIRECT JSR target=$C00444 discovered=no external=yes") != NULL);

    audit.missing_direct_count = 0u;
    audit.computed_count = 0u;
    audit.jump_table_missing_entries = 0u;
    CHECK(!ng_dispatch_audit_has_gaps(&audit));

    NgGameConfig config;
    ng_game_config_init(&config);
    config.runtime_dispatch[config.runtime_dispatch_count++] = 0x60u;
    CHECK(ng_dispatch_audit_build_with_config(&rom, &discovery, &config, &audit));
    CHECK(audit.computed_count == 1u);
    CHECK(audit.runtime_computed_count == 1u);
    CHECK(audit.sites[2].kind == NG_DISPATCH_AUDIT_COMPUTED);
    CHECK(audit.sites[2].site_addr == 0x60u);
    CHECK(audit.sites[2].runtime_allowed);
    CHECK(audit.sites[3].kind == NG_DISPATCH_AUDIT_COMPUTED);
    CHECK(audit.sites[3].site_addr == 0x68u);
    CHECK(!audit.sites[3].runtime_allowed);

    out = tmpfile();
    CHECK(out != NULL);
    CHECK(ng_dispatch_audit_write(out, &audit));
    CHECK(read_file(out, text, sizeof(text)));
    fclose(out);
    CHECK(strstr(text, "computed=1 runtime_computed=1") != NULL);
    CHECK(strstr(text, "$000060 COMPUTED JSR target=<runtime> allowed=yes") != NULL);
    CHECK(strstr(text, "$000068 COMPUTED JSR target=<runtime>\n") != NULL);

    ng_program_rom_free(&rom);

    NgProgramRom stop_rom = make_rom(0x80u);
    CHECK(stop_rom.data != NULL);

    write16(&stop_rom, 0x10u, 0x42C0u); /* UNKNOWN; do not scan into $12 */
    write16(&stop_rom, 0x12u, 0x4EB9u);
    write32(&stop_rom, 0x14u, 0x00000300u);

    write16(&stop_rom, 0x20u, 0x4AFCu); /* ILLEGAL; do not scan into $22 */
    write16(&stop_rom, 0x22u, 0x4EB9u);
    write32(&stop_rom, 0x24u, 0x00000300u);

    write16(&stop_rom, 0x30u, 0x4E41u); /* TRAP #1; do not scan into $32 */
    write16(&stop_rom, 0x32u, 0x4EB9u);
    write32(&stop_rom, 0x34u, 0x00000300u);

    write16(&stop_rom, 0x40u, 0x4E73u); /* RTE; do not scan into $42 */
    write16(&stop_rom, 0x42u, 0x4EB9u);
    write32(&stop_rom, 0x44u, 0x00000300u);

    write16(&stop_rom, 0x50u, 0x4E77u); /* RTR; do not scan into $52 */
    write16(&stop_rom, 0x52u, 0x4EB9u);
    write32(&stop_rom, 0x54u, 0x00000300u);

    write16(&stop_rom, 0x60u, 0x6006u); /* BRA $68; do not scan into $62 */
    write16(&stop_rom, 0x62u, 0x4EB9u);
    write32(&stop_rom, 0x64u, 0x00000300u);
    write16(&stop_rom, 0x68u, 0x4E75u);

    static NgFunctionDiscovery stop_discovery;
    ng_function_discovery_init(&stop_discovery);
    CHECK(ng_function_discovery_add(&stop_discovery, &stop_rom, 0x10u));
    CHECK(ng_function_discovery_add(&stop_discovery, &stop_rom, 0x20u));
    CHECK(ng_function_discovery_add(&stop_discovery, &stop_rom, 0x30u));
    CHECK(ng_function_discovery_add(&stop_discovery, &stop_rom, 0x40u));
    CHECK(ng_function_discovery_add(&stop_discovery, &stop_rom, 0x50u));
    CHECK(ng_function_discovery_add(&stop_discovery, &stop_rom, 0x60u));

    CHECK(ng_dispatch_audit_build(&stop_rom, &stop_discovery, &audit));
    CHECK(audit.count == 0u);
    CHECK(audit.direct_count == 0u);
    CHECK(audit.missing_direct_count == 0u);
    CHECK(audit.computed_count == 0u);
    CHECK(audit.jump_table_count == 0u);
    CHECK(!ng_dispatch_audit_has_gaps(&audit));
    CHECK(!audit.truncated);

    ng_program_rom_free(&stop_rom);

    {
        const uint32_t seed_count = 1500u;
        NgProgramRom many_rom = make_rom(seed_count * 2u);
        uint32_t seeds[1500];
        CHECK(many_rom.data != NULL);

        for (uint32_t i = 0; i < seed_count; ++i) {
            uint32_t addr = i * 2u;
            write16(&many_rom, addr, 0x4E75u); /* RTS */
            seeds[i] = addr;
        }

        static NgFunctionDiscovery many_discovery;
        CHECK(ng_function_discover_from_seeds(&many_rom,
                                              seeds,
                                              seed_count,
                                              &many_discovery));
        CHECK(many_discovery.count == seed_count);
        CHECK(!many_discovery.truncated);
        CHECK(ng_dispatch_audit_build(&many_rom, &many_discovery, &audit));
        CHECK(audit.count == 0u);
        CHECK(!audit.truncated);
        CHECK(!ng_dispatch_audit_has_gaps(&audit));

        ng_program_rom_free(&many_rom);
    }

    {
        const uint32_t dispatch_count = 8300u;
        NgProgramRom many_dispatch_rom = make_rom(dispatch_count * 8u);
        static NgFunctionDiscovery many_dispatch_discovery;
        CHECK(many_dispatch_rom.data != NULL);

        ng_function_discovery_init(&many_dispatch_discovery);
        for (uint32_t i = 0; i < dispatch_count; ++i) {
            uint32_t addr = i * 8u;
            write16(&many_dispatch_rom, addr, 0x4EB9u); /* JSR $000000 */
            write32(&many_dispatch_rom, addr + 2u, 0x00000000u);
            write16(&many_dispatch_rom, addr + 6u, 0x4E75u); /* RTS */
            many_dispatch_discovery.addrs[many_dispatch_discovery.count++] = addr;
        }

        CHECK(ng_dispatch_audit_build(&many_dispatch_rom,
                                      &many_dispatch_discovery,
                                      &audit));
        CHECK(audit.count == dispatch_count);
        CHECK(audit.direct_count == dispatch_count);
        CHECK(!audit.truncated);

        ng_program_rom_free(&many_dispatch_rom);
    }

    return 0;
}
